#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if ! command -v docker >/dev/null 2>&1; then
  echo 'Требуется Docker Desktop / Docker Engine.' >&2; exit 1
fi
if ! command -v pspbuild >/dev/null 2>&1; then
  echo 'Требуется pspbuild: cargo install pspbuild --locked' >&2; exit 1
fi
./tests/run_tests.sh
./tests/run_render_tests.sh
./tests/run_stress.sh
./tests/safety_audit.sh
python3 tests/check_russian.py
docker run --rm --platform linux/amd64 -v "$PWD:/source" -w /source pspdev/pspdev:v20261001 make -j2
mkdir -p dist/PSP/GAME/DALREYS
pspbuild encrypt EBOOT.PBP -o dist/PSP/GAME/DALREYS/EBOOT.PBP -v
pspbuild inspect dist/PSP/GAME/DALREYS/EBOOT.PBP | tee dist/psp-inspection.txt
pspbuild verify dist/PSP/GAME/DALREYS/EBOOT.PBP | tee dist/psp-verification.txt
(cd dist && zip -q -r ../DALNIY_REYS_PSP_РУССКИЙ_НОВАЯ_СБОРКА.zip PSP)
echo 'Готово: DALNIY_REYS_PSP_РУССКИЙ_НОВАЯ_СБОРКА.zip'
