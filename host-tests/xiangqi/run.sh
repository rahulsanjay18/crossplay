#!/bin/sh
# Builds and runs the Xiangqi rules, engine, flow and save tests. No device and
# no PlatformIO: XiangqiCore and XiangqiEngine are freestanding C++17.
#
#   host-tests/xiangqi/run.sh
#
# -O2 because the opening perft at depth 4 is 3.3 million nodes.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-xiangqi-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/xiangqi
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -I$SRC $SRC/XiangqiCore.cpp $SRC/XiangqiEngine.cpp test_xiangqi.cpp \
  -o "$BUILD_DIR/test_xiangqi"
"$BUILD_DIR/test_xiangqi"
