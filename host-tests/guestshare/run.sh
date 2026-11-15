#!/bin/sh
# Builds and runs the guest file share rules. No device and no PlatformIO.
#
#   host-tests/guestshare/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -I../../src/network test_guestshare.cpp -o "$BUILD_DIR/test_guestshare"
"$BUILD_DIR/test_guestshare"

# The guest surface must register only its own routes.
SOURCE=../../src/network/CrossPointWebServer.cpp
ROUTES=$(awk '/if \(isGuest\(\)\) \{/{found=1} found{print} found&&/^  }/{exit}' "$SOURCE" | grep -c 'server->on(')
if [ "$ROUTES" -ne 3 ]; then
  echo "FAIL guestshare: guest surface registers $ROUTES routes, expected 3"
  exit 1
fi
if ! grep -q 'if (isFull()) server->enableCORS(true);' "$SOURCE"; then
  echo "FAIL guestshare: CORS must stay on the full surface only"
  exit 1
fi
echo "guestshare routes: ok"
