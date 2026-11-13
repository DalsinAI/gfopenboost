#!/bin/sh
# GFOpenBoost: a quick lab build for Asio work. Takes a published SDK
# (TAG), puts this checkout's Asio headers and the header patches after
# 0001 (which the SDK has) over it, builds
# libboost_asio again and links boosttest against it, so a lab can try an
# Asio change without the full build. The other parts don't include Asio.
# Also links UDPLT (tests/udplt.cpp), the UDP checks GFOpenTorrent needs.
#   STOVE=.../stoves/os32-gcc16-open6 OUT=out tools/asio-check/build-amiga.sh
# Copyright (c) 2026 AmiCradle Team
# Distributed under the Boost Software License, Version 1.0.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TAG=${TAG:-v1.89.0-rc3}
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
TC=${TOOLCHAIN:-"$STOVE/prefix"}
OUT=${OUT:-out}
W=$(mktemp -d "${TMPDIR:-/var/tmp}/gfob-asio.XXXXXX")
trap 'rm -rf "$W"' EXIT
PATH="$TC/bin:$PATH"
CPU="-noixemul -m68040 -O2 -fno-delete-null-pointer-checks"
mkdir -p "$OUT"
curl -fsSL "https://github.com/DalsinAI/gfopenboost/releases/download/$TAG/gfopenboost-1.89.0-sdk.tar.xz" | tar -C "$W" -xJf -
S="$W/gfopenboost-1.89.0"; INC="$S/include"; LIB="$S/lib"
cp -R "$HERE/config/boost/." "$INC/boost/"
for p in "$HERE"/patches/headers/*.patch; do
    case "$(basename "$p")" in 0001-*) continue ;; esac
    # a release SDK may have the patch already (rc3 has 0002-0004)
    if patch -s -R --dry-run -d "$INC" -p1 < "$p" >/dev/null 2>&1; then continue; fi
    patch -s -d "$INC" -p1 --no-backup-if-mismatch < "$p"
done
printf '#include <boost/asio/impl/src.hpp>\n' > "$W/asio_src.cpp"
XI=""; for d in ${EXTRA_INC:-}; do XI="$XI -idirafter $d"; done
CXXF="$CPU -std=gnu++17 -D_DEFAULT_SOURCE -DBOOST_ALL_NO_LIB -I$INC $XI"
m68k-amigaos-g++ $CXXF -DBOOST_ASIO_SEPARATE_COMPILATION -c "$W/asio_src.cpp" -o "$W/asio_src.o"
for c in amiga_net amiga_select amiga_event amiga_ioctl; do
    m68k-amigaos-gcc $CPU -std=gnu99 -D_DEFAULT_SOURCE -I"$INC" -I"$HERE/compat/include" $XI -c "$HERE/compat/src/$c.c" -o "$W/$c.o"
done
rm -f "$LIB/libboost_asio.a"
m68k-amigaos-ar rcs "$LIB/libboost_asio.a" "$W/asio_src.o" "$W/amiga_net.o" "$W/amiga_select.o" "$W/amiga_event.o" "$W/amiga_ioctl.o"
m68k-amigaos-g++ $CXXF -DBOOST_ASIO_SEPARATE_COMPILATION -o "$OUT/boosttest" "$HERE/tests/boosttest.cpp" -L"$LIB" \
    -lboost_program_options -lboost_json -lboost_url -lboost_locale -lboost_filesystem -lboost_iostreams \
    -lboost_serialization -lboost_regex -lboost_timer -lboost_chrono -lboost_thread -lboost_date_time \
    -lboost_container -lboost_charconv -lboost_random -lboost_asio -lboost_atomic \
    -lpthread -lsocket -latomic 2> "$W/link.log" || { grep -E 'undefined|error' "$W/link.log" | sort -u | head -40; exit 1; }
cp "$LIB/libboost_asio.a" "$OUT/"
# UDPLT: UDP as libtorrent uses it (async_wait + drain, multicast, SIOCGIFCONF)
m68k-amigaos-g++ $CXXF -DBOOST_ASIO_SEPARATE_COMPILATION -o "$OUT/UDPLT" "$HERE/tests/udplt.cpp" -L"$LIB" \
    -lboost_asio -lpthread -lsocket -latomic 2> "$W/udplt.log" || { grep -E 'undefined|error' "$W/udplt.log" | sort -u | head -40; exit 1; }
# SockCtl: ioctl()/fcntl() on sockets with amiga_ioctl.c; SockCtl-libnix:
# the same with libnix's own (select() is amiga_select.c in both)
m68k-amigaos-gcc $CPU -std=gnu99 -D_DEFAULT_SOURCE -o "$OUT/SockCtl" "$HERE/tests/sockctl.c" \
    "$W/amiga_ioctl.o" "$W/amiga_select.o" -lsocket
m68k-amigaos-gcc $CPU -std=gnu99 -D_DEFAULT_SOURCE -o "$OUT/SockCtl-libnix" "$HERE/tests/sockctl.c" \
    "$W/amiga_select.o" -lsocket
# the library the reactor waits through, for the lab's LIBS:
EV=${EVENT_TAG:-v1.1-rc2}
# v1.1-rc2's zip, as gfamigaamule's amule.lock pins it; another EVENT_TAG
# needs its EVENT_SHA256
case "$EV" in v1.1-rc2) EVS=${EVENT_SHA256:-3bbb04429ecc850982b6cbcaf9ba629a06d914b6b951d6b69cd66c33c36c8ca7} ;;
              *) EVS=${EVENT_SHA256:?EVENT_SHA256 for $EV} ;; esac
curl -fsSL -o "$W/ev.zip" "https://github.com/DalsinAI/gfopenevent/releases/download/$EV/GFOpenEvent-${EV#v}.zip"
echo "$EVS  $W/ev.zip" | sha256sum -c - >/dev/null || { echo "GFOpenEvent $EV: zip checksum differs"; exit 1; }
(cd "$W" && unzip -q ev.zip) && cp "$W/GFOpenEvent/Libs/event.library" "$OUT/"
echo "asio-check: $TAG + $(cd "$HERE" && git rev-parse --short HEAD 2>/dev/null || echo tree)"
ls -l "$OUT"
