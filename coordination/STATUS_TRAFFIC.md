# TRAFFIC — iteration 1 / actual code and QA

Date: 2026-10-28, deadline 06:20 UTC. Branch `roadhaul-traffic-8h`; PR targets `roadhaul-realism-8h`. Latest source commit before status: `8a2cd454eee88d7f1abd5049c08aea3b91c95377`.

## Implemented (not yet integrated into the game loop)

- `src/traffic_system.h/.c`: fixed-capacity directed lane graph (40 nodes / 112 directed edges), traffic signal controller with NS green/yellow, all-red, EW green/yellow, all-red phases; per-lane car tracking, car following, yielding at crosswalks, rain-dependent target speed, pedestrians walking only on permitted crossing phase, deterministic 4x4 sample city with 16 cars, 8 pedestrians, 4 signals. Bounded 22 cars / 12 pedestrians / 12 signals. No dynamic allocation.
- `src/traffic_system.c`: automatic detection of speeding and crossing red signal for players on mapped lanes; pursuit state, initial patrol + two reinforcements after 12s/25s (up to 3 patrols total), EXACT 60s simulated time timeout (double accumulator), bounded chase speed, catch only with contact >=0.75s and player speed <3.5m/s; one bank-limited fine per pursuit, cooldown after catch/timeout. Explicit collision violation hook.
- `src/drive_physics.h/.c`: separate vehicle dynamics for trucks/cars/vans, mass + trailer mass, engine torque/power cap, automatic gearbox, wet-road grip, rolling resistance, slope, braking, yaw/trailer lag; finite checked and bounded collision impulse/damage; safe low-speed enter/exit with occupancy and obstruction checks.
- `tests/test_traffic_system.c` and `tests/test_traffic_runner.sh`: host regression suite and 30-minute deterministic stress test.

## Evidence

Local GNU C99 `-O2 -Wall -Wextra -Werror -pedantic` test runner: PASS. UBSan `-fsanitize=undefined -fno-sanitize-recover=all`: PASS. ASan final revision `-fsanitize=address -fno-omit-frame-pointer` with leak detection: PASS. Tests verify signal red stop/green release, crosswalk permission, automatic speeding/red detection, reinforcement escalation, timeout at 59.9 vs 60.0 s, catch once and fine limited to available money, car entry/exit and collision finite, loaded vs unloaded and wet grip, 30-minute deterministic city update. `sizeof(RH_TrafficWorld)=3040` bytes on host. No PSP runtime FPS or hardware test claimed. Source content was verified byte-for-byte against the locally tested version via FNV-1a for all 6 committed files.

## Integration contract for RELEASE (do not change old Game save struct blindly)

1. Add `src/traffic_system.o src/drive_physics.o` to Makefile and run `sh tests/test_traffic_runner.sh` in CI.
2. Keep `static RH_TrafficWorld city;` in a global/static runtime object **outside** persisted `Game`, to preserve v5 saves. Call `rh_traffic_init(&city,seed)` then `rh_traffic_build_sample_city(&city,2800,1500,80)` for demo, or construct streamed world lane graph via `rh_traffic_add_node/add_edge/add_signal`. The graph is an example and not yet the full 4096m city.
3. At each unpaused simulation tick build `RH_TPlayer tp={g->player.x,g->player.z,g->player.v,g->player.yaw,g->bank}; rh_traffic_step(&city,&tp,dt,wetness); g->bank=tp.bank;`. Do not advance while paused/map open. Use `city.wanted`, `city.patrols[]`, `city.fines_count`, `city.signals[]`, `city.cars[]`, `city.pedestrians[]` to render patrol lights/sirens, NPC traffic and Russian fine notification. Avoid running old traffic in parallel or doubling counts.
4. On major collision call `rh_traffic_report_violation(&city,RH_VIOLATION_COLLISION,&tp)`. Speed and red-light checks run automatically when player follows a mapped lane.
5. For new vehicle mechanics map existing player fields to/from `RH_DriveBody` (x/z/yaw/speed/rpm/engine_response/gear/damage/fuel/trailer_yaw/trailer_mass_kg), but first benchmark handling and prevent double physics integration. Enter/exit via `RH_Driver`, `rh_drive_try_enter/exit`, with persistent original truck/cargo. The traffic module's `occupied` flag prevents moving a stolen/entered NPC as an AI car.
6. Rendering, Russian UI, police siren, and job routing 100m to max remain RELEASE/WORLD/ART ownership. Patrol AI currently pursues in free space rather than routing on streets; **do not claim full road-aware police navigation yet**. Add route graph following before final acceptance.
7. Verify PSPDEV build and PPSSPP graphical boot separately; CI compile is not a PSP hardware proof. Existing `Повреждённые данные` issue remains outside TRAFFIC ownership.

## Next iteration

Replace free-space patrol pursuit with connected lane-route pursuit, test low-speed blocking, add finite-math fuzzing and ASan final pass, optimize distant traffic LOD, provide safe adapter suggestions for the integrated `Game` runtime. Do not modify shared files directly.
