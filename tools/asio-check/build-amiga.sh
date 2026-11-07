#!/bin/sh
# GFOpenBoost: a quick lab build for Asio work. Takes a published SDK
# (TAG), puts this checkout's Asio headers and patch 0002 over it, builds
# libboost_asio again and links boosttest against it, so a lab can try an
# Asio change without the full build. The other parts don't include Asio.
#   STOVE=.../stoves/os32-gcc16-open6 OUT=out tools/asio-check/build-amiga.sh
# Copyright (c) 2026 AmiCradle Team
# Distributed under the Boost Software License, Version 1.0.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TAG=${TAG:-v1.89.0-rc1}
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
patch -s -d "$INC" -p1 --no-backup-if-mismatch < "$HERE/patches/headers/0002-asio-amiga-event-reactor.patch"
printf '#include <boost/asio/impl/src.hpp>\n' > "$W/asio_src.cpp"
XI=""; for d in ${EXTRA_INC:-}; do XI="$XI -idirafter $d"; done
CXXF="$CPU -std=gnu++17 -D_DEFAULT_SOURCE -DBOOST_ALL_NO_LIB -I$INC $XI"
m68k-amigaos-g++ $CXXF -DBOOST_ASIO_SEPARATE_COMPILATION -c "$W/asio_src.cpp" -o "$W/asio_src.o"
for c in amiga_net amiga_select amiga_event; do
    m68k-amigaos-gcc $CPU -std=gnu99 -D_DEFAULT_SOURCE -I"$INC" -I"$HERE/compat/include" $XI -c "$HERE/compat/src/$c.c" -o "$W/$c.o"
done
rm -f "$LIB/libboost_asio.a"
m68k-amigaos-ar rcs "$LIB/libboost_asio.a" "$W/asio_src.o" "$W/amiga_net.o" "$W/amiga_select.o" "$W/amiga_event.o"
m68k-amigaos-g++ $CXXF -DBOOST_ASIO_SEPARATE_COMPILATION -o "$OUT/boosttest" "$HERE/tests/boosttest.cpp" -L"$LIB" \
    -lboost_program_options -lboost_json -lboost_url -lboost_locale -lboost_filesystem -lboost_iostreams \
    -lboost_serialization -lboost_regex -lboost_timer -lboost_chrono -lboost_thread -lboost_date_time \
    -lboost_container -lboost_charconv -lboost_random -lboost_asio -lboost_atomic \
    -lpthread -lsocket -latomic 2> "$W/link.log" || { grep -E 'undefined|error' "$W/link.log" | sort -u | head -40; exit 1; }
cp "$LIB/libboost_asio.a" "$OUT/"
# the library the reactor waits through, for the lab's LIBS:
EV=${EVENT_TAG:-v1.1-rc2}
curl -fsSL -o "$W/ev.zip" "https://github.com/DalsinAI/gfopenevent/releases/download/$EV/GFOpenEvent-${EV#v}.zip"
(cd "$W" && unzip -q ev.zip) && cp "$W/GFOpenEvent/Libs/event.library" "$OUT/"
echo "asio-check: $TAG + $(cd "$HERE" && git rev-parse --short HEAD 2>/dev/null || echo tree)"
ls -l "$OUT"
