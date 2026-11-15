#!/bin/bash
# Generates the cuts Markdown notes need: italic, code, and math.
#
#   ./tools_local/toybox/gen_notes_fonts.sh
#
# Math comes from DejaVu Sans because it carries Greek and the math operators in
# one face. Each cut is subset to what the notes renderer can emit.
set -euo pipefail
cd "$(dirname "$0")/../.."
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
OUT=src/apps_local/ui/fonts
ASCII="U+0020-007E"
MATH="$ASCII,U+00B0,U+00B1,U+00B7,U+00D7,U+00F7,U+0391-03A9,U+03B1-03C9,U+2026,U+2070,U+00B9,U+00B2,U+00B3,U+2074-207E,U+207F,U+2080-208E,U+2190,U+2192,U+21D2,U+21D4,U+2200,U+2202,U+2203,U+2207,U+2208,U+2209,U+220F,U+2211,U+221A,U+221E,U+2229,U+222A,U+222B,U+2248,U+2260,U+2261,U+2264,U+2265,U+2282"

base="https://github.com/google/fonts/raw/main/ofl/notoserif"
curl -fsSL "$base/NotoSerif-Italic%5Bwdth,wght%5D.ttf" -o "$WORK/italic.ttf"
curl -fsSL "$base/NotoSerif%5Bwdth,wght%5D.ttf" -o "$WORK/upright.ttf"
uv run --quiet --with fonttools fonttools varLib.instancer "$WORK/upright.ttf" wght=700 wdth=100 -o "$WORK/bold.ttf"

cut() {
  local name="$1" size="$2" source="$3" unicodes="$4"
  shift 4
  uv run --quiet --with fonttools pyftsubset "$source" --unicodes="$unicodes" --output-file="$WORK/$name.ttf"
  uv run --quiet --with freetype-py --with fonttools \
    python lib/EpdFont/scripts/fontconvert.py "$name" "$size" "$WORK/$name.ttf" --force-autohint "$@" 2>/dev/null | grep -v "extracted$" > "$OUT/$name.h"
}

cut notes_italic_14 14 "$WORK/italic.ttf" "$ASCII,U+00A0-00FF"
cut notes_bold_14 14 "$WORK/bold.ttf" "$ASCII,U+00A0-00FF"
EXTRA=(--additional-intervals 0x0370,0x03FF --additional-intervals 0x2070,0x209F --additional-intervals 0x2190,0x22FF)
cut notes_math_14 14 /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf "$MATH" "${EXTRA[@]}"
cut notes_math_10 10 /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf "$MATH" "${EXTRA[@]}"
cut notes_code_12 12 /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf "$ASCII"
