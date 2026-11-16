#pragma once

// The graphing mode's keypad and the line being typed. Freestanding, so
// host-tests/calcgraph checks that every row fills its grid and that editing
// keeps the cursor honest.

#include <cstddef>
#include <cstdint>
#include <string>

#include "CalcLayout.h"

namespace calc {
namespace ti {

enum class Action : uint8_t {
  Insert,  // `insert` goes in at the cursor
  Enter,
  Delete,
  Clear,
  Left,
  Right,
  YEquals,
  Graph,
  Table,
  Angle,
  Keyboard,
  Basic,
  Store,
};

struct Key {
  const char* label;
  Action action;
  const char* insert;  // Insert only
  uint8_t span;        // columns, so ENTER can be wide
};

constexpr int kColumns = 6;

// clang-format off
constexpr Key kKeys[] = {
    {"Y=", Action::YEquals, nullptr, 1}, {"GRAPH", Action::Graph, nullptr, 1}, {"TABLE", Action::Table, nullptr, 1},
    {"DEG", Action::Angle, nullptr, 1},  {"ABC", Action::Keyboard, nullptr, 1}, {"BASIC", Action::Basic, nullptr, 1},

    {"sin", Action::Insert, "sin(", 1},   {"cos", Action::Insert, "cos(", 1},   {"tan", Action::Insert, "tan(", 1},
    {"ln", Action::Insert, "ln(", 1},     {"log", Action::Insert, "log(", 1},   {"^", Action::Insert, "^", 1},

    {"asin", Action::Insert, "asin(", 1}, {"acos", Action::Insert, "acos(", 1}, {"atan", Action::Insert, "atan(", 1},
    {"sqrt", Action::Insert, "sqrt(", 1}, {"x^2", Action::Insert, "^2", 1},     {"!", Action::Insert, "!", 1},

    {"(", Action::Insert, "(", 1},        {")", Action::Insert, ")", 1},        {"pi", Action::Insert, "pi", 1},
    {"e", Action::Insert, "e", 1},        {"X", Action::Insert, "X", 1},        {"STO", Action::Store, nullptr, 1},

    {"7", Action::Insert, "7", 1},        {"8", Action::Insert, "8", 1},        {"9", Action::Insert, "9", 1},
    {kDiv, Action::Insert, "/", 1},       {"DEL", Action::Delete, nullptr, 1},  {"CLR", Action::Clear, nullptr, 1},

    {"4", Action::Insert, "4", 1},        {"5", Action::Insert, "5", 1},        {"6", Action::Insert, "6", 1},
    {kMul, Action::Insert, "*", 1},       {"ANS", Action::Insert, "Ans", 1},    {"E", Action::Insert, "E", 1},

    {"1", Action::Insert, "1", 1},        {"2", Action::Insert, "2", 1},        {"3", Action::Insert, "3", 1},
    {kMinus, Action::Insert, "-", 1},     {"<", Action::Left, nullptr, 1},      {">", Action::Right, nullptr, 1},

    {"0", Action::Insert, "0", 1},        {".", Action::Insert, ".", 1},        {"+", Action::Insert, "+", 1},
    {"ENTER", Action::Enter, nullptr, 3},
};
// clang-format on

constexpr int kKeyCount = static_cast<int>(sizeof(kKeys) / sizeof(kKeys[0]));

constexpr int rowCount() {
  int units = 0;
  for (const Key& key : kKeys) units += key.span;
  return units / kColumns;
}

constexpr bool rowsAreFull() {
  int units = 0;
  for (const Key& key : kKeys) {
    if (units % kColumns + key.span > kColumns) return false;  // a key may not wrap
    units += key.span;
  }
  return units % kColumns == 0;
}
static_assert(rowsAreFull(), "every keypad row must hold exactly kColumns units");

constexpr int kRows = rowCount();

// The cell a key starts in, counted in units.
inline int keyStart(const int index) {
  int units = 0;
  for (int i = 0; i < index; i++) units += kKeys[i].span;
  return units;
}

inline Rect16 keyRect(const Rect16& grid, const int16_t gap, const int index) {
  const int16_t cellW = static_cast<int16_t>((grid.w - (kColumns - 1) * gap) / kColumns);
  const int16_t cellH = static_cast<int16_t>((grid.h - (kRows - 1) * gap) / kRows);
  const int start = keyStart(index);
  const int column = start % kColumns;
  const int row = start / kColumns;
  const int span = kKeys[index].span;
  return Rect16{static_cast<int16_t>(grid.x + column * (cellW + gap)), static_cast<int16_t>(grid.y + row * (cellH + gap)),
                static_cast<int16_t>(span * cellW + (span - 1) * gap), cellH};
}

inline int keyAt(const Rect16& grid, const int16_t gap, const int x, const int y) {
  for (int i = 0; i < kKeyCount; i++) {
    const Rect16 r = keyRect(grid, gap, i);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) return i;
  }
  return -1;
}

// The line being typed. ASCII only: every key inserts ASCII.
struct Entry {
  std::string text;
  size_t cursor = 0;

  void set(const std::string& value) {
    text = value;
    cursor = text.size();
  }
  void insert(const char* value) {
    if (cursor > text.size()) cursor = text.size();
    const std::string piece = value != nullptr ? value : "";
    text.insert(cursor, piece);
    cursor += piece.size();
  }
  void backspace() {
    if (cursor == 0 || text.empty()) return;
    if (cursor > text.size()) cursor = text.size();
    // Function names went in whole, so they come out whole.
    static constexpr const char* kWhole[] = {"asin(", "acos(", "atan(", "sqrt(", "sin(", "cos(",
                                             "tan(",  "log(",  "ln(",   "Ans",   "pi"};
    for (const char* whole : kWhole) {
      const std::string piece(whole);
      if (cursor >= piece.size() && text.compare(cursor - piece.size(), piece.size(), piece) == 0) {
        text.erase(cursor - piece.size(), piece.size());
        cursor -= piece.size();
        return;
      }
    }
    text.erase(cursor - 1, 1);
    cursor--;
  }
  void left() {
    if (cursor > 0) cursor--;
  }
  void right() {
    if (cursor < text.size()) cursor++;
  }
  void clear() {
    text.clear();
    cursor = 0;
  }
};

}  // namespace ti
}  // namespace calc
