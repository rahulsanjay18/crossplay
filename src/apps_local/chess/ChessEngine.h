#pragma once

// Chess opponent: alpha-beta search over ChessCore.
//
// Freestanding like ChessCore, so the whole thing is host-testable. No heap, no
// clock, no RTOS. Search is bounded by a node budget rather than a wall-clock
// deadline: node counts are deterministic, so a test can assert on them and the
// same budget behaves identically on the C3 and the S3.

#include <cstdint>

#include "ChessCore.h"

namespace chess {

// Scores are centipawns from the side-to-move's point of view.
constexpr int kMateScore = 30000;
// Beyond any plausible material score, so "mate in n" always beats material.
constexpr int kMateThreshold = kMateScore - 1000;

// Each ply costs a ~1KB MoveList in SearchBuffers, and this move generator is
// slow enough that depth 4 is already a second or more on a 160MHz C3, so
// there is nothing to gain from a deeper cap.
constexpr int kMaxSearchDepth = 4;

// Captures searched past the horizon, for the styles that ask for it.
constexpr int kMaxQuiescence = 4;

// One MoveList per ply, ~1KB each. Far too big for a stack frame on the C3, so
// the caller owns it: the activity holds one as a member. Handing the search a
// pre-allocated buffer is also what keeps the engine allocation-free.
struct SearchBuffers {
  MoveList plies[kMaxSearchDepth + kMaxQuiescence + 1];
  int rootScores[kMaxMoves];
};

// How one level plays. A margin of 0 always plays the best move found.
struct PlayStyle {
  const char* name;
  int depth;
  uint32_t nodeBudget;
  // Any root move within this many centipawns of the best may be played, the
  // closer the likelier.
  int marginCp;
  // Chance per move of playing with three times the margin: a lapse.
  uint8_t lapsePercent;
  bool quiescence;
};

// Ordered and spaced by tournament between neighbours. The two widest steps,
// EASY to CASUAL and SOLID to TRICKY, are where a full extra ply arrives.
constexpr int kLevelCount = 10;
constexpr int kDefaultLevel = 6;
constexpr PlayStyle kLevels[kLevelCount] = {
    {"BEGINNER", 1, 60000, 150, 25, false}, {"EASY", 1, 60000, 0, 0, false},  {"CASUAL", 2, 60000, 250, 25, false},
    {"STEADY", 2, 60000, 80, 15, false},    {"SOLID", 2, 60000, 0, 0, false}, {"TRICKY", 3, 60000, 80, 15, false},
    {"NORMAL", 3, 60000, 0, 0, false},      {"HARD", 4, 60000, 0, 0, false},  {"STRONG", 3, 60000, 0, 0, true},
    {"EXPERT", 4, 120000, 0, 0, true},
};

// Settings files once held 0, 1 or 2 for EASY, NORMAL and HARD. Levels are
// stored offset so those files keep meaning what they meant.
constexpr int kStoredLevelBase = 10;
constexpr int levelFromStored(const int stored) {
  if (stored >= kStoredLevelBase && stored < kStoredLevelBase + kLevelCount) return stored - kStoredLevelBase;
  if (stored == 0) return 1;
  if (stored == 2) return 7;
  return kDefaultLevel;
}

struct SearchResult {
  Move best{};
  int score = 0;
  uint32_t nodes = 0;
  // True when the node budget stopped the search early. The move is still
  // legal and still the best found so far, just less considered.
  bool budgetExhausted = false;
  bool hasMove = false;
};

// Static evaluation in centipawns, positive when the side to move is better.
int evaluate(const Position& position);

// Searches to `depth` and returns the best move for the side to move.
// `nodeBudget` caps the work; pass 0 for no cap. Returns hasMove == false only
// when the position has no legal moves (checkmate or stalemate).
SearchResult search(Position& position, int depth, SearchBuffers& buffers, uint32_t nodeBudget = 0);

// Plays `style`. `seed` decides between moves the style considers acceptable,
// so the same seed and position always give the same move.
SearchResult searchStyled(Position& position, const PlayStyle& style, SearchBuffers& buffers, uint32_t seed);

}  // namespace chess
