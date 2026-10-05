#!/usr/bin/env bash
# =====================================================================
#  Builds the browser player: every game compiled to WebAssembly.
#    build/site/play/<game>.js + .wasm   (next to web/play/index.html)
#
#  Needs Emscripten (em++ on PATH) and LovyanGFX for its 6x8 font
#  (scripts/setup.sh installs it; or set LGFX_DIR). Run after build.sh,
#  which creates build/site, or set OUT to build somewhere else.
# =====================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/site/play}"
GAMES="NovaLance Blockfall BrickStorm AlienTide NeonSerpent AstroDrift HopRush VoltRally MazeMunch PixelPeaks TurboHorizon GemCascade CityShield"
if [ -z "${LGFX_DIR:-}" ]; then
  USERDIR="$(arduino-cli config get directories.user 2>/dev/null || echo "$HOME/Arduino")"
  LGFX_DIR="$USERDIR/libraries/LovyanGFX"
fi
test -f "$LGFX_DIR/src/lgfx/Fonts/glcdfont.h" || { echo "LovyanGFX not found at $LGFX_DIR (set LGFX_DIR)"; exit 1; }

mkdir -p "$OUT"
for g in $GAMES; do
  id="$(echo "$g" | tr '[:upper:]' '[:lower:]')"
  echo "==> web $g"
  em++ -O2 -std=gnu++17 -fno-exceptions -fno-rtti \
    -DARCADE_SIM -DARCADE_WEB -DDEFAULT_VOLUME=6 \
    -DGAME_H="\"../../games/$g/game.h\"" \
    -I"$ROOT/tools/web" -I"$ROOT/tools/sim" -I"$ROOT/lib/ArcadeCore/src" -I"$LGFX_DIR/src" \
    "$ROOT/tools/web/web_main.cpp" -o "$OUT/$id.js" \
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 \
    -sEXPORTED_FUNCTIONS=_web_init,_web_pad,_web_frame,_web_audio,_web_audio_rate \
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPF32 \
    -Wno-format-truncation
done
ls -la "$OUT"
