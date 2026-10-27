# Safety & QA — ROAD HAUL 1.0

## Design restrictions

- Runs as `PSP_MODULE_USER`, no custom kernel drivers.
- No writes to firmware flash, no calls to NAND, IdStorage, custom bootloaders, cold reset functions or flashing utilities.
- Only persistent data is `ms0:/PSP/GAME/ROADHAUL/SAVE.DAT`, `.NEW`, and `.BAK`; never writes outside the game directory during normal operation.
- Saves use a checksum and a two-generation swap to recover on the next boot. No save is guaranteed immune to card failures or removal of the Memory Stick mid-write.
- `scePowerSetClockFrequency(333,333,166)` selects a standard PSP hardware frequency, not an overclock above documented hardware limits.
- Game buffers are statically allocated and GPU draw-list counts are bounded (54k 3D and 25k HUD vertices).
- Failure of the procedural audio channel is nonfatal: the game continues silently.

## Important limitations

All third-party code could theoretically expose hardware/firmware bugs, regardless of a code audit. PSP/PPSSPP hardware boot, GPU output and FPS require actual device/emulator testing. An EBOOT file compiling alone is insufficient proof it will run without freezing. Preserve Memory Stick backups and test in PPSSPP first.
