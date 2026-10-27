#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
python3 tools/assets/generate_meshes.py
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Isrc src/asset_mesh.c tools/assets/test_asset_mesh.c -lm -o /tmp/roadhaul_asset_test
/tmp/roadhaul_asset_test
