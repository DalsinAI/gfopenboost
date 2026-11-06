#!/bin/sh
# GFOpenBoost: Boost for AmigaOS 3 (68040, soft float), built with a stove.
#   STOVE=.../stoves/os32-gcc16-open6 OUT=out ./build-amiga.sh
# Fetches the Boost release boost.lock pins, puts in our one Amiga config
# (config/) and edits (patches/), and builds Boost's separately compiled
# parts as static link libraries. The SDK it makes, in $WORK/sdk and
# packed into $OUT/gfopenboost-<version>-sdk.tar.xz:
#   include/boost/...                 the headers, with the Amiga config
#   lib/libboost_<part>.a             one link library a part
#   lib/cmake/Boost-<version>/        a CMake package (Boost::<part> targets)
#   boost.txt                         what was built, with sizes
# The work tree lives outside the checkout (Cradle cleans the checkout before
# every job), so a run stopped at the time cap goes on from its objects.
# Copyright (c) 2026 AmiCradle Team
# Distributed under the Boost Software License, Version 1.0.
set -eu
renice -n 19 $$ >/dev/null 2>&1 || true
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/boost.lock"
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
TC=${TOOLCHAIN:-"$STOVE/prefix"}
WORK=${WORK:-"$HOME/.cache/gfopenboost/build"}
OUT=${OUT:-"$WORK/out"}
JOBS=${JOBS:-4}
BUDGET=${BUDGET_TOTAL:-840}
# extra include folders after the toolchain's own (the cloud check uses
# OpenNDK's headers; a stove has its NDK already)
EXTRA_INC=${EXTRA_INC:-}
PATH="$TC/bin:$PATH"
t0=$(date +%s)
mkdir -p "$WORK/dl" "$OUT"
export HERE TC WORK OUT JOBS BUDGET EXTRA_INC t0 BOOST_VERSION BOOST_URL BOOST_SHA256
exec sh "$HERE/scripts/build-boost.sh"
