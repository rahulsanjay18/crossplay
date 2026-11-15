#pragma once

// A Chart laid out for a screen width: rows of chords and lyrics, and where
// each page starts. Freestanding, with measuring passed in, so host-tests/songs
// can check the wrapping rule: a chord and the lyric it sits over always move
// to the next line together.

#include <cstdint>
#include <string>
#include <vector>

#include "SongsCore.h"

namespace songs {

struct Measure {
  void* context = nullptr;
  int (*width)(void* context, bool chord, const std::string& text) = nullptr;
  int16_t chordHeight = 0;
  int16_t lyricHeight = 0;
};

struct Placed {
  std::string text;
  int16_t x = 0;
  bool chord = false;
};

enum class RowKind : uint8_t { Pair, Text, Section, Blank };

struct Row {
  RowKind kind = RowKind::Blank;
  int16_t height = 0;
  bool hasChords = false;  // a Pair row: its chords sit on a line above the lyric
  std::vector<Placed> items;
};

std::vector<Row> layout(const Song& song, const View& view, int width, const Measure& measure);

// The first row of each page. Page one starts at 0. A Section label is never
// left alone at the foot of a page.
std::vector<int> pageStarts(const std::vector<Row>& rows, int height);

}  // namespace songs
