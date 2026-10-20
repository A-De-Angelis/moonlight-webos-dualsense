#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$ROOT/haptics" \
  "$ROOT/haptics/protocol_test.c" -o "$OUT/protocol_test"
cc -std=c11 -fsyntax-only -Wall -Wextra -Werror -D_GNU_SOURCE \
    -I"$ROOT/tests/stubs" -I"$ROOT/haptics" \
    "$ROOT/haptics/webos_haptics.c"
"$OUT/protocol_test"
python3 -m unittest discover -s "$ROOT/tests" -p 'test_*.py' -v
