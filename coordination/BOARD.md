# Coordination board — 8 hourly work cycles

Baseline Russian PSP simulator. Source at repository root. Main integration branch: `roadhaul-realism-8h`.

- WORLD role: `roadhaul-world-8h`; deliver original 4096×4096m world grid, 6 biomes, roads, connectivity, landmark/route points. Owned files `src/world_*`, `data/world/**`, `tools/world/**`, `coordination/STATUS_WORLD.md`.
- ART role: `roadhaul-art-8h`; replace cubic vehicles, richer buildings, licensed CC0 assets optimized for PSP, wet roads/grass/sky assets. Owned files `src/asset_*`, `assets/**`, `tools/assets/**`, `coordination/STATUS_ART.md`.
- TRAFFIC role: `roadhaul-traffic-8h`; realistic vehicle response, traffic signals, pedestrians, collision response, enter/exit vehicle. Owned files `src/traffic_*`, `src/drive_*`, `src/pedestrian_*`, tests relevant, `coordination/STATUS_TRAFFIC.md`.
- INTEGRATION role: `roadhaul-integration-8h`; keep compiling & feature flags, maps renderer/audio/weather/day cycle, benchmark, QA, EBOOT package, PR integration. Owned shared files and `coordination/STATUS_RELEASE.md`.

Read `docs/REALISM_8H_PLAN.md` before writing. Propose cross-team APIs through role status files. Every cycle: inspect newest branches, code commits, work concretely, tests/build, publish PR, note evidence/blockers and schedule next. Final cycle ends with release artifact or transparent failure details. Do not delete branches or change user's other projects. No use of GTA copyrighted game textures, logos, models or direct map files.
