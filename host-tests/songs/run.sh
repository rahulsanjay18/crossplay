#!/bin/sh
# Builds and runs the chord chart tests. SongsCore is freestanding C++17.
#
#   host-tests/songs/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
SRC=../../src/apps_local/songs
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC test_songs.cpp $SRC/SongsCore.cpp $SRC/SongsLayout.cpp -o "$BUILD_DIR/test_songs"
"$BUILD_DIR/test_songs"
