#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUT="${TMPDIR:-/tmp}/psp_route_planner_test_$$"
trap 'rm -f "$OUT"' EXIT INT TERM
"$CC" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I src tests/test_route_planner.c src/route_planner.c -lm -o "$OUT"
"$OUT"
