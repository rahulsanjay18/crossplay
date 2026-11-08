#pragma once

// The built-in Shogi opponent: iterative-deepening alpha-beta over ShogiCore,
// with a capture-only quiescence search so it does not stop counting in the
// middle of an exchange.
//
// Freestanding like the rules, so it is host-tested. Bounded by a NODE budget
// rather than a clock: node counts are deterministic, so a test can assert on
// them and the same budget plays the same move on every device -- which also
// keeps a position reconstructible.
//
// Copy-make throughout. A Board is 98 bytes, so copying one per node is cheaper
// than an unmake that has to remember hand counts and promotions, and it means
// the search can never hand back a board it forgot to restore.

#include <cstdint>

#include "ShogiCore.h"

namespace shogi {

constexpr int kMateScore = 30000;
constexpr int kMateThreshold = kMateScore - 1000;

// Main-search plies, and quiescence plies beyond them. Each costs one 2.4KB
// MoveList in SearchBuffers.
constexpr int kMaxSearchDepth = 4;
constexpr int kMaxQuiescence = 4;

// Owned by the caller (the activity holds one as a member), so the engine
// allocates nothing and nothing large lands on a task stack.
struct SearchBuffers {
  MoveList plies[kMaxSearchDepth + kMaxQuiescence + 1];
  int scores[kMaxMoves];
};

struct SearchResult {
  Move best{};
  int score = 0;
  uint32_t nodes = 0;
  int depthCompleted = 0;
  bool budgetExhausted = false;
  bool hasMove = false;
};

// Material and a little king shelter, in centipawn-like units, from the
// side-to-move's point of view.
int evaluate(const Board& board);

// The value of one piece of `type` on the board.
int pieceValue(uint8_t type);

// Best move for the side to move, searching depth 1, 2, ... up to `maxDepth`
// while `nodeBudget` lasts (0 means no budget). Returns the best move of the
// deepest COMPLETED iteration, so running out of budget never returns a move
// that was only half considered. hasMove is false only when there is no legal
// move at all.
SearchResult search(const Board& board, int maxDepth, SearchBuffers& buffers, uint32_t nodeBudget);

}  // namespace shogi
