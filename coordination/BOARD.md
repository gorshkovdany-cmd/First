# Coordination board — 8 hourly work cycles

Baseline Russian PSP simulator. Source at repository root. Main integration branch: `roadhaul-realism-8h`.

- WORLD role: `roadhaul-world-8h`; deliver original 4096×4096m world grid, 6 biomes, roads, connectivity, landmark/route points. Owned files `src/world_*`, `data/world/**`, `tools/world/**`, `coordination/STATUS_WORLD.md`.
- ART role: `roadhaul-art-8h`; replace cubic vehicles, richer buildings, licensed CC0 assets optimized for PSP, wet roads/grass/sky assets. Owned files `src/asset_*`, `assets/**`, `tools/assets/**`, `coordination/STATUS_ART.md`.
- TRAFFIC role: `roadhaul-traffic-8h`; realistic vehicle response, traffic signals, pedestrians, collision response, enter/exit vehicle. Owned files `src/traffic_*`, `src/drive_*`, `src/pedestrian_*`, tests relevant, `coordination/STATUS_TRAFFIC.md`.
- INTEGRATION role: `roadhaul-integration-8h`; keep compiling & feature flags, maps renderer/audio/weather/day cycle, benchmark, QA, EBOOT package, PR integration. Owned shared files and `coordination/STATUS_RELEASE.md`.

Read `docs/REALISM_8H_PLAN.md` before writing. Propose cross-team APIs through role status files. Every cycle: inspect newest branches, code commits, work concretely, tests/build, publish PR, note evidence/blockers and schedule next. Final cycle ends with release artifact or transparent failure details. Do not delete branches or change user's other projects. No use of GTA copyrighted game textures, logos, models or direct map files.

## Urgent update (28.10) — all specialists must read `docs/LIVING_CITY_ACCEPTANCE.md`
- WORLD: prioritize packed city and road graph delivering from 100m, pedestrian crossings, patrol spawn zones, streetlight metadata.
- ART: first create saturated dawn/day/sunset/night and lit city, headlights, shadows approximated under fixed function PSP; sound minimal.
- TRAFFIC: city traffic + pedestrians + light obedience; 60s chase timeout, patrol backup 1–3, capture exactly one fine, crash response.
- INTEGRATION: connect 420s day, weather 1–3/day, audio low priority, real 100m+ delivery routes, police, dense city lights; add integration tests.
- Release order: dense city and boot safety before distant ornamental environments. Do not claim RTX or hardware launch without proof.
