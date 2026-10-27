# ДАЛЬНИЙ РЕЙС — PSP REALISM / 8-hour iteration plan

**Baseline:** Russian-language RoadHaul 1.0 from 2026-10-26. Prior physical PSP reported **«Повреждённые данные»**, so a successful compile is NOT sufficient. Latest compatibility build used `pspbuild encrypt` and `verify`, but never received physical-hardware confirmation.

**Reference:** user-supplied aerial *concept* with north mountain forest, western freight port, central plains/town, eastern coastal city with canals/bridges, southern wetlands, island chain, and airport. Create an original game map; do not reuse GTA maps, geometry or branded assets. No user screenshot may be uploaded publicly without separate permission.

## Technical choice
Native PSPDEV/PSPSDK, C, libGU/libGUM, hardware transform, 480x272, 64 MiB RAM, 333 MHz peak. Stay within actual PSP geometry, texture, and EBOOT/PBP restrictions. This is a lean home-grown PSP-specific runtime, NOT Unity, Unreal or Godot. Don't switch to third-party large engines without a demonstrably successful benchmark.

## World coordinates and biome layout
- World is **4,096 x 4,096 real metres** at 1 unit = 1 m; ~16.78 km^2, not artificial 'km' that are only a few turns. Roads, building footprints, traffic speeds, odometer and delivery lengths use actual metric scaling.
- North (z=0..900): ridgelines, mountain switchbacks, conifers, national park, overlooks, snow-free rocky heights.
- West (x=0..1200,z=900..2500): deepwater industrial port, dock cranes, warehouses, tanker access, truck terminals, dense vehicle logistics.
- Interior (x=1000..2600,z=900..2700): farming lowlands, lakes, town centre, commercial yards, airport approaches and trucking depot.
- East (x=2500..4096,z=950..3200): dense riverfront city, city blocks with 3D facades, multiple bridges, waterfront boulevard, suburban streets, intersections.
- South (z=2850..4096): wetlands with reeds, tidal channels, raised causeways; island chain connected by long bridges, tourist and supply routes.
- Road classes: motorway > boulevard > arterial > 2-lane rural > service/port > local alley. Continuous driveable network, no teleport seams. Add city loops, truck-safe access and appropriate turns. Don't blindly copy image geometry.
- At least 3 independently distinct cargo origins/destinations by the first build, with visible logistics context; preserve infinite jobs beyond map size via rerouting.

## Performance and visuals
- 32x32 or 64x64 metre metadata cells; geometry chunks ideally 128x128m, visible neighbourhood streamed, **hard cap on resident chunks** and texture pages. No allocation spikes while driving. Use prefetch and deterministic chunk IDs. Full world is NOT a single resident mesh.
- Distances: high detail near 0..65m; medium 65..170m; merged/impostor far 170..400m; fog/haze as horizon >400m. Culling per chunk and camera cone; batch materials; reuse low-poly mesh templates and instance transforms.
- Target 30fps steady in exterior city; 20fps minimum controlled fallback on density, pedestrians, particles, grass, and distant traffic. No promise of fixed FPS before PPSSPP/hardware benchmark.
- PSP-friendly convincing realism: atlas textures (careful CC0 licensing), UV facades/windows, textured asphalt with road marks, static directional/ambient vertex lighting; night streetlight pool decals; night windows; shiny wet pavement via carefully placed specular-colored triangles or low-opacity asphalt overlays; puddles as opaque reflective atlas/sky-colored quads (NOT screen-space ray tracing). No true real-time shadows, PBR, HDR or GTA-grade reflection simulation.
- Day/night exactly 420 real seconds per in-game day with clear sun/sky color interpolations, streetlights/wet effect nighttime; seeded weather transitions 1–3 times per simulated day with distinct dry, cloudy, drizzle, downpour and improving states; transitions gentle and deterministic for testability.
- Grass is crossed billboard clumps only near road and player, sparse/faded far away. Trees share small geometry/texture atlas and LOD.
- All models from verified CC0 packs or original geometry. **Quaternius licence terms may have changed in 2026: check per-pack and redistribution terms**; safe defaults are Kenney CC0 Car Kit and City Kit. Record exact URLs, revision, licence before bundling anything. Never copy Rockstar/game assets.

## Gameplay and behaviour
- First/third person inside/outside of drivable vehicle; collision-enabled entry/exit at safely stopped/low-speed vehicles, nearby AI traffic only when stationary and available; preserve vehicle states, not teleport into road traffic at speed.
- Realistic but efficient force/friction model: throttle/gear/RPM, gross weight/grade, braking friction and stopping distance, traction based on weather, steering radius/speed, collision impulses and light-body dent approximation, damage and servicing. Avoid unbounded impulses/NaN/unsafe memory.
- AI traffic follows connected lane graph, intersection right-of-way / stop signals, speed classes, reaction to proximity and obstacle stop; 10–22 agents max around player, lower in dense rain city scenes. Spawn/unspawn outside camera with deterministic seeds.
- Pedestrians along sidewalks cross only at crossings; LOD animated sprite/billboard far and 3D simplified meshes near; 4–12 nearby as budget allows. Do not add harmful gameplay incentives for crashes.
- Lights have 3+ phase states; collision boxes / lane links consistent with visual placements. Police not required in the 8h baseline.
- Fuel stations, cargo loading bays, parking, repair shop, save system, persistent economy; original file path compatibility with migration/backup.
- Fully Russian GUI, fonts with no clipped tops or unsupported glyphs. Optimize text for 480x272 and safe margins.

## Shared ownership
Main integration branch: `roadhaul-realism-8h`. Separate branches: `roadhaul-world-8h`, `roadhaul-art-8h`, `roadhaul-traffic-8h`, `roadhaul-integration-8h`.
Ownership paths: WORLD=src/world_* + data/world + tools/world; ART=assets, tools/assets, src/asset_*; TRAFFIC=src/traffic_*,src/drive_*,src/pedestrian_* + traffic tests; INTEGRATION=Makefile, src/main_psp.c, src/render_psp.c, src/sim.*, tests, workflows, coordination/RELEASE*. Each owner may write only its own subtree; propose shared API changes in `coordination/<role>_api.md` before modifying others' owned code.
- All code changes are PRs targeting `roadhaul-realism-8h`, **not main**. Coordinator merges/cherry-picks only after C compilation + QA; if GitHub PR cannot merge, use connector tree/branch carefully, never overwrite another role's work.
- Everyone reads `coordination/BOARD.md` and updates only their own role status file under `coordination/STATUS_WORLD.md`, etc., recording commit SHA, tests, blockers, next task. The integrator reads status + PR diffs hourly.
- Work can use mail only for notifications, NOT as a substitute for source sync. Avoid contacts/secrets in public repo.

## Eight-cycle milestones (relative to scheduled start)
1. Baseline audit, safe build and PBP startup triage; shared coordinate and asset API; integration tests.
2. Biomes and terrain chunk generator; truck & car meshes atlas; lane graph and collision baseline.
3. Streaming and city grid; depots/ports, day/night prototype; vehicle control/AI stopping.
4. Bridges/wetlands/airport; traffic lights/pedestrians; weather and lighting integration.
5. 3D richness: facades, road markings, vehicle parts; hitch physics/accident effects; quality review.
6. Optimisation pass: culling, instancing, LOD, 64 MiB heap/2 MiB eDRAM budgets; frame-time stress.
7. Feature freeze; full integration; compile PSP EBOOT; run tests & real PPSSPP graphical boot when possible.
8. Release candidate: signed/encrypted EBOOT if supported, verify PBP + KIRK, zero unsafe flash/NAND operations, install ZIP `PSP/GAME/DALREYS/EBOOT.PBP`, SHA-256 report; do NOT call it proven playable unless emulator+hardware actually confirms. Provide artifacts and unresolved defects at deadline.

## Release acceptance gates
- Minimal: PBP correct header & SFO Russian title, PSP module valid and appropriate homebrew loader compatibility; install path; PSPDEV compile; unit/geometry/night/weather/controller/traffic/save tests; memory leak and finite math test, dynamic worst case budget, archive CRC + hashes.
- Important: PPSSPP must actually show a non-black game frame and run 2+ minutes, and preferably physical PSP boot is separately confirmed. If not, release status remains **UNVERIFIED BOOT**; explain to user exactly. Never mislabel automated GitHub Action as proof of graphical boot (prior attempts returned black images).
- Stop after eight scheduled rounds. Better a working smaller map and traffic than non-bootable grand promises. Avoid overconfident statements about PSP safety; no firmware/NAND/flash writes, use memory stick only.
