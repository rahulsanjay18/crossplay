#!/bin/sh
# Builds and runs the Shogi rules, engine, flow and save tests. No device and
# no PlatformIO: ShogiCore and ShogiEngine are freestanding C++17.
#
#   host-tests/shogi/run.sh
#
# -O2 because the Matsuri perft at depth 3 is 4.8 million nodes.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-shogi-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/shogi
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -I$SRC $SRC/ShogiCore.cpp $SRC/ShogiEngine.cpp test_shogi.cpp \
  -o "$BUILD_DIR/test_shogi"
"$BUILD_DIR/test_shogi"
