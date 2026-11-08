# Build this tree for the reTerminal Sticky and flash it over USB, on Windows.
# flash-sticky.sh is the same script for Linux.
#
#   .\scripts_local\flash-sticky.ps1                   # build, find the board, flash
#   .\scripts_local\flash-sticky.ps1 -Port COM5
#   .\scripts_local\flash-sticky.ps1 -PioEnv sticky      # the dev build (default gh_release_sticky)
#   .\scripts_local\flash-sticky.ps1 -SkipBuild        # flash what is already built
#   .\scripts_local\flash-sticky.ps1 -BuildOnly
#
# Writes bootloader, partition table, blank otadata and app, and leaves NVS
# alone. Blank otadata matters: a device that last took an OTA runs app1, and
# without it the bootloader goes on booting the old firmware from there.
#
# A port is chosen only if exactly one Espressif native-USB device (VID 303A)
# is attached. With more than one, name it with -Port.
param(
  [string]$Port = "",
  [string]$PioEnv = "gh_release_sticky",
  [switch]$SkipBuild,
  [switch]$BuildOnly
)
$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $PSScriptRoot
$PioHome = Join-Path $env:USERPROFILE ".platformio"
$PenvPio = Join-Path $PioHome "penv\Scripts\pio.exe"

$Pio = (Get-Command pio -ErrorAction SilentlyContinue).Source
if (-not $Pio -and (Test-Path $PenvPio)) { $Pio = $PenvPio }
if (-not $Pio) { throw "PlatformIO not found; py -m pip install platformio" }

if (-not $SkipBuild) {
  & $Pio run -d $Repo -e $PioEnv
  if ($LASTEXITCODE -ne 0) { throw "build failed" }
}
if ($BuildOnly) { exit 0 }

$Build = Join-Path $Repo ".pio\build\$PioEnv"
foreach ($f in "bootloader.bin", "partitions.bin", "firmware.bin") {
  if (-not (Test-Path (Join-Path $Build $f))) { throw "no $Build\$f; run without -SkipBuild" }
}

if (-not $Port) {
  $found = @(& $Pio device list --serial --json-output | ConvertFrom-Json |
    Where-Object { $_.hwid -match "VID:PID=303A" } | ForEach-Object { $_.port })
  if ($found.Count -eq 0) { throw "no Espressif USB device found. Check the cable: a charge-only one shows no port." }
  if ($found.Count -gt 1) { throw "$($found.Count) devices found ($($found -join ', ')); choose one with -Port." }
  $Port = $found[0]
}

$Esptool = Join-Path $PioHome "packages\tool-esptoolpy\esptool.py"
$PioPy = Join-Path $PioHome "penv\Scripts\python.exe"
if (-not ((Test-Path $Esptool) -and (Test-Path $PioPy))) { throw "esptool missing; run a build first" }

$OtaBlank = [System.IO.Path]::GetTempFileName()
try {
  [System.IO.File]::WriteAllBytes($OtaBlank, [byte[]](,0xFF * 0x2000))
  Write-Host "flashing $PioEnv to $Port"
  & $PioPy $Esptool --chip esp32s3 --port $Port --baud 921600 `
    --before usb-reset --after hard-reset write-flash `
    0x0 (Join-Path $Build "bootloader.bin") 0x8000 (Join-Path $Build "partitions.bin") `
    0xe000 $OtaBlank 0x10000 (Join-Path $Build "firmware.bin")
  if ($LASTEXITCODE -ne 0) {
    throw "the write failed. If it says no serial data was received, hold BOOT while tapping RESET and retry."
  }
} finally {
  Remove-Item -Force $OtaBlank -ErrorAction SilentlyContinue
}
Write-Host "flashed $PioEnv to $Port"
