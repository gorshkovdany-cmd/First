# ART status — iteration 1/8

**Branch:** `roadhaul-art-8h`; **target:** `roadhaul-realism-8h`.

## Implemented

- Inspected baseline `src/render_psp.c`: currently procedural untextured cube-heavy geometry; day/night is a global color multiplier, single-color sky, windows darkened at night, 54k vertex cap and no texture atlas. Existing car wheels are cuboids and road structures repeat. `src/render_psp.c` belongs to integration role and was NOT modified.
- Added deterministic offline compiler and indexed 1/64 m meshes for 9 distinct asset types with near/mid/far LOD. 2,410 vertices and 2,964 triangle records total; ~38 KiB static geometry excluding descriptors. Car has chamfered hood and windshield, 8-sided wheels, mirrors, front/back lamps; truck has trailer, wheels and front glass; 3D buildings have multi-storey windows, roof details, doors, loading docks. Added trees, grass, bridge, lamp, puddle.
- Added atmosphere palette with 12 time keyframes: vivid sunrise, cyan daylight, rose/violet sunset, deep blue night, cloud cover and wetness. Lit windows, headlights and streetlamps mark emissive triangles for bypassing global day tint. Puddles approximate sky reflections with colored triangles. No real RTX/PBR.
- Verified Kenney Car Kit / City Kit Roads, Industrial, Suburban are CC0 candidate packs (2026-10-27 site results). No external models were downloaded or committed in this iteration; originals remain procedural.

## Tests (local desktop, NOT PSP hardware)

- `sh tools/assets/test_assets.sh` — PASS: 9 model types × 3 LODs; 2,964 emitted triangles, 400 emissive at night, 1,440 sky steps.
- `cc -std=c99 -Wall -Wextra -Werror -pedantic -O2` — PASS.
- No dynamic heap allocations in asset library. Need integration build, actual GPU test, and framerate measurements. Current code is a standalone module and does not yet alter the game until integrator hooks it.

## Integration requests / ownership

1. Integrator: add `src/asset_mesh.o` to `Makefile`, `#include "asset_mesh.h"` in `src/render_psp.c`, implement RHAssetEmit callback that appends 3 vertices (emissive must bypass `day_color`), and replace car/building/tree render calls gradually. Do not draw all 860-triangle buildings at once; cap visible high-LOD count.
2. Integrator: replace solid clear sky with 4-stop gradient using `rh_asset_atmosphere(g->clock_hours,cloudiness,wetness)` and add small ground-projected light pools under lamps/headlights; only at night. Color palette already available. Render opaque pseudo-reflections for wet roads, no shaders.
3. User's new priorities: **living illuminated city, saturated sunrise/sunset/day, strong shadows, headlamps, minor sound polish**. Coordinate with traffic and integration roles for city traffic and audio. Patrol pursuit/penalties is gameplay role, not ART.
4. Follow-up ART: implement offline CC0 Kenney OBJ/glTF to PSP conversion with manifest and triangle/texture budgets, richer modular facades and roof variation, texture atlas, reduced near-LOD building count.

**Implementation commit SHA:** `d3a28d393a72d2f0f854aeb562f75c760e990395`; **asset manifest commit:** `cfeee3b43c444ba81e06fe32b09c29a9a5c5bb0d`. **PR:** https://github.com/gorshkovdany-cmd/First/pull/7.

## Blockers

No PPSSPP/physical PSP validation in ART role; full integration not yet performed. No texture atlas or third-party asset conversion yet. Added `assets/asset_manifest.json` with per-LOD budgets and CC0 candidate provenance; raw geometry is 38,172 bytes. High-detail building is 860 triangles and needs strict visibility budget.
