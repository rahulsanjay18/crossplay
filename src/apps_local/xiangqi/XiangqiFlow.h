#pragma once

// Xiangqi's navigation and the state between two taps. Freestanding.
//
// Same discipline as Shogi's and Checkers': every mark the board draws is read
// out of the rules' own legal move list, so a marked point is always one the
// rules will accept.

#include <cstdint>

#include "XiangqiCore.h"

namespace xiangqi {

enum class Screen : uint8_t { Menu, HowTo, Board, Result };

constexpr Screen back(const Screen screen) {
  switch (screen) {
    case Screen::Menu:
      return Screen::Menu;
    case Screen::HowTo:
    case Screen::Board:
    case Screen::Result:
      return Screen::Menu;
  }
  return Screen::Menu;
}

constexpr bool leavesApp(const Screen screen) { return screen == Screen::Menu; }

enum class Opponent : uint8_t { Computer = 0, Friend = 1 };

constexpr int kNothingPicked = -1;

struct Targets {
  uint8_t to[kPoints];
  bool capture[kPoints];
  int count = 0;
};

inline void targetsFor(const Board& board, const MoveList& legal, const int from, Targets& out) {
  out.count = 0;
  if (from < 0) return;
  for (int i = 0; i < legal.count; ++i) {
    if (legal.moves[i].from != from) continue;
    out.to[out.count] = legal.moves[i].to;
    out.capture[out.count] = board.cell[legal.moves[i].to] != 0;
    ++out.count;
  }
}

inline bool canPick(const MoveList& legal, const int point) {
  for (int i = 0; i < legal.count; ++i) {
    if (legal.moves[i].from == point) return true;
  }
  return false;
}

inline bool moveBetween(const MoveList& legal, const int from, const int to, Move& out) {
  for (int i = 0; i < legal.count; ++i) {
    if (legal.moves[i].from != from || legal.moves[i].to != to) continue;
    out = legal.moves[i];
    return true;
  }
  return false;
}

enum class Ending : uint8_t { None, Checkmate, Stalemate, Repetition, NoCaptures, MoveLimit };

inline Ending endingOf(const Game& game, const Outcome result) {
  if (result == Outcome::Running) return Ending::None;
  if (result == Outcome::Draw) {
    if (repetitions(game) >= kRepetitionDraw) return Ending::Repetition;
    return game.idlePlies >= kIdleLimit ? Ending::NoCaptures : Ending::MoveLimit;
  }
  return inCheck(game.board, game.board.turn) ? Ending::Checkmate : Ending::Stalemate;
}

// Material each side still has on the board, by type, for the strip under the
// board: which pieces are gone is what a player scans for.
inline int countOf(const Board& board, const uint8_t seat, const uint8_t type) {
  int n = 0;
  for (int i = 0; i < kPoints; ++i) {
    if (board.cell[i] == makeCell(type, seat)) ++n;
  }
  return n;
}

}  // namespace xiangqi
