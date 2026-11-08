#pragma once

// The built-in Xiangqi opponent: iterative-deepening alpha-beta with a capture
// quiescence search, bounded by a deterministic node budget. Freestanding and
// copy-make, the same shape as ShogiEngine; see that header for why.

#include <cstdint>

#include "XiangqiCore.h"

namespace xiangqi {

constexpr int kMateScore = 30000;
constexpr int kMateThreshold = kMateScore - 1000;

constexpr int kMaxSearchDepth = 5;
constexpr int kMaxQuiescence = 6;

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

int pieceValue(uint8_t type);

// Material plus small positional terms, from the side-to-move's view.
int evaluate(const Board& board);

// Best move of the deepest completed iteration up to `maxDepth` within
// `nodeBudget` nodes (0 = unbounded). hasMove is false only with no legal move.
SearchResult search(const Board& board, int maxDepth, SearchBuffers& buffers, uint32_t nodeBudget);

}  // namespace xiangqi
