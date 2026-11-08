#include "XiangqiEngine.h"

namespace xiangqi {

namespace {

// Conventional values: a chariot is worth about two horses or two cannons, and
// a soldier doubles once it has crossed the river and can move sideways.
constexpr int kValue[8] = {0, 0, 200, 200, 400, 900, 450, 100};

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

int orderScore(const Board& board, const Move& move, const Move* preferred) {
  if (preferred != nullptr && sameMove(move, *preferred)) return 1000000;
  const uint8_t victim = board.cell[move.to];
  if (victim == 0) return 0;
  // A general is never actually taken, but a move that "captures" one would
  // mean the opponent left it en prise; order it first anyway.
  const int victimValue = typeOf(victim) == General ? 5000 : kValue[typeOf(victim)];
  return 10000 + victimValue * 8 - kValue[typeOf(board.cell[move.from])] / 16;
}

void sortMoves(const Board& board, MoveList& list, int* scores, const Move* preferred) {
  for (int i = 0; i < list.count; ++i) scores[i] = orderScore(board, list.moves[i], preferred);
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
  int kept = 0;
  for (int i = 0; i < list.count; ++i) {
    if (board.cell[list.moves[i].to] != 0) list.moves[kept++] = list.moves[i];
  }
  list.count = kept;
  sortMoves(board, list, ctx.buffers.scores, nullptr);

  for (int i = 0; i < list.count; ++i) {
    const Move move = list.moves[i];
    if (!isLegal(board, move)) continue;
    Board next = board;
    makeMove(next, move);
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
  // No legal move loses, stalemate included.
  if (!any) return -kMateScore + ply;
  return best;
}

}  // namespace

int pieceValue(const uint8_t type) { return kValue[type & kTypeMask]; }

int evaluate(const Board& board) {
  int score[2] = {0, 0};
  for (int i = 0; i < kPoints; ++i) {
    const uint8_t cell = board.cell[i];
    if (cell == 0) continue;
    const uint8_t owner = ownerOf(cell);
    const uint8_t type = typeOf(cell);
    const int file = fileOf(i);
    const int rank = rankOf(i);
    score[owner] += kValue[type];
    if (type == Soldier && !ownSide(rank, owner)) {
      // Across the river: worth double, and more the nearer the palace and the
      // centre files it gets.
      const int depth = owner == kRed ? 4 - rank : rank - 5;
      score[owner] += 100 + 10 * depth - 6 * (file > 4 ? file - 4 : 4 - file);
    }
    if (type == Horse || type == Cannon) {
      // Mildly central pieces, mildly advanced horses.
      score[owner] -= 4 * (file > 4 ? file - 4 : 4 - file);
      if (type == Horse && !ownSide(rank, owner)) score[owner] += 20;
    }
  }
  const int mine = board.turn;
  return score[mine] - score[other(static_cast<uint8_t>(mine))];
}

SearchResult search(const Board& board, const int maxDepth, SearchBuffers& buffers, const uint32_t nodeBudget) {
  SearchResult result;
  Context ctx{buffers};
  ctx.budget = nodeBudget;

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
    if (bestScore >= kMateThreshold) break;
  }
  result.nodes = ctx.nodes;
  return result;
}

}  // namespace xiangqi
