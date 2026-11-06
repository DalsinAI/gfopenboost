#!/bin/sh
# Run by build-amiga.sh (which sets HERE TC WORK OUT JOBS BUDGET EXTRA_INC t0
# and the boost.lock values). Exit 0 when done or stopped at the time cap
# (the log says which), 1 on a failure.
# Copyright (c) 2026 AmiCradle Team
# Distributed under the Boost Software License, Version 1.0.
set -eu
CXX=m68k-amigaos-g++; CC=m68k-amigaos-gcc; AR=m68k-amigaos-ar
V=$BOOST_VERSION
B="$WORK/src/boost-$V"; SDK="$WORK/sdk"; INC="$SDK/include"; LIB="$SDK/lib"; OBJ="$WORK/obj"
mkdir -p "$WORK/src" "$INC" "$LIB" "$OBJ"
left() { echo $(( BUDGET - ($(date +%s) - t0) )); }
log() { echo "boost: $*"; }

# 68040, soft float (the stoves' specs add the FPU check at link time)
CPU="-noixemul -m68040 -O2 -fno-delete-null-pointer-checks"
INCF="-I$INC"
for d in $EXTRA_INC; do INCF="$INCF -idirafter $d"; done
ZINC=""
for d in "$TC/m68k-amigaos/port-deps/include" $EXTRA_INC; do
    [ -f "$d/zlib.h" ] && { ZINC="-idirafter $d"; break; }
done
CXXF="$CPU -std=gnu++17 -D_DEFAULT_SOURCE -DBOOST_ALL_NO_LIB $INCF"
CF="$CPU -std=gnu99 -D_DEFAULT_SOURCE $INCF"

# ---- headers: the release, our config over it, our edits
hkey=$( (echo "$BOOST_SHA256"; cd "$HERE" && cat $(find config patches -type f | sort)) | sha256sum | cut -c1-16)
if [ "$(cat "$WORK/.headers" 2>/dev/null)" != "$hkey" ]; then
    T="$WORK/dl/boost-$V.tar.xz"
    if [ ! -f "$T" ] || ! echo "$BOOST_SHA256  $T" | sha256sum -c - >/dev/null 2>&1; then
        curl -fsSL -o "$T.part" "$BOOST_URL"
        echo "$BOOST_SHA256  $T.part" | sha256sum -c - >/dev/null || { log "sha256 mismatch: $BOOST_URL"; exit 1; }
        mv "$T.part" "$T"
    fi
    rm -rf "$B"; tar -C "$WORK/src" -xJf "$T"
    rm -rf "$INC/boost"; cp -R "$B/boost" "$INC/boost"
    cp -R "$HERE/config/boost/." "$INC/boost/"
    for p in "$HERE"/patches/headers/*.patch; do
        patch -s -d "$INC" -p1 --no-backup-if-mismatch < "$p" || { log "patch did not apply: $(basename "$p")"; exit 1; }
    done
    for p in "$HERE"/patches/src/*.patch; do
        patch -s -d "$B" -p1 --no-backup-if-mismatch < "$p" || { log "patch did not apply: $(basename "$p")"; exit 1; }
    done
    cp "$B/LICENSE_1_0.txt" "$SDK/LICENSE_1_0.txt"
    # new headers: every object again
    rm -rf "$OBJ"; mkdir -p "$OBJ"; rm -f "$LIB"/libboost_*.a
    echo "$hkey" > "$WORK/.headers"
    log "headers $V ($hkey), $(( $(date +%s) - t0 )) s"
fi

# ---- the compiled parts
# part | defines | source files under libs/<lib>/src (lib = part before any _)
PARTS='
atomic|-DBOOST_ATOMIC_SOURCE -DBOOST_ATOMIC_STATIC_LINK|lock_pool.cpp
charconv|-DBOOST_CHARCONV_SOURCE -DBOOST_CHARCONV_STATIC_LINK|from_chars.cpp to_chars.cpp
chrono|-DBOOST_CHRONO_STATIC_LINK|chrono.cpp thread_clock.cpp process_cpu_clocks.cpp @amiga_clock.cpp
container|-DBOOST_CONTAINER_SOURCE -DBOOST_CONTAINER_STATIC_LINK|alloc_lib.c dlmalloc.cpp global_resource.cpp monotonic_buffer_resource.cpp pool_resource.cpp synchronized_pool_resource.cpp unsynchronized_pool_resource.cpp
date_time|-DBOOST_DATE_TIME_SOURCE -DBOOST_DATE_TIME_STATIC_LINK|gregorian/greg_month.cpp
filesystem|-DBOOST_FILESYSTEM_SOURCE -DBOOST_FILESYSTEM_STATIC_LINK -DBOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF -DO_DIRECTORY=0|codecvt_error_category.cpp directory.cpp exception.cpp operations.cpp path.cpp path_traits.cpp portability.cpp unique_path.cpp utf8_codecvt_facet.cpp @amiga_statvfs.c @amiga_statvfs_cxx.cpp @amiga_fchmod.c
iostreams|-DBOOST_IOSTREAMS_SOURCE|file_descriptor.cpp zlib.cpp gzip.cpp
json|-DBOOST_JSON_SOURCE -DBOOST_JSON_STATIC_LINK|src.cpp
locale|-DBOOST_LOCALE_SOURCE -DBOOST_LOCALE_STATIC_LINK -DBOOST_LOCALE_NO_POSIX_BACKEND -DBOOST_LOCALE_NO_WINAPI_BACKEND -DBOOST_THREAD_USE_LIB|encoding/codepage.cpp shared/date_time.cpp shared/format.cpp shared/formatting.cpp shared/generator.cpp shared/ids.cpp shared/localization_backend.cpp shared/message.cpp shared/mo_lambda.cpp std/codecvt.cpp std/collate.cpp std/converter.cpp std/numeric.cpp std/std_backend.cpp util/codecvt_converter.cpp util/default_locale.cpp util/encoding.cpp util/gregorian.cpp util/info.cpp util/locale_data.cpp
program_options|-DBOOST_PROGRAM_OPTIONS_SOURCE -DBOOST_PROGRAM_OPTIONS_STATIC_LINK|cmdline.cpp config_file.cpp convert.cpp options_description.cpp parsers.cpp positional_options.cpp split.cpp utf8_codecvt_facet.cpp value_semantic.cpp variables_map.cpp
random|-DBOOST_RANDOM_SOURCE -DBOOST_RANDOM_STATIC_LINK|random_device.cpp
regex|-DBOOST_REGEX_SOURCE|posix_api.cpp wide_posix_api.cpp
serialization|-DBOOST_SERIALIZATION_SOURCE -DBOOST_SERIALIZATION_STATIC_LINK|archive_exception.cpp basic_archive.cpp basic_iarchive.cpp basic_iserializer.cpp basic_oarchive.cpp basic_oserializer.cpp basic_pointer_iserializer.cpp basic_pointer_oserializer.cpp basic_serializer_map.cpp basic_text_iprimitive.cpp basic_text_oprimitive.cpp basic_xml_archive.cpp binary_iarchive.cpp binary_oarchive.cpp codecvt_null.cpp extended_type_info.cpp extended_type_info_no_rtti.cpp extended_type_info_typeid.cpp polymorphic_binary_iarchive.cpp polymorphic_binary_oarchive.cpp polymorphic_iarchive.cpp polymorphic_oarchive.cpp polymorphic_text_iarchive.cpp polymorphic_text_oarchive.cpp polymorphic_xml_iarchive.cpp polymorphic_xml_oarchive.cpp stl_port.cpp text_iarchive.cpp text_oarchive.cpp utf8_codecvt_facet.cpp void_cast.cpp xml_archive_exception.cpp xml_grammar.cpp xml_iarchive.cpp xml_oarchive.cpp
wserialization|-DBOOST_WSERIALIZATION_SOURCE -DBOOST_SERIALIZATION_STATIC_LINK -DBOOST_WSERIALIZATION_STATIC_LINK|basic_text_wiprimitive.cpp basic_text_woprimitive.cpp binary_wiarchive.cpp binary_woarchive.cpp polymorphic_text_wiarchive.cpp polymorphic_text_woarchive.cpp polymorphic_xml_wiarchive.cpp polymorphic_xml_woarchive.cpp text_wiarchive.cpp text_woarchive.cpp xml_wgrammar.cpp xml_wiarchive.cpp xml_woarchive.cpp
stacktrace_noop|-DBOOST_STACKTRACE_INTERNAL_BUILD_LIBS -DBOOST_STACKTRACE_LINK|noop.cpp
thread|-DBOOST_THREAD_BUILD_LIB -DBOOST_THREAD_POSIX|pthread/thread.cpp pthread/once.cpp future.cpp
timer|-DBOOST_TIMER_SOURCE -DBOOST_TIMER_STATIC_LINK|cpu_timer.cpp auto_timers_construction.cpp
url|-DBOOST_URL_SOURCE -DBOOST_URL_STATIC_LINK|@url
asio|-DBOOST_ASIO_SEPARATE_COMPILATION|@asio_src.cpp @amiga_net.c
system||@empty.cpp
'
# where a part's sources live
libdir() { case "$1" in wserialization) echo serialization ;; stacktrace_noop) echo stacktrace ;; *) echo "$1" ;; esac; }

# generated sources (@name)
G="$WORK/gen"; mkdir -p "$G"
printf '// Boost.Asio, separately compiled (programs define BOOST_ASIO_SEPARATE_COMPILATION)\n#include <boost/asio/impl/src.hpp>\n' > "$G/asio_src.cpp"
printf '// Boost.System is header-only; this empty library keeps -lboost_system links working\nnamespace boost { namespace gfopenboost { int system_is_header_only = 1; } }\n' > "$G/empty.cpp"
cp "$HERE"/compat/src/*.c "$HERE"/compat/src/*.cpp "$G/"

# PART -> its source paths (absolute)
sources() {
    files=$(echo "$PARTS" | awk -F'|' -v p="$1" '$1==p {print $3}')
    l=$(libdir "$1")
    for f in $files; do
        case "$f" in
            @url) (cd "$B/libs/url/src" && find . -name '*.cpp' | sed 's|^\./||' | sort) | sed "s|^|$B/libs/url/src/|" ;;
            @*) echo "$G/${f#@}" ;;
            *) echo "$B/libs/$l/src/$f" ;;
        esac
    done
}

failed=""
for part in $(echo "$PARTS" | awk -F'|' 'NF>1 {print $1}'); do
    a="$LIB/libboost_$part.a"
    [ -f "$a" ] && continue
    [ "$(left)" -gt 180 ] || { log "time cap: $part and later go on next run"; exit 0; }
    defs=$(echo "$PARTS" | awk -F'|' -v p="$part" '$1==p {print $2}')
    l=$(libdir "$part")
    case "$part" in iostreams) [ -n "$ZINC" ] || { log "iostreams: no zlib.h, built without zlib/gzip"; } ;; esac
    od="$OBJ/$part"; mkdir -p "$od"
    srcs=$(sources "$part")
    [ "$part" = iostreams ] && [ -z "$ZINC" ] && srcs=$(echo "$srcs" | grep -v 'zlib.cpp\|gzip.cpp')
    for s in $srcs; do
        o="$od/$(echo "${s#$B/libs/}" | tr / _ | sed 's/\.c[p]*$/.o/')"
        [ -f "$o" ] && continue
        case "$s" in
            *.c) set -- $CC $CF ;;
            *) set -- $CXX $CXXF $ZINC ;;
        esac
        ( ulimit -v ${CC_MEM_KB:-2500000} 2>/dev/null || true
          "$@" $defs -I"$B/libs/$l/src" -I"$B/libs/atomic/src" -c "$s" -o "$o.part" 2> "$o.err" && mv "$o.part" "$o" ) &
        while [ "$(jobs -p | wc -l)" -ge "$JOBS" ]; do sleep 1; done
        [ "$(left)" -gt 150 ] || break
    done
    wait
    n=0; miss=""
    for s in $srcs; do
        o="$od/$(echo "${s#$B/libs/}" | tr / _ | sed 's/\.c[p]*$/.o/')"
        [ -f "$o" ] || { n=$((n+1)); miss="$miss $o"; }
    done
    if [ "$n" -ne 0 ]; then
        errs=$(for o in $miss; do [ -s "$o.err" ] && echo "$o"; done)
        if [ -z "$errs" ]; then log "time cap inside $part ($n files left), goes on next run"; exit 0; fi
        for o in $errs; do
            echo "FAIL $part $(basename "$o" .o)"
            grep -m12 -E 'error|Error' "$o.err" | cut -c1-400 | sed 's/^/ERR /'
        done
        failed="$failed $part"; continue
    fi
    rm -f "$a"; $AR rcs "$a" "$od"/*.o
    log "$part: $(echo "$srcs" | wc -w) objects, $(wc -c < "$a") bytes"
done
[ -z "$failed" ] || { log "failed:$failed"; exit 1; }

# ---- CMake package
sh "$HERE/scripts/write-cmake.sh" "$SDK" "$V" "$(echo "$PARTS" | awk -F'|' 'NF>1 {print $1}' | tr '\n' ' ')"

# ---- link check: one program with every part, run in a lab
if [ ! -f "$OUT/boosttest" ] || [ "$HERE/tests/boosttest.cpp" -nt "$OUT/boosttest" ]; then
    [ "$(left)" -gt 90 ] || { log "time cap: test next run"; exit 0; }
    $CXX $CXXF -DBOOST_ASIO_SEPARATE_COMPILATION -o "$OUT/boosttest" "$HERE/tests/boosttest.cpp" -L"$LIB" \
        -lboost_program_options -lboost_json -lboost_url -lboost_locale -lboost_filesystem -lboost_iostreams \
        -lboost_serialization -lboost_regex -lboost_timer -lboost_chrono -lboost_thread -lboost_date_time \
        -lboost_container -lboost_charconv -lboost_random -lboost_asio -lboost_atomic \
        -lpthread -lsocket -latomic ${TEST_LIBS:-} > "$WORK/test-link.log" 2>&1 \
        || { log "test link failed"; grep -E 'undefined|error' "$WORK/test-link.log" | sort | uniq | head -40 | cut -c1-300; exit 1; }
    log "boosttest: $(wc -c < "$OUT/boosttest") bytes"
fi

# ---- what was built, and the SDK packed
( echo "GFOpenBoost: Boost $V for AmigaOS 3 (68040, soft float)"
  echo "headers: $hkey"
  cd "$LIB" && ls -l libboost_*.a | awk '{print $5, $9}' ) > "$SDK/boost.txt"
cp "$SDK/boost.txt" "$OUT/boost.txt"
P="$OUT/gfopenboost-$V-sdk.tar.xz"
if [ ! -f "$P" ] || [ "$(cat "$WORK/.packed" 2>/dev/null)" != "$(sha256sum "$SDK/boost.txt" | cut -c1-16)" ]; then
    [ "$(left)" -gt 240 ] || { log "time cap: packing next run"; exit 0; }
    tar -C "$WORK" --transform "s|^sdk|gfopenboost-$V|" -cf - sdk | xz -6 -T${JOBS} > "$P.part"
    mv "$P.part" "$P"; sha256sum "$SDK/boost.txt" | cut -c1-16 > "$WORK/.packed"
    (cd "$OUT" && sha256sum "$(basename "$P")" > "$(basename "$P").sha256")
fi
log "done, $(( $(date +%s) - t0 )) s: $(ls -l "$P" | awk '{print $5}') bytes packed"
