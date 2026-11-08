#!/bin/bash
# Build this tree for the reTerminal Sticky and flash it over USB, on Linux.
# flash-sticky.ps1 is the same script for Windows.
#
#   ./scripts_local/flash-sticky.sh                    # build, find the board, flash
#   ./scripts_local/flash-sticky.sh --port /dev/ttyACM0
#   ./scripts_local/flash-sticky.sh --env sticky       # the dev build (default gh_release_sticky)
#   ./scripts_local/flash-sticky.sh --skip-build       # flash what is already built
#   ./scripts_local/flash-sticky.sh --build-only
#
# Writes bootloader, partition table, blank otadata and app, and leaves NVS
# alone. Blank otadata matters: a device that last took an OTA runs app1, and
# without it the bootloader goes on booting the old firmware from there.
#
# A port is chosen only if exactly one Espressif native-USB device (VID 303A)
# is attached. With more than one, name it with --port.
set -uo pipefail

REPO="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
ENV_NAME="gh_release_sticky"
PORT=""
SKIP_BUILD=0
BUILD_ONLY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --env) ENV_NAME="${2:?--env needs a name}"; shift 2 ;;
    --port) PORT="${2:?--port needs a device}"; shift 2 ;;
    --skip-build) SKIP_BUILD=1; shift ;;
    --build-only) BUILD_ONLY=1; shift ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    *) echo "flash-sticky: unknown flag $1" >&2; exit 2 ;;
  esac
done

PIO="$(command -v pio || true)"
for candidate in "$HOME/.platformio/penv/bin/pio" "$HOME/.local/share/pio-venv/bin/pio"; do
  [ -z "$PIO" ] && [ -x "$candidate" ] && PIO="$candidate"
done
[ -n "$PIO" ] || { echo "error: PlatformIO not found; pip install platformio" >&2; exit 1; }

if [ "$SKIP_BUILD" = "0" ]; then
  "$PIO" run -d "$REPO" -e "$ENV_NAME" || { echo "error: build failed" >&2; exit 1; }
fi
[ "$BUILD_ONLY" = "1" ] && exit 0

BUILD="$REPO/.pio/build/$ENV_NAME"
for f in bootloader.bin partitions.bin firmware.bin; do
  [ -f "$BUILD/$f" ] || { echo "error: no $BUILD/$f; run without --skip-build" >&2; exit 1; }
done

if [ -z "$PORT" ]; then
  mapfile -t FOUND < <("$PIO" device list --serial --json-output 2>/dev/null | python3 -c '
import json, sys
for p in json.load(sys.stdin):
    if "VID:PID=303A" in p.get("hwid", "").upper():
        print(p["port"])
')
  if [ "${#FOUND[@]}" -eq 0 ]; then
    echo "error: no Espressif USB device found. Check the cable: a charge-only one shows no port." >&2
    grep -qi microsoft /proc/version 2>/dev/null &&
      echo "       WSL cannot see USB by default; run flash-sticky.ps1 from Windows instead." >&2
    exit 1
  fi
  if [ "${#FOUND[@]}" -gt 1 ]; then
    echo "error: ${#FOUND[@]} devices found (${FOUND[*]}); choose one with --port." >&2
    exit 1
  fi
  PORT="${FOUND[0]}"
fi

if [ -e "$PORT" ] && [ ! -w "$PORT" ]; then
  echo "error: no write access to $PORT. Fix with: sudo usermod -aG dialout $USER (then log in again)" >&2
  exit 1
fi

ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
PIO_PY="$HOME/.platformio/penv/bin/python"
[ -f "$ESPTOOL" ] && [ -x "$PIO_PY" ] || { echo "error: esptool missing; run a build first" >&2; exit 1; }

OTA_BLANK="$(mktemp)"
trap 'rm -f "$OTA_BLANK"' EXIT
python3 -c "import sys; open(sys.argv[1],'wb').write(b'\xff' * 0x2000)" "$OTA_BLANK"

echo "flashing $ENV_NAME to $PORT"
if ! "$PIO_PY" "$ESPTOOL" --chip esp32s3 --port "$PORT" --baud 921600 \
    --before usb-reset --after hard-reset write-flash \
    0x0 "$BUILD/bootloader.bin" 0x8000 "$BUILD/partitions.bin" \
    0xe000 "$OTA_BLANK" 0x10000 "$BUILD/firmware.bin"; then
  echo "error: the write failed. If it says no serial data was received, hold BOOT while tapping RESET and retry." >&2
  exit 1
fi
echo "flashed $ENV_NAME to $PORT"
