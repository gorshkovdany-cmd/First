#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer src/sim.c tests/test_stress.c -lm -o /tmp/rh_stress_test
ASAN_OPTIONS=detect_leaks=1 /tmp/rh_stress_test
