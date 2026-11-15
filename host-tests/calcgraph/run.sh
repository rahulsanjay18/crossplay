#!/bin/sh
# Builds and runs the graphing calculator's expression and graph tests.
#
#   host-tests/calcgraph/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
SRC=../../src/apps_local/calculator
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_calcgraph.cpp $SRC/CalcExpr.cpp $SRC/CalcGraph.cpp \
  -o "$BUILD_DIR/test_calcgraph"
"$BUILD_DIR/test_calcgraph"
