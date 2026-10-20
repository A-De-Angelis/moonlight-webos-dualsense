#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bash "$ROOT/scripts/test.sh"
python3 "$ROOT/scripts/bootstrap.py" --dest "$ROOT/moonlight-tv-worktree"
cd "$ROOT/moonlight-tv-worktree"
# TOOLCHAIN_FILE must be a real arm-webos-linux-gnueabi toolchain file.
: "${TOOLCHAIN_FILE:?Set TOOLCHAIN_FILE to the OpenLGTV buildroot webOS NDK toolchainfile.cmake}"
CI=1 ./scripts/webos/easy_build.sh -DCMAKE_BUILD_TYPE=Release
printf 'IPK output: %s\n' "$PWD/dist"
