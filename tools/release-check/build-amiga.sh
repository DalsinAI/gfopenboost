#!/bin/sh
# GFOpenBoost: fetch a published release's boosttest (and SDK checksum) into
# $OUT so a lab can run the exact released binary, not a fresh build.
#   OUT=out TAG=v1.89.0-rc1 tools/release-check/build-amiga.sh
# Copyright (c) 2026 AmiCradle Team
# Distributed under the Boost Software License, Version 1.0.
set -eu
TAG=${TAG:-v1.89.0-rc1}
OUT=${OUT:-out}
U="https://github.com/DalsinAI/gfopenboost/releases/download/$TAG"
mkdir -p "$OUT"
for f in boosttest boost.txt gfopenboost-1.89.0-sdk.tar.xz.sha256; do
    curl -fsSL -o "$OUT/$f" "$U/$f"
done
chmod +x "$OUT/boosttest"
echo "release-check: $TAG"
ls -l "$OUT"
sha256sum "$OUT/boosttest"
