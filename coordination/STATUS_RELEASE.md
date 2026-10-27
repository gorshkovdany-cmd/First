# PSP realism coordination — initial status

2026-10-28: Initial PSP native C source imported into branch `roadhaul-realism-8h`. Build baseline based on Russian 1.0.

Critical known unresolved: physical PSP reported 'Повреждённые данные'; encrypted compatibility PBP had cryptographic verification but was not tested on device. Earlier PPSSPP Xvfb tests only yielded black frames due to graphics init failure. Do not claim boot passes.

Immediate fix already committed: game save path now `ms0:/PSP/GAME/DALREYS/SAVE.DAT` (real install directory) and attempts legacy `ms0:/PSP/GAME/ROADHAUL/SAVE.DAT` for migration.

Baseline local tests: simulator, 137 render configs, 45k fixed updates, safety source audit, Cyrillic coverage all passed. FPS and hardware boot unverified.

Next: check GitHub Actions PSPDEV+pspbuild release; merge only passing PRs from WORLD/ART/TRAFFIC, add 420-second day/1–3 weather shifts/day, traffic/weather visual and 4x4km world streaming, produce PSP install ZIP and non-black PPSSPP capture if possible. Hard deadline Oct 28 2026 06:20 UTC.
