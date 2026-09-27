# CLAUDE.md

Guidance for Claude Code (and humans) working on **Nova Arcade**: a retro games console for an ESP32-S3 2.8" board. A launcher in the factory partition loads game `.bin` files from the SD card, and games are played with a Bluetooth LE gamepad.

## Hardware (tested board: LCDWIKI ES3N28P, no touch)

- ESP32-S3, 16 MB flash, 8 MB OPI PSRAM
- ILI9341V 240×320 IPS panel on SPI: SCLK 12, MOSI 11, MISO 13, CS 10, DC 46, RST tied to EN (-1), backlight 45. Runs landscape (rotation 1), inverted colours.
- **Display SPI clock is 27 MHz.** Both 80 MHz and 40 MHz produced pixel glitches on the real board (sparkles, then rows drawn shifted sideways). Don't raise it without testing on hardware.
- Audio: ES8311 codec. I2S MCLK 4, BCLK 5, LRCK 7, DOUT 8 (ESP→codec). I2C SDA 16, SCL 15, address 0x18. Amp enable is GPIO 1, active low. Volume uses the codec's DAC register 0x32 (hardware volume, confirmed working); software scaling is only a fallback.
- SD card: SDMMC 4-bit, CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 (falls back to 1-bit mode).
- All pins live in `lib/ArcadeCore/src/arcade/Config.h`.

## Controllers

The ESP32-S3 has **Bluetooth LE only**. Pads that use Classic Bluetooth, including all 8BitDo pads, PS4/PS5 and Switch Pro, will never pair. The owner uses a **Stadia controller in Bluetooth mode** (pair with Y + Stadia held for 2 s). Xbox Wireless on firmware v5+ also works. Input comes from Bluepad32. Stadia's "…" button maps to SELECT and "≡" to START.

## Toolchain

- Board package: **esp32_bluepad32 4.1.0** (arduino-esp32 2.0.x / ESP-IDF 4.4). IDF 5 APIs are not available: use the legacy `driver/i2s.h`, not `i2s_std`.
- FQBN: `esp32-bluepad32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,CDCOnBoot=cdc`
- Library: LovyanGFX 1.2.30, pinned in `scripts/setup.sh`.
- `bash scripts/setup.sh` installs the toolchain. `bash scripts/build.sh` builds everything into `build/`:
  - `nova-arcade-full.bin`: the flash image (bootloader, partition table, launcher), written at offset 0x0
  - `nova-arcade-sd-card.zip`: the `/arcade` folder for the SD card
  - `site/`: the web installer
- `bash scripts/build-web.sh` builds the browser player into `build/site/play/` (see below).
- CI (`.github/workflows/build.yml`) runs the same scripts on every push, deploys `build/site` to GitHub Pages (https://drhammadkhan.github.io/nova-arcade/), and makes a release on `v*` tags.

## Flashing and debugging on the Mac

- Flash: `esptool.py --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 write_flash 0x0 build/nova-arcade-full.bin`. `scripts/flash-mac.command` does this in one click and finds an existing esptool. If the board won't connect: hold BOOT, tap RST, release BOOT.
- Writing the full image wipes NVS at 0x9000, which clears the saved controller pairing, volume and hi-scores.
- Serial runs at 115200 over native USB CDC. Use it for debugging; the board reports errors such as "ES8311 not found".
- To test a single game quickly without the launcher, flash its `build/<Game>/<Game>.ino.bin` at 0x10000. This overwrites the launcher; reflash the full image afterwards.

## Architecture

```
0x010000 factory (2 MB)  Launcher/        menu, reads /arcade on SD, copies the chosen .bin to ota_0
0x210000 ota_0   (6 MB)  the current game (installed by the launcher via the Update library)
```
- Partition table: `Launcher/partitions.csv`.
- On start-up, every game calls `arcade::begin()`, which points the boot partition back at factory. Any reset therefore returns to the launcher, as does SELECT+START or the Stadia button.
- The launcher skips reinstalling if the NVS key `arcade/slot` matches the game's path, size and mtime.
- NVS namespaces: `arcade` holds shared settings (volume `vol`, `last`, `slot`). Each game has its own namespace for its difficulty (`diff`) and hi-scores (`hi`, `hiE`, `hiH`).
- SD layout: `/arcade/<Game>.bin` (required), `<Game>.png` (160×120 thumbnail) and `<Game>.txt` (title, description and save id, one per line).

## ArcadeCore (`lib/ArcadeCore`): header-only, include once per sketch

- `Render.h`: the frame is drawn as five 320×48 strips, double-buffered in internal DMA RAM and pushed with `pushImageDMA`. `draw()` runs once per strip, with `Y0` set to the strip's top row. Pixel colours in the buffer are **byte-swapped RGB565** (`rgbS()`, `pal[]`); LovyanGFX text and circle calls take normal RGB565 (`c565()`). Every helper clips to the current strip.
- `System.h`: `arcade::begin(id, &music)` and `arcade::run(step, draw)` provide a fixed 60 Hz step, the pause menu (`arcade::pause()`), volume (SELECT+Up/Down), difficulty, hi-scores (`loadHi`/`saveHi`) and `exitToLauncher()`.
- Difficulty: Easy, Normal and Hard, saved per game (NVS key `diff`). Title screens call `arcade::titleInput(in)` (Up/Down picks the difficulty, Left/Right the volume) and `arcade::drawDifficulty(y)`. Games scale their hazards with `arcade::speed()` (0.7 / 1.0 / 1.25) and `arcade::frames(n)`; slower is easier, and the player's own speed isn't scaled. Hi-scores are kept per difficulty: `hi` (Normal, so older saves carry over), `hiE` and `hiH`.
- `Audio.h`: the chiptune synth, running as a task on core 0 with 22.05 kHz I2S output. Songs are defined per game with the `Music`, `SongDef` and `Bar` structs. There's a shared `Sfx` enum. `audio::play(sfx, param)`.
- `Input.h`: `Pad` with `ax`/`ay` plus `held`, `pressed` and `released` bitmasks (`BTN_*`), and `input::Repeat` for auto-repeat.

## Conventions and gotchas

- **Sketch code lives in `game.h`; the `.ino` only does `#include "game.h"`.** arduino-cli inserts auto-generated prototypes at the top of `.ino` files, before the game's own structs, which breaks the build. This already broke CI once.
- Mark header helper functions `static inline` (not bare `static`), or `--warnings all` turns "unused function" into an error. Don't mark variables `inline` (that's C++17 only).
- Avoid short global type names that clash with SDK headers. A `struct Pop` failed to compile, so it was renamed `PopFx`.
- `Wire.h` must be included before LovyanGFX. `ArcadeCore.h` does this so arduino-cli's library detection picks up Wire.
- Keep DMA strip buffers in internal RAM (`setPsram(false)`). Launcher thumbnails use PSRAM sprites.
- Art is generated by Python scripts. Edit the script, then regenerate the header: `games/NovaLance/tools/make_assets.py`, `tools/art/logos.py`, `tools/art/alientide_art.py`, `Launcher/tools/make_assets.py`.
- All game art and music must stay original: no copied sprites, logos or tunes from commercial games.

## Desktop simulator (`tools/sim`)

This compiles any game or the launcher for the host with stubbed hardware, and saves PPM screenshots to `tools/sim/out/`. Scripted inputs live in `sim_<game>.cpp`.
```bash
cd tools/sim
g++ -std=gnu++17 -O1 -DARCADE_SIM -I. -I../../lib/ArcadeCore/src -x c++ -fsanitize=address,undefined sim_blockfall.cpp -o sim_bf && ./sim_bf
python3 sheet.py bf out/bf.png title play clear over   # contact sheet
```
Use it to check visuals and game logic before flashing. Also rebuild the thumbnails (`games/*/thumb.png`) and `web/img/*.png` from it when a game's look changes.

## Browser player (`web/play/`, `tools/web/`)

Every game also builds to WebAssembly with Emscripten, from the same `game.h`, and runs at https://drhammadkhan.github.io/nova-arcade/play/.
- `scripts/build-web.sh` (run after `build.sh`; needs `em++` and LovyanGFX for the font) writes `build/site/play/<game>.js/.wasm`. CI installs Emscripten 6.0.10 with `setup-emsdk`.
- It builds with `-DARCADE_SIM -DARCADE_WEB`, reusing the simulator's stubs. `-Itools/web` comes first, so `tools/web/ArcadeSimSystem.h` replaces the sim's: Preferences live in localStorage (`nova-arcade/<namespace>/<key>`), and `exitToLauncher()` calls `arcadeWebExit()`, which goes back to the game list.
- `tools/web/web_main.cpp` exports `web_init`, `web_pad`, `web_frame` (returns 320×240 RGBA) and `web_audio` (the synth's `renderBlock` at 22.05 kHz). `web/play/play.js` handles the keyboard, the Gamepad API, the canvas and Web Audio.
- When adding a game, add it to `GAMES` in `build-web.sh` and in `web/play/play.js`.
- To test locally: build into a copy of `web/`, serve it with `python3 -m http.server`, and drive it with Playwright (Chromium is preinstalled in cloud sessions).

## Status

- Confirmed on hardware: Nova Lance as a standalone sketch (display at 27 MHz, sound, Stadia input, hardware volume) and the launcher flashed and booting.
- Not yet verified on hardware: SD scanning, installing and booting games through the launcher, the difficulty setting, and the games other than Nova Lance (Blockfall, Brick Storm, Alien Tide, Neon Serpent, Astro Drift, Hop Rush, Volt Rally, Maze Munch). All of them run in the desktop simulator.
- GitHub Pages is live and deploys from `main` (and `v*` tags). Other branches only build.
