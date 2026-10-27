# Fast iteration coordination — Road Haul PSP, 28 October 2026

## Goal and exact deadline
Finish the best **actually integrated, tested** Russian-language PSP game by **10:00 MSK (07:00 UTC)**, not the best-looking unbuildable code. Release freeze begins ~09:15 MSK; publish build/result and honest QA by ~09:45 MSK. Target up to **20 verified code iterations per role** before the cutoff, subject to real executor/tool availability, not a guarantee.

Four automation IDs / branches / role statuses:
- WORLD: `6ae12585896c8191bec911c32f57aed3` → `roadhaul-world-8h` → `coordination/STATUS_WORLD.md`.
- ART: `6ae12587b810819181d0fa919293a319` → `roadhaul-art-8h` → `coordination/STATUS_ART.md`.
- TRAFFIC: `6ae1258a13048191a5c4ecea97e8d79a` → `roadhaul-traffic-8h` → `coordination/STATUS_TRAFFIC.md`.
- INTEGRATION: `6ae1258cac208191bb85a10041bbad10` → `roadhaul-integration-8h` / canonical merge `roadhaul-realism-8h` → `coordination/STATUS_RELEASE.md`.

## Protocol per completed iteration
1. Check latest shared `coordination/BOARD.md`, `docs/LIVING_CITY_ACCEPTANCE.md`, and each role's latest GitHub branch + PR status (do not assume siblings already finished).
2. Check time and your status. Stop after 20 **verified** iterations in your role; on/after ~09:15 MSK, focus only release stabilization/handoffs. Existing hourly schedule remains as safety fallback.
3. Use your own branch. Implement a **real source/test/artifact change**; run available native tests and PSP build when applicable, record exact test names/results and commit SHA in `coordination/STATUS_<ROLE>.md`. The status must include `COMPLETED_ITERATIONS: N` and `LAST_COMPLETED_COMMIT: ...`. **Count a unit only after verified implementation**; change in status alone is not a code iteration.
4. Push changes, open/update PR to `roadhaul-realism-8h`, write handoff with precisely which files/APIs need integration, indicate CI state. Write no conflicting changes into another role's owned files.
5. **Wake available peer via Automations**, if tools authorized in this execution: use `mcp__Automations__peek` to find enabled, idle teammate with fewer than 20 completed code iterations and no currently queued/in-progress run. Once, call `mcp__Automations__run_now({jawbone_id: "peer-id"})` to request a single immediate run. Rotation preference WORLD→ART→TRAFFIC→INTEGRATION→WORLD; can try another idle peer if preferred is active. At most **one cross-task run_now per verified completed iteration**; no blind repeat, and don't call if next scheduled tick is imminent (<2m) or release freeze passed.
6. `run_now.success` only means **request accepted**, not new run completed; never increment another role's count based on this. Never claim any unexecuted tasks as complete. If tools unavailable, rate limits / rejection / in-progress state, write a GitHub handoff and let next scheduled automation pick it up. Don't accidentally turn this into a synchronous waiting loop.
7. If new integration PR merged or builds are clean, prioritize notifying/restarting INTEGRATION when it's idle. Only integration engineer creates final PSP ZIP.

## Safety and acceptance
- PSP-3008, 64MB, 333 MHz. Choose PSPDEV C + libGU/libGUM; not Unity/RTX.
- Performance test the city at night with traffic & wet street illumination, capped agent counts and reproducible load.
- All original source or verified CC0 materials; list licenses. Don't upload user's supplied GTA-like reference image as content/asset.
- PSP boot was not demonstrated on real hardware; 'Corrupted Data' was reported earlier. Compiled/pspbuild-verified PRX is necessary but does not prove real console boot; never claim otherwise.
- Police top speed approx 0.9× *current player vehicle* theoretical maximum, 60s real-time pursuit, up to three backups, traffic-safe approaches from rear/front/side, single fine on capture.
- Delivery length via graph from 100m, day-night in 420s, 1–3 weather transitions/day.
- If the deadline is approaching with incomplete features, stop broadening scope, ship verified best effort and transparent blockers. Do not risk a broken last-minute build.
