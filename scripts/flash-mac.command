#!/bin/bash
# Nova Arcade one-click flasher for ESP32-S3 (macOS)
cd "$(dirname "$0")"
exec > >(tee flash.log) 2>&1
echo "=== Nova Arcade flasher ==="
BIN="nova-arcade-full.bin"
PORT=$(ls /dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1)
if [ -z "$PORT" ]; then
  echo "RESULT: NO_BOARD - plug the board in with a data USB cable and run again."
  read -p "Press Enter to close"; exit 1
fi
echo "Using port: $PORT"
ESPTOOL=""
for c in ~/.espressif/python_env/*/bin/esptool.py \
         ~/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool \
         "$(command -v esptool.py)" "$(command -v esptool)"; do
  if [ -n "$c" ] && [ -x "$c" ]; then ESPTOOL="$c"; break; fi
done
if [ -z "$ESPTOOL" ]; then
  echo "No esptool found - installing a private copy..."
  python3 -m venv .venv && .venv/bin/pip install -q esptool && ESPTOOL=".venv/bin/esptool.py"
fi
echo "Using esptool: $ESPTOOL"
for BAUD in 921600 460800 115200; do
  echo "--- Flashing at $BAUD baud ---"
  if $ESPTOOL --chip esp32s3 --port "$PORT" --baud $BAUD --before default_reset --after hard_reset \
       write_flash 0x0 "$BIN"; then
    echo "RESULT: SUCCESS - the Nova Arcade menu should appear on the screen."
    read -t 15 -p "Press Enter to close (auto-closes in 15s)"; exit 0
  fi
done
echo "RESULT: FAILED - hold BOOT, tap RST, release BOOT, then run this again."
read -p "Press Enter to close"
