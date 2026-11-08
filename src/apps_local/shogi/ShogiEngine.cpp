#include "ShogiEngine.h"

namespace shogi {

namespace {

// On the board. Roughly the values computer shogi converged on (Bonanza's
// order of magnitude), rounded: a dragon is worth more than a rook and a
// promoted minor piece about a gold.
constexpr int kBoardValue[16] = {
    0,     // None
    90,    // Pawn
    315,   // Lance
    405,   // Knight
    495,   // Silver
    540,   // Gold
    855,   // Bishop
    990,   // Rook
    0,     // King (never captured; mate is scored separately)
    540,   // Tokin
    540,   // PLance
    540,   // PKnight
    540,   // PSilver
    0,     // 13 does not exist
    945,   // Horse
    1395,  // Dragon
};

// In hand a piece is worth a little more than on the board: it can be dropped
// anywhere, which is the whole character of the game.
constexpr int kHandValue[8] = {0, 100, 350, 450, 550, 600, 950, 1100};

struct Context {
  SearchBuffers& buffers;
  uint32_t nodes = 0;
  uint32_t budget = 0;
  bool stopped = false;

  bool outOfBudget() {
    if (budget != 0 && nodes >= budget) stopped = true;
    return stopped;
  }
};

// Higher sorts first: captures by most valuable victim, least valuable
// attacker; then promotions; then quiet moves; drops last. Good ordering is
// what makes alpha-beta prune, and on this branching factor that is the
// difference between depth 2 and depth 3.
int orderScore(const Board& board, const Move& move, const Move* preferred) {
  if (preferred != nullptr && sameMove(move, *preferred)) return 1000000;
  if (isDrop(move)) return 0;
  const uint8_t victim = board.cell[move.to];
  const uint8_t mover = typeOf(board.cell[move.from]);
  int score = 100;
  if (victim != 0) score += 10000 + kBoardValue[typeOf(victim)] * 8 - kBoardValue[mover] / 16;
  if (move.promote != 0) score += 2000;
  return score;
}

void sortMoves(const Board& board, MoveList& list, int* scores, const Move* preferred) {
  for (int i = 0; i < list.count; ++i) scores[i] = orderScore(board, list.moves[i], preferred);
  // Insertion sort: stable (so equal scores keep generation order and the
  // engine stays deterministic) and cheap on lists this size.
  for (int i = 1; i < list.count; ++i) {
    const Move move = list.moves[i];
    const int score = scores[i];
    int j = i - 1;
    while (j >= 0 && scores[j] < score) {
      list.moves[j + 1] = list.moves[j];
      scores[j + 1] = scores[j];
      --j;
    }
    list.moves[j + 1] = move;
    scores[j + 1] = score;
  }
}

int quiesce(Context& ctx, const Board& board, int alpha, const int beta, const int qply, const int slot) {
  ++ctx.nodes;
  const int standPat = evaluate(board);
  if (qply >= kMaxQuiescence || ctx.outOfBudget()) return standPat;
  if (standPat >= beta) return standPat;
  if (standPat > alpha) alpha = standPat;

  MoveList& list = ctx.buffers.plies[slot];
  generatePseudo(board, list);
  // Captures only. Keep them in place, then order.
  int kept = 0;
  for (int i = 0; i < list.count; ++i) {
    const Move& m = list.moves[i];
    if (!isDrop(m) && board.cell[m.to] != 0) list.moves[kept++] = m;
  }
  list.count = kept;
  sortMoves(board, list, ctx.buffers.scores, nullptr);

  for (int i = 0; i < list.count; ++i) {
    const Move move = list.moves[i];
    Board next = board;
    makeMove(next, move);
    const int king = kingSquare(next, board.turn);
    if (king >= 0 && isAttacked(next, king, next.turn)) continue;
    const int score = -quiesce(ctx, next, -beta, -alpha, qply + 1, slot + 1);
    if (ctx.stopped) return alpha;
    if (score >= beta) return score;
    if (score > alpha) alpha = score;
  }
  return alpha;
}

int negamax(Context& ctx, const Board& board, const int depth, int alpha, const int beta, const int ply) {
  if (depth == 0) return quiesce(ctx, board, alpha, beta, 0, ply);
  ++ctx.nodes;
  if (ctx.outOfBudget()) return 0;

  MoveList& list = ctx.buffers.plies[ply];
  generatePseudo(board, list);
  sortMoves(board, list, ctx.buffers.scores, nullptr);

  bool any = false;
  int best = -kMateScore;
  for (int i = 0; i < list.count; ++i) {
    const Move move = list.moves[i];
    if (!isLegal(board, move)) continue;
    any = true;
    Board next = board;
    makeMove(next, move);
    const int score = -negamax(ctx, next, depth - 1, -beta, -alpha, ply + 1);
    if (ctx.stopped) return 0;
    if (score > best) best = score;
    if (score > alpha) alpha = score;
    if (alpha >= beta) break;
  }
  // No legal move loses in shogi, check or not. Nearer mates score higher.
  if (!any) return -kMateScore + ply;
  return best;
}

}  // namespace

int pieceValue(const uint8_t type) { return kBoardValue[type & kTypeMask]; }

int evaluate(const Board& board) {
  int score[2] = {0, 0};
  int kingAt[2] = {-1, -1};
  for (int i = 0; i < kCells; ++i) {
    const uint8_t cell = board.cell[i];
    if (cell == 0) continue;
    const uint8_t owner = ownerOf(cell);
    const uint8_t type = typeOf(cell);
    score[owner] += kBoardValue[type];
    if (type == King) kingAt[owner] = i;
    // A pawn or silver pressing forward is worth a few points more.
    if (type == Pawn || type == Silver) score[owner] += 3 * (kRanks - 1 - ranksToEdge(rankOf(i), owner));
  }
  for (int seat = 0; seat < 2; ++seat) {
    for (int t = Pawn; t <= Rook; ++t) score[seat] += kHandValue[t] * board.hand[seat][t];
  }
  // Shelter: golds and silvers next to their own king.
  for (int seat = 0; seat < 2; ++seat) {
    if (kingAt[seat] < 0) continue;
    const int kf = fileOf(kingAt[seat]);
    const int kr = rankOf(kingAt[seat]);
    for (int df = -1; df <= 1; ++df) {
      for (int dr = -1; dr <= 1; ++dr) {
        if (!inside(kf + df, kr + dr) || (df == 0 && dr == 0)) continue;
        const uint8_t cell = board.cell[indexOf(kf + df, kr + dr)];
        if (cell == 0 || ownerOf(cell) != seat) continue;
        const uint8_t type = typeOf(cell);
        if (type == Gold || type == Silver || (type >= Tokin && type <= PSilver)) score[seat] += 25;
      }
    }
  }
  const int mine = board.turn;
  return score[mine] - score[other(static_cast<uint8_t>(mine))];
}

SearchResult search(const Board& board, const int maxDepth, SearchBuffers& buffers, const uint32_t nodeBudget) {
  SearchResult result;
  Context ctx{buffers};
  ctx.budget = nodeBudget;

  // The root list lives in the last slot, which the search below never reaches
  // (main plies + quiescence plies < slot count), so iterating it is safe while
  // the children reuse the others.
  MoveList& root = buffers.plies[kMaxSearchDepth + kMaxQuiescence];
  generateLegal(board, root);
  if (root.count == 0) return result;
  result.hasMove = true;
  result.best = root.moves[0];

  const int depthCap = maxDepth < 1 ? 1 : (maxDepth > kMaxSearchDepth ? kMaxSearchDepth : maxDepth);
  for (int depth = 1; depth <= depthCap; ++depth) {
    const Move preferred = result.best;
    sortMoves(board, root, buffers.scores, &preferred);
    int alpha = -kMateScore - 1;
    const int beta = kMateScore + 1;
    Move bestHere = root.moves[0];
    int bestScore = -kMateScore - 1;
    for (int i = 0; i < root.count; ++i) {
      Board next = board;
      makeMove(next, root.moves[i]);
      const int score = -negamax(ctx, next, depth - 1, -beta, -alpha, 1);
      if (ctx.stopped) break;
      if (score > bestScore) {
        bestScore = score;
        bestHere = root.moves[i];
      }
      if (score > alpha) alpha = score;
    }
    if (ctx.stopped) {
      result.budgetExhausted = true;
      break;
    }
    result.best = bestHere;
    result.score = bestScore;
    result.depthCompleted = depth;
    // A forced mate found: deeper search cannot improve on it.
    if (bestScore >= kMateThreshold) break;
  }
  result.nodes = ctx.nodes;
  return result;
}

}  // namespace shogi
