#!/usr/bin/env bash
# Installs the toolchain used by scripts/build.sh (Linux/macOS, needs arduino-cli on PATH).
set -euo pipefail
arduino-cli config init --overwrite >/dev/null
arduino-cli config set board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json \
  https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
arduino-cli core update-index
arduino-cli core install esp32-bluepad32:esp32@4.1.0
# LovyanGFX pinned to the version the project is tested with
LIBDIR="$(arduino-cli config get directories.user)/libraries"
mkdir -p "$LIBDIR"
rm -rf "$LIBDIR/LovyanGFX"
git clone -q --depth 1 --branch 1.2.30 https://github.com/lovyan03/LovyanGFX "$LIBDIR/LovyanGFX"
python3 -m pip install -q pyserial esptool || true
echo "Toolchain ready."
