#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/psp_visibility_test_$$"
trap 'rm -f "$OUT"' EXIT INT TERM
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I src \
  tests/test_visibility_budget.c src/visibility_budget.c -lm -o "$OUT"
"$OUT"
