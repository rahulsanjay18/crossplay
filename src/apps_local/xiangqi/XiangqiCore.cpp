#include "XiangqiCore.h"

namespace xiangqi {

namespace {

constexpr int8_t kOrthFile[4] = {0, 0, -1, 1};
constexpr int8_t kOrthRank[4] = {-1, 1, 0, 0};
constexpr int8_t kDiagFile[4] = {-1, 1, -1, 1};
constexpr int8_t kDiagRank[4] = {-1, -1, 1, 1};
// The eight horse jumps. The leg is the orthogonal neighbour in the direction
// of the long arm.
constexpr int8_t kHorseFile[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
constexpr int8_t kHorseRank[8] = {-2, -2, -1, -1, 1, 1, 2, 2};

inline bool ownedBy(const uint8_t cell, const uint8_t seat) { return cell != 0 && ownerOf(cell) == seat; }

inline void legOf(const int df, const int dr, int& lf, int& lr) {
  if (dr == 2 || dr == -2) {
    lf = 0;
    lr = dr / 2;
  } else {
    lf = df / 2;
    lr = 0;
  }
}

void addIfNotOwn(const Board& board, MoveList& list, const int from, const int f, const int r, const uint8_t seat) {
  const int to = indexOf(f, r);
  if (ownedBy(board.cell[to], seat)) return;
  list.add(Move{static_cast<uint8_t>(from), static_cast<uint8_t>(to)});
}

uint8_t fromFenLetter(const char c, uint8_t& owner) {
  owner = (c >= 'a' && c <= 'z') ? kBlack : kRed;
  switch (c | 0x20) {
    case 'k':
      return General;
    case 'a':
      return Advisor;
    case 'b':
    case 'e':
      return Elephant;
    case 'n':
    case 'h':
      return Horse;
    case 'r':
      return Chariot;
    case 'c':
      return Cannon;
    case 'p':
      return Soldier;
    default:
      return None;
  }
}

}  // namespace

void start(Board& board) { parseFen("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w", board); }

bool parseFen(const char* fen, Board& out) {
  if (fen == nullptr) return false;
  Board board{};
  const char* c = fen;
  int file = 0;
  int rank = 0;
  for (; *c != '\0' && *c != ' '; ++c) {
    if (*c == '/') {
      if (file != kFiles) return false;
      ++rank;
      file = 0;
      continue;
    }
    if (*c >= '1' && *c <= '9') {
      file += *c - '0';
      if (file > kFiles) return false;
      continue;
    }
    uint8_t owner = kRed;
    const uint8_t type = fromFenLetter(*c, owner);
    if (type == None || file >= kFiles || rank >= kRanks) return false;
    board.cell[indexOf(file, rank)] = makeCell(type, owner);
    ++file;
  }
  if (rank != kRanks - 1 || file != kFiles || *c != ' ') return false;
  ++c;
  if (*c == 'w' || *c == 'r') {
    board.turn = kRed;
  } else if (*c == 'b') {
    board.turn = kBlack;
  } else {
    return false;
  }
  out = board;
  return true;
}

int generalSquare(const Board& board, const uint8_t seat) {
  const uint8_t general = makeCell(General, seat);
  // Only the palaces can hold one, so only the palaces are searched.
  const int r0 = seat == kRed ? 7 : 0;
  for (int r = r0; r < r0 + 3; ++r) {
    for (int f = 3; f <= 5; ++f) {
      if (board.cell[indexOf(f, r)] == general) return indexOf(f, r);
    }
  }
  return -1;
}

bool isAttacked(const Board& board, const int point, const uint8_t by) {
  const int pf = fileOf(point);
  const int pr = rankOf(point);

  // Lines: a chariot as the first piece, a cannon as the second. A general as
  // the first piece on the FILE is the facing rule; one point away on the rank
  // is an ordinary general step (only possible inside its palace).
  for (int d = 0; d < 4; ++d) {
    int f = pf + kOrthFile[d];
    int r = pr + kOrthRank[d];
    int seen = 0;
    int distance = 1;
    while (inside(f, r)) {
      const uint8_t cell = board.cell[indexOf(f, r)];
      if (cell != 0) {
        ++seen;
        if (seen == 1) {
          if (ownedBy(cell, by)) {
            const uint8_t type = typeOf(cell);
            if (type == Chariot) return true;
            if (type == General && (kOrthFile[d] == 0 || (distance == 1 && inPalace(pf, pr, by)))) return true;
          }
        } else {
          if (ownedBy(cell, by) && typeOf(cell) == Cannon) return true;
          break;
        }
      }
      f += kOrthFile[d];
      r += kOrthRank[d];
      ++distance;
    }
  }

  // Horses, each checked against its own leg.
  for (int i = 0; i < 8; ++i) {
    const int hf = pf - kHorseFile[i];
    const int hr = pr - kHorseRank[i];
    if (!inside(hf, hr)) continue;
    const uint8_t cell = board.cell[indexOf(hf, hr)];
    if (!ownedBy(cell, by) || typeOf(cell) != Horse) continue;
    int lf = 0;
    int lr = 0;
    legOf(kHorseFile[i], kHorseRank[i], lf, lr);
    if (board.cell[indexOf(hf + lf, hr + lr)] == 0) return true;
  }

  // Soldiers: from behind (in their direction of travel), or from the side
  // once they have crossed the river.
  {
    const int sr = pr - forward(by);
    if (inside(pf, sr) && board.cell[indexOf(pf, sr)] == makeCell(Soldier, by)) return true;
    if (!ownSide(pr, by)) {
      for (int df = -1; df <= 1; df += 2) {
        if (inside(pf + df, pr) && board.cell[indexOf(pf + df, pr)] == makeCell(Soldier, by)) return true;
      }
    }
  }

  // Advisors and elephants reach only their own side, so they can only ever
  // attack a point there.
  if (inPalace(pf, pr, by)) {
    for (int d = 0; d < 4; ++d) {
      const int f = pf + kDiagFile[d];
      const int r = pr + kDiagRank[d];
      if (inside(f, r) && board.cell[indexOf(f, r)] == makeCell(Advisor, by)) return true;
    }
  }
  if (ownSide(pr, by)) {
    for (int d = 0; d < 4; ++d) {
      const int f = pf + kDiagFile[d] * 2;
      const int r = pr + kDiagRank[d] * 2;
      if (!inside(f, r) || board.cell[indexOf(f, r)] != makeCell(Elephant, by)) continue;
      if (board.cell[indexOf(pf + kDiagFile[d], pr + kDiagRank[d])] == 0) return true;
    }
  }
  return false;
}

bool inCheck(const Board& board, const uint8_t seat) {
  const int general = generalSquare(board, seat);
  return general >= 0 && isAttacked(board, general, other(seat));
}

void generatePseudo(const Board& board, MoveList& list) {
  list.clear();
  const uint8_t seat = board.turn;
  for (int from = 0; from < kPoints; ++from) {
    const uint8_t cell = board.cell[from];
    if (!ownedBy(cell, seat)) continue;
    const int ff = fileOf(from);
    const int fr = rankOf(from);
    switch (typeOf(cell)) {
      case General:
        for (int d = 0; d < 4; ++d) {
          const int f = ff + kOrthFile[d];
          const int r = fr + kOrthRank[d];
          if (inPalace(f, r, seat)) addIfNotOwn(board, list, from, f, r, seat);
        }
        break;
      case Advisor:
        for (int d = 0; d < 4; ++d) {
          const int f = ff + kDiagFile[d];
          const int r = fr + kDiagRank[d];
          if (inPalace(f, r, seat)) addIfNotOwn(board, list, from, f, r, seat);
        }
        break;
      case Elephant:
        for (int d = 0; d < 4; ++d) {
          const int f = ff + kDiagFile[d] * 2;
          const int r = fr + kDiagRank[d] * 2;
          if (!inside(f, r) || !ownSide(r, seat)) continue;
          if (board.cell[indexOf(ff + kDiagFile[d], fr + kDiagRank[d])] != 0) continue;
          addIfNotOwn(board, list, from, f, r, seat);
        }
        break;
      case Horse:
        for (int i = 0; i < 8; ++i) {
          const int f = ff + kHorseFile[i];
          const int r = fr + kHorseRank[i];
          if (!inside(f, r)) continue;
          int lf = 0;
          int lr = 0;
          legOf(kHorseFile[i], kHorseRank[i], lf, lr);
          if (board.cell[indexOf(ff + lf, fr + lr)] != 0) continue;
          addIfNotOwn(board, list, from, f, r, seat);
        }
        break;
      case Chariot:
        for (int d = 0; d < 4; ++d) {
          int f = ff + kOrthFile[d];
          int r = fr + kOrthRank[d];
          while (inside(f, r)) {
            const uint8_t target = board.cell[indexOf(f, r)];
            if (!ownedBy(target, seat)) list.add(Move{static_cast<uint8_t>(from), static_cast<uint8_t>(indexOf(f, r))});
            if (target != 0) break;
            f += kOrthFile[d];
            r += kOrthRank[d];
          }
        }
        break;
      case Cannon:
        for (int d = 0; d < 4; ++d) {
          int f = ff + kOrthFile[d];
          int r = fr + kOrthRank[d];
          bool screened = false;
          while (inside(f, r)) {
            const uint8_t target = board.cell[indexOf(f, r)];
            if (!screened) {
              if (target == 0) {
                list.add(Move{static_cast<uint8_t>(from), static_cast<uint8_t>(indexOf(f, r))});
              } else {
                screened = true;
              }
            } else if (target != 0) {
              if (!ownedBy(target, seat))
                list.add(Move{static_cast<uint8_t>(from), static_cast<uint8_t>(indexOf(f, r))});
              break;
            }
            f += kOrthFile[d];
            r += kOrthRank[d];
          }
        }
        break;
      case Soldier: {
        const int r = fr + forward(seat);
        if (inside(ff, r)) addIfNotOwn(board, list, from, ff, r, seat);
        if (!ownSide(fr, seat)) {
          for (int df = -1; df <= 1; df += 2) {
            if (inside(ff + df, fr)) addIfNotOwn(board, list, from, ff + df, fr, seat);
          }
        }
        break;
      }
      default:
        break;
    }
  }
}

void makeMove(Board& board, const Move& move) {
  board.cell[move.to] = board.cell[move.from];
  board.cell[move.from] = 0;
  board.turn = other(board.turn);
}

bool isLegal(const Board& board, const Move& move) {
  Board next = board;
  makeMove(next, move);
  const int general = generalSquare(next, board.turn);
  return general < 0 || !isAttacked(next, general, next.turn);
}

void generateLegal(const Board& board, MoveList& list) {
  generatePseudo(board, list);
  int kept = 0;
  for (int i = 0; i < list.count; ++i) {
    if (isLegal(board, list.moves[i])) list.moves[kept++] = list.moves[i];
  }
  list.count = kept;
}

bool hasLegalMove(const Board& board) {
  static MoveList scratch;
  generatePseudo(board, scratch);
  for (int i = 0; i < scratch.count; ++i) {
    if (isLegal(board, scratch.moves[i])) return true;
  }
  return false;
}

uint64_t perft(const Board& board, const int depth) {
  if (depth == 0) return 1;
  MoveList list;
  generateLegal(board, list);
  if (depth == 1) return static_cast<uint64_t>(list.count);
  uint64_t nodes = 0;
  for (int i = 0; i < list.count; ++i) {
    Board next = board;
    makeMove(next, list.moves[i]);
    nodes += perft(next, depth - 1);
  }
  return nodes;
}

uint32_t positionKey(const Board& board) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < kPoints; ++i) {
    hash ^= board.cell[i];
    hash *= 16777619u;
  }
  hash ^= board.turn;
  hash *= 16777619u;
  return hash;
}

void moveToIccs(const Move& move, char* out) {
  out[0] = static_cast<char>('a' + fileOf(move.from));
  out[1] = static_cast<char>('0' + (kRanks - 1 - rankOf(move.from)));
  out[2] = static_cast<char>('a' + fileOf(move.to));
  out[3] = static_cast<char>('0' + (kRanks - 1 - rankOf(move.to)));
  out[4] = '\0';
}

// ---------------------------------------------------------------------------

namespace {

void remember(Game& game) {
  game.keys[game.keyHead] = positionKey(game.board);
  game.keyHead = static_cast<uint8_t>((game.keyHead + 1) % kKeyRing);
  if (game.keyCount < kKeyRing) ++game.keyCount;
}

}  // namespace

void startGame(Game& game) {
  game = Game{};
  start(game.board);
  game.lastFrom = kNoPoint;
  game.lastTo = kNoPoint;
  remember(game);
}

bool play(Game& game, const Move& move) {
  static MoveList pseudo;
  generatePseudo(game.board, pseudo);
  for (int i = 0; i < pseudo.count; ++i) {
    if (!sameMove(pseudo.moves[i], move)) continue;
    if (!isLegal(game.board, move)) return false;
    const bool capture = game.board.cell[move.to] != 0;
    makeMove(game.board, move);
    game.lastFrom = move.from;
    game.lastTo = move.to;
    if (game.plies < 0xFFFF) ++game.plies;
    if (capture) {
      game.idlePlies = 0;
      // A capture is irreversible: nothing before it can come back, so the
      // ring starts over and an old position cannot be miscounted.
      game.keyCount = 0;
      game.keyHead = 0;
    } else if (game.idlePlies < 255) {
      ++game.idlePlies;
    }
    remember(game);
    return true;
  }
  return false;
}

int repetitions(const Game& game) {
  const uint32_t now = positionKey(game.board);
  int count = 0;
  for (int i = 0; i < game.keyCount; ++i) {
    if (game.keys[i] == now) ++count;
  }
  return count;
}

Outcome outcome(const Game& game) {
  if (!hasLegalMove(game.board)) return game.board.turn == kRed ? Outcome::BlackWins : Outcome::RedWins;
  if (repetitions(game) >= kRepetitionDraw) return Outcome::Draw;
  if (game.idlePlies >= kIdleLimit || game.plies >= kMaxPlies) return Outcome::Draw;
  return Outcome::Running;
}

}  // namespace xiangqi
