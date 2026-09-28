# Fruit Jam shared UI, Atari adapter (2026-09-23)

This is a copy of the actual `pico_shared` browser/settings implementation in
`green-tasks/05-lowres-nx/port/pico_shared`, the UI shown in IMG_3876/3877.
It is not the Atari800 BASIC UI repainted white, nor a separately drawn clone.
The upstream license is retained in LICENSE. Other task directories are not
modified. The candidate archive retains the original imported files too.

Reused: `menu`, `showSettingsMenu`, controller test, recent-games UI,
`RomSelect_DrawLine`, `putText`, directory sorting, palette, and 8x8 font.
The font bitmap is byte-identical to LowRes NX's. The original 40x30 layout,
white settings page, inverted selection, fixed SAVE/CANCEL/DEFAULT row,
colour palette, contextual help, and controls are retained.

Atari adaptations:

- `atari_adapter.cpp` provides actual AFILE/SIO loading, per-Atari settings and
  the existing scratch[7] launcher return. No ROM flashing or PSRAM preload.
- Keep existing USB/GPIO/HSTX/audio drivers and button mapping. No external
  audio driver changes, new resampler, gain change, or game modification.
- SELECT or SELECT+START opens settings from the browser. In-game SELECT+START
  opens the same settings page; `Quit game` returns to this browser, while
  `Return to emulator selection` returns to Fruit Jam.
- Only implemented options are visible. No FDS/overclock/border/frame-skip or
  save-state placeholder. Audio enabled gates the existing final PCM; it does
  not change synthesis. Settings are in `/atari800-menu.cfg`; history in
  `/recent_A800.txt`. Other cores' configuration is not written.
- SRAM directory buffer is 4096 bytes (50 entries; use subdirectories for a
  larger collection; truncation is explicitly indicated). Artwork, WAV menu
  previews and screensaver allocations are disabled. Audio-test images remain
  normal emulator programs and are listed with games.
- Enable FatFs relative paths/string reads. Use the shared exFAT cwd wrapper
  with its real path tracking enabled; exFAT's native getcwd returns `/`.
- Fix null controller names, initial settings buffer contents, glyph bounds,
  true button-release waiting, stale selection bounds, empty directories,
  allocation checks, and the hardware-label/control-footer overlap.
- Core0 gets its full dedicated 4 KiB scratch stack; core1 remains 2 KiB.
- `Reset game` reopens the remembered Atari image through `AFILE_OpenFile`.
  A plain cold start is insufficient for XEX files and previously displayed
  AltirraOS Memo Pad instead of restarting the selected program.

Raw ambiguous `.rom`/`.bin` cartridges must be supplied with a typed `.car`
header. The shared browser does not offer Atari's old cartridge-type dialog.
Supported browser extensions: `.xex .atr .xfd .car .rom .bin`.

`tools/menu_test/run.sh` executes this same source on the host with scripted
USB input/FatFs fixtures under AddressSanitizer + UndefinedBehaviorSanitizer.
This tests menu logic/rendering, not real HDMI timing, SD hardware or game FPS.
Hardware verification is still required before accepting the candidate.
