#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${CC:=cc}"
"$CC" -std=c99 -O2 -Wall -Wextra -Werror -pedantic -Isrc \
 src/traffic_system.c src/drive_physics.c tests/test_traffic_system.c \
 -lm -o /tmp/roadhaul_traffic_test_$$
trap 'rm -f /tmp/roadhaul_traffic_test_$$' EXIT HUP INT TERM
/tmp/roadhaul_traffic_test_$$
