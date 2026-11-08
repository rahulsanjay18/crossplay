#pragma once

// Shogi's navigation, and the state that lives between two taps. Freestanding.
//
// A shogi move is two taps -- a piece (on the board OR in your hand) and then a
// destination -- and sometimes three, when the rules let the piece choose
// whether to promote. Everything here reads the rules' own legal move list
// rather than reasoning about pieces, so a square the UI marks is always a
// square the rules will accept, and a promotion question is only ever asked
// when both answers are legal.

#include <cstdint>

#include "ShogiCore.h"

namespace shogi {

enum class Screen : uint8_t {
  // The top. Back from here leaves the app, and it is the only screen that does.
  Menu,
  HowTo,
  Board,
  Result,
};

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

// Who the other side is. A link match is not a mode here: LinkActivity owns it
// and the activity asks inMatch().
enum class Opponent : uint8_t { Computer = 0, Friend = 1 };

// What is in hand between the first tap and the second.
struct Pick {
  // kNoPick, a board square, or kDropFrom with `drop` naming the hand piece.
  uint8_t from = kNoPick;
  uint8_t drop = None;

  static constexpr uint8_t kNoPick = 0xFD;
  bool any() const { return from != kNoPick; }
  bool fromHand() const { return from == kDropFrom; }
};

// How a landing square relates to promotion.
enum class Promotion : uint8_t {
  None = 0,      // only the unpromoted move exists
  Optional = 1,  // both exist: the player is asked
  Forced = 2,    // only the promoted move exists
};

struct Targets {
  uint8_t to[kCells];
  Promotion promotion[kCells];
  bool capture[kCells];
  int count = 0;
};

inline bool picks(const Pick& pick, const Move& move) {
  if (pick.fromHand()) return isDrop(move) && move.drop == pick.drop;
  return !isDrop(move) && move.from == pick.from;
}

// Every square the picked piece may land on, deduplicated, with whether that
// landing promotes. Derived from the legal list, never recomputed.
inline void targetsFor(const Board& board, const MoveList& legal, const Pick& pick, Targets& out) {
  out.count = 0;
  if (!pick.any()) return;
  // Per square: bit 0 = an unpromoted move exists, bit 1 = a promoted one.
  uint8_t seen[kCells] = {};
  for (int i = 0; i < legal.count; ++i) {
    const Move& move = legal.moves[i];
    if (!picks(pick, move)) continue;
    seen[move.to] = static_cast<uint8_t>(seen[move.to] | (move.promote != 0 ? 2 : 1));
  }
  for (int square = 0; square < kCells; ++square) {
    if (seen[square] == 0) continue;
    out.to[out.count] = static_cast<uint8_t>(square);
    out.promotion[out.count] =
        seen[square] == 3 ? Promotion::Optional : (seen[square] == 2 ? Promotion::Forced : Promotion::None);
    out.capture[out.count] = board.cell[square] != 0;
    ++out.count;
  }
}

// How a landing on `to` stands on promotion, or false when it is not a target.
inline bool promotionAt(const Targets& targets, const int to, Promotion& out) {
  for (int i = 0; i < targets.count; ++i) {
    if (targets.to[i] != to) continue;
    out = targets.promotion[i];
    return true;
  }
  return false;
}

// The legal move for this pick, destination and promotion answer, or false.
// The caller never builds a Move itself, so a move the rules did not offer
// cannot be expressed at the boundary.
inline bool moveFor(const MoveList& legal, const Pick& pick, const int to, const bool promote, Move& out) {
  for (int i = 0; i < legal.count; ++i) {
    const Move& move = legal.moves[i];
    if (!picks(pick, move) || move.to != to) continue;
    if ((move.promote != 0) != promote) continue;
    out = move;
    return true;
  }
  return false;
}

// Whether the piece on `square` (or the hand piece `drop`) has any legal move.
// A piece that cannot move cannot be lifted, so a tap on it changes nothing.
inline bool canPickSquare(const MoveList& legal, const int square) {
  for (int i = 0; i < legal.count; ++i) {
    if (!isDrop(legal.moves[i]) && legal.moves[i].from == square) return true;
  }
  return false;
}

inline bool canPickHand(const MoveList& legal, const uint8_t drop) {
  for (int i = 0; i < legal.count; ++i) {
    if (isDrop(legal.moves[i]) && legal.moves[i].drop == drop) return true;
  }
  return false;
}

// The hand strip's order, most valuable first, as printed sets lay them out.
constexpr uint8_t kHandOrder[7] = {Rook, Bishop, Gold, Silver, Knight, Lance, Pawn};
constexpr int kHandOrderCount = 7;

// How the game ended, for the result screen's second line.
enum class Ending : uint8_t { None, Checkmate, NoMoves, Repetition, MoveLimit };

inline Ending endingOf(const Game& game, const Outcome result) {
  if (result == Outcome::Running) return Ending::None;
  if (result == Outcome::Draw) return game.plies >= kMaxPlies ? Ending::MoveLimit : Ending::Repetition;
  return inCheck(game.board, game.board.turn) ? Ending::Checkmate : Ending::NoMoves;
}

}  // namespace shogi
