#!/bin/bash
# Runs scripts_local/flash-sticky.sh against a fake pio and esptool. No device
# and no PlatformIO. What it pins: the regions written (NVS must stay out of
# them, otadata must stay in), the reset mode, and when a port may be guessed.
#
#   host-tests/flashsticky/run.sh
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$ROOT/scripts_local/flash-sticky.sh"
PS1="$ROOT/scripts_local/flash-sticky.ps1"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

checks=0
failed=0
check() {  # description, condition already evaluated into $?
  local status=$?
  checks=$((checks + 1))
  if [ "$status" -ne 0 ]; then
    failed=$((failed + 1))
    echo "FAIL $1"
  fi
}

# The script resolves itself to find the repo, so it runs from a copy whose
# .pio holds empty images.
REPO="$WORK/repo"
mkdir -p "$REPO/scripts_local" "$REPO/.pio/build/gh_release_sticky"
cp "$SCRIPT" "$REPO/scripts_local/"
touch "$REPO"/.pio/build/gh_release_sticky/{bootloader,partitions,firmware}.bin

export HOME="$WORK/home"
mkdir -p "$HOME/.platformio/packages/tool-esptoolpy" "$HOME/.platformio/penv/bin" "$WORK/bin"
touch "$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
cat > "$HOME/.platformio/penv/bin/python" <<'EOF'
#!/bin/bash
shift
echo "$@" > "$FAKE_LOG"
exit "${FAKE_ESPTOOL_RC:-0}"
EOF
cat > "$WORK/bin/pio" <<'EOF'
#!/bin/bash
if [ "$1" = "run" ]; then echo run >> "$FAKE_BUILDS"; exit "${FAKE_BUILD_RC:-0}"; fi
cat "$FAKE_PORTS"
EOF
chmod +x "$HOME/.platformio/penv/bin/python" "$WORK/bin/pio"
export PATH="$WORK/bin:$PATH" FAKE_LOG="$WORK/esptool.log" FAKE_BUILDS="$WORK/builds" FAKE_PORTS="$WORK/ports"

one='[{"port":"/dev/ttyACM7","hwid":"USB VID:PID=303A:1001 SER=X"},{"port":"/dev/ttyS0","hwid":"n/a"}]'
two='[{"port":"/dev/ttyACM7","hwid":"USB VID:PID=303a:1001"},{"port":"/dev/ttyACM8","hwid":"USB VID:PID=303A:1001"}]'
none='[{"port":"/dev/ttyS0","hwid":"n/a"}]'

run() { rm -f "$FAKE_LOG" "$FAKE_BUILDS"; OUT="$("$REPO/scripts_local/flash-sticky.sh" "$@" 2>&1)"; RC=$?; }

echo "$one" > "$FAKE_PORTS"
run
[ "$RC" -eq 0 ]; check "flashes with one Espressif device: $OUT"
[ -f "$FAKE_BUILDS" ]; check "builds by default"
grep -q -- '--port /dev/ttyACM7 ' "$FAKE_LOG"; check "picks the only Espressif port"
grep -q -- '--chip esp32s3 ' "$FAKE_LOG"; check "targets the S3"
grep -q -- '--before usb-reset ' "$FAKE_LOG"; check "uses the native-USB reset"
for region in '0x0 ' '0x8000 ' '0xe000 ' '0x10000 '; do
  grep -q -- " $region" "$FAKE_LOG"; check "writes $region"
done
! grep -q -- ' 0x9000 ' "$FAKE_LOG"; check "leaves NVS alone"

run --skip-build
[ ! -f "$FAKE_BUILDS" ]; check "--skip-build does not build"
[ -f "$FAKE_LOG" ]; check "--skip-build still flashes"

run --build-only
[ -f "$FAKE_BUILDS" ]; check "--build-only builds"
[ ! -f "$FAKE_LOG" ]; check "--build-only does not flash"

run --port /dev/ttyNAMED
grep -q -- '--port /dev/ttyNAMED ' "$FAKE_LOG"; check "--port wins over detection"

echo "$two" > "$FAKE_PORTS"
run
[ "$RC" -ne 0 ] && [ ! -f "$FAKE_LOG" ]; check "refuses to guess between two devices"
run --port /dev/ttyACM8
[ "$RC" -eq 0 ]; check "two devices are fine with --port"

echo "$none" > "$FAKE_PORTS"
run
[ "$RC" -ne 0 ] && [ ! -f "$FAKE_LOG" ]; check "no device is an error"

echo "$one" > "$FAKE_PORTS"
FAKE_BUILD_RC=1 run
[ "$RC" -ne 0 ] && [ ! -f "$FAKE_LOG" ]; check "a failed build never flashes"
FAKE_ESPTOOL_RC=1 run
[ "$RC" -ne 0 ]; check "a failed write is reported"

run --bogus
[ "$RC" -eq 2 ]; check "an unknown flag is refused"

# PowerShell is not on every host, so the Windows twin is held to the same
# contract by reading it.
for needle in '--before usb-reset' '0x0 ' '0x8000 ' '0xe000 ' '0x10000 ' 'VID:PID=303A' 'gh_release_sticky'; do
  grep -qF -- "$needle" "$PS1"; check "flash-sticky.ps1 carries $needle"
done
! grep -qF -- '0x9000' "$PS1"; check "flash-sticky.ps1 leaves NVS alone"

echo "$checks checks, $failed failed"
[ "$failed" -eq 0 ]
