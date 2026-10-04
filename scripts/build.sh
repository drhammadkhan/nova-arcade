#!/usr/bin/env bash
# =====================================================================
#  Builds everything into ./build:
#    build/nova-arcade-full.bin      launcher + bootloader + partitions (flash at 0x0)
#    build/nova-arcade-sd-card.zip   /arcade folder for the SD card
#    build/sd/arcade/*               the same files, unzipped
#    build/site/                     the web flasher, ready for GitHub Pages
#
#  Needs arduino-cli with the esp32_bluepad32 core and LovyanGFX installed
#  (scripts/setup.sh does that), plus python3 for esptool.
# =====================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build"
FQBN="esp32-bluepad32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,CDCOnBoot=cdc"
GAMES="NovaLance Blockfall BrickStorm AlienTide NeonSerpent AstroDrift HopRush VoltRally MazeMunch PixelPeaks TurboHorizon GemCascade CityShield"
VERSION="${VERSION:-$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)}"

rm -rf "$OUT"
mkdir -p "$OUT/sd/arcade"

compile() {  # name, sketch dir
  echo "==> building $1"
  arduino-cli compile --fqbn "$FQBN" --library "$ROOT/lib/ArcadeCore" \
    --output-dir "$OUT/$1" "$2" | grep -E "Sketch uses|error" || true
  test -f "$OUT/$1/$1.ino.bin"
}

compile Launcher "$ROOT/Launcher"
for g in $GAMES; do
  compile "$g" "$ROOT/games/$g"
  cp "$OUT/$g/$g.ino.bin" "$OUT/sd/arcade/$g.bin"
  cp "$ROOT/games/$g/thumb.png" "$OUT/sd/arcade/$g.png"
  cp "$ROOT/games/$g/meta.txt" "$OUT/sd/arcade/$g.txt"
done

# ---- single flash image: bootloader + partition table + boot_app0 + launcher
DATA="${ARDUINO_DATA_DIR:-$HOME/.arduino15}"
BOOT_APP0="$(find "$DATA/packages/esp32-bluepad32/hardware/esp32" -name boot_app0.bin | head -1)"
ESPTOOL="$(find "$DATA/packages" -path '*esptool_py*' -name esptool.py | head -1 || true)"
if [ -n "$ESPTOOL" ]; then ESPTOOL_CMD=(python3 "$ESPTOOL"); else ESPTOOL_CMD=(python3 -m esptool); fi
"${ESPTOOL_CMD[@]}" --chip esp32s3 merge_bin -o "$OUT/nova-arcade-full.bin" \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 "$OUT/Launcher/Launcher.ino.bootloader.bin" \
  0x8000 "$OUT/Launcher/Launcher.ino.partitions.bin" \
  0xe000 "$BOOT_APP0" \
  0x10000 "$OUT/Launcher/Launcher.ino.bin"

(cd "$OUT/sd" && zip -qr "$OUT/nova-arcade-sd-card.zip" arcade)

# ---- web flasher site
SITE="$OUT/site"
cp -r "$ROOT/web" "$SITE"
mkdir -p "$SITE/firmware"
cp "$OUT/nova-arcade-full.bin" "$OUT/nova-arcade-sd-card.zip" "$SITE/firmware/"
for g in $GAMES; do cp "$OUT/sd/arcade/$g.bin" "$SITE/firmware/"; done
sed -i.bak "s/__VERSION__/$VERSION/g" "$SITE/manifest.json" "$SITE/index.html" && rm -f "$SITE"/*.bak

echo
echo "Done ($VERSION):"
ls -la "$OUT"/*.bin "$OUT"/*.zip
