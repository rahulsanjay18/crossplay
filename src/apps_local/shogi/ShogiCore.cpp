#include "ShogiCore.h"

namespace shogi {

namespace {

// Ten directions as (file, rank) offsets in SENTE's orientation, where forward
// is rank - 1. Gote uses each one rotated half a turn. Eight king directions,
// then the two knight jumps.
constexpr int8_t kDirFile[10] = {-1, 0, 1, -1, 1, -1, 0, 1, -1, 1};
constexpr int8_t kDirRank[10] = {-1, -1, -1, 0, 0, 1, 1, 1, -2, -2};

// Not `dirBit()`: Arduino.h defines dirBit(b) as a function-like macro, and any
// translation unit that reaches it would silently rewrite this.
constexpr uint16_t dirBit(const int i) { return static_cast<uint16_t>(1u << i); }

// The gold's six: three forward, two sideways, one straight back. Every
// promoted minor piece moves like this.
constexpr uint16_t kGoldSteps = dirBit(0) | dirBit(1) | dirBit(2) | dirBit(3) | dirBit(4) | dirBit(6);
constexpr uint16_t kDiagonals = dirBit(0) | dirBit(2) | dirBit(5) | dirBit(7);
constexpr uint16_t kOrthogonals = dirBit(1) | dirBit(3) | dirBit(4) | dirBit(6);

// One step in each direction set. Indexed by type.
constexpr uint16_t kSteps[16] = {
    0,                                                          // None
    dirBit(1),                                                  // Pawn
    0,                                                          // Lance (slides)
    dirBit(8) | dirBit(9),                                      // Knight
    dirBit(0) | dirBit(1) | dirBit(2) | dirBit(5) | dirBit(7),  // Silver
    kGoldSteps,                                                 // Gold
    0,                                                          // Bishop (slides)
    0,                                                          // Rook (slides)
    kDiagonals | kOrthogonals,                                  // King
    kGoldSteps,                                                 // Tokin
    kGoldSteps,                                                 // PLance
    kGoldSteps,                                                 // PKnight
    kGoldSteps,                                                 // PSilver
    0,                                                          // 13 does not exist
    kOrthogonals,                                               // Horse: bishop + one orthogonal step
    kDiagonals,                                                 // Dragon: rook + one diagonal step
};

// Unlimited slides. Only directions 0..7 ever slide.
constexpr uint16_t kSlides[16] = {
    0, 0, dirBit(1), 0, 0, 0, kDiagonals, kOrthogonals, 0, 0, 0, 0, 0, 0, kDiagonals, kOrthogonals,
};

inline int dirFile(const int i, const uint8_t seat) { return seat == kSente ? kDirFile[i] : -kDirFile[i]; }
inline int dirRank(const int i, const uint8_t seat) { return seat == kSente ? kDirRank[i] : -kDirRank[i]; }

inline bool ownedBy(const uint8_t cell, const uint8_t seat) { return cell != 0 && ownerOf(cell) == seat; }

void addBoardMove(MoveList& list, const uint8_t type, const int from, const int to, const uint8_t seat) {
  const int fromRank = rankOf(from);
  const int toRank = rankOf(to);
  Move move{static_cast<uint8_t>(from), static_cast<uint8_t>(to), None, 0};
  if (canPromoteType(type) && (inZone(fromRank, seat) || inZone(toRank, seat))) {
    move.promote = 1;
    list.add(move);
    move.promote = 0;
  }
  // A piece that could never move again from `to` must promote, so the
  // unpromoted version of that move simply does not exist.
  if (!deadEnd(type, toRank, seat)) list.add(move);
}

uint8_t fromSfenLetter(const char c, uint8_t& owner) {
  owner = (c >= 'a' && c <= 'z') ? kGote : kSente;
  switch (c | 0x20) {
    case 'p':
      return Pawn;
    case 'l':
      return Lance;
    case 'n':
      return Knight;
    case 's':
      return Silver;
    case 'g':
      return Gold;
    case 'b':
      return Bishop;
    case 'r':
      return Rook;
    case 'k':
      return King;
    default:
      return None;
  }
}

constexpr char kUsiDrop[8] = {'?', 'P', 'L', 'N', 'S', 'G', 'B', 'R'};

}  // namespace

void start(Board& board) { parseSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", board); }

bool parseSfen(const char* sfen, Board& out) {
  if (sfen == nullptr) return false;
  Board board{};
  const char* c = sfen;
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
    bool promoted = false;
    if (*c == '+') {
      promoted = true;
      ++c;
    }
    uint8_t owner = kSente;
    uint8_t type = fromSfenLetter(*c, owner);
    if (type == None || file >= kFiles || rank >= kRanks) return false;
    if (promoted) {
      if (!canPromoteType(type)) return false;
      type = static_cast<uint8_t>(type + 8);
    }
    board.cell[indexOf(file, rank)] = makeCell(type, owner);
    ++file;
  }
  if (rank != kRanks - 1 || file != kFiles || *c != ' ') return false;
  ++c;
  if (*c == 'b') {
    board.turn = kSente;
  } else if (*c == 'w') {
    board.turn = kGote;
  } else {
    return false;
  }
  ++c;
  if (*c != ' ') return false;
  ++c;
  if (*c == '-') {
    ++c;
  } else {
    while (*c != '\0' && *c != ' ') {
      int count = 1;
      if (*c >= '0' && *c <= '9') {
        count = 0;
        while (*c >= '0' && *c <= '9') count = count * 10 + (*c++ - '0');
      }
      uint8_t owner = kSente;
      const uint8_t type = fromSfenLetter(*c, owner);
      if (type == None || type == King || count <= 0 || count > 18) return false;
      board.hand[owner][type] = static_cast<uint8_t>(board.hand[owner][type] + count);
      ++c;
    }
  }
  out = board;
  return true;
}

int kingSquare(const Board& board, const uint8_t seat) {
  const uint8_t king = makeCell(King, seat);
  for (int i = 0; i < kCells; ++i) {
    if (board.cell[i] == king) return i;
  }
  return -1;
}

bool isAttacked(const Board& board, const int square, const uint8_t by) {
  const int sf = fileOf(square);
  const int sr = rankOf(square);
  // Steppers: an attacker one step BEHIND the target in direction i attacks it
  // if its type steps in direction i.
  for (int i = 0; i < 10; ++i) {
    const int f = sf - dirFile(i, by);
    const int r = sr - dirRank(i, by);
    if (!inside(f, r)) continue;
    const uint8_t cell = board.cell[indexOf(f, r)];
    if (ownedBy(cell, by) && (kSteps[typeOf(cell)] & dirBit(i)) != 0) return true;
  }
  // Sliders: walk backwards from the target until the first piece.
  for (int i = 0; i < 8; ++i) {
    const int df = dirFile(i, by);
    const int dr = dirRank(i, by);
    int f = sf - df;
    int r = sr - dr;
    while (inside(f, r)) {
      const uint8_t cell = board.cell[indexOf(f, r)];
      if (cell != 0) {
        if (ownedBy(cell, by) && (kSlides[typeOf(cell)] & dirBit(i)) != 0) return true;
        break;
      }
      f -= df;
      r -= dr;
    }
  }
  return false;
}

bool inCheck(const Board& board, const uint8_t seat) {
  const int king = kingSquare(board, seat);
  return king >= 0 && isAttacked(board, king, other(seat));
}

void generatePseudo(const Board& board, MoveList& list) {
  list.clear();
  const uint8_t seat = board.turn;

  for (int from = 0; from < kCells; ++from) {
    const uint8_t cell = board.cell[from];
    if (!ownedBy(cell, seat)) continue;
    const uint8_t type = typeOf(cell);
    const int ff = fileOf(from);
    const int fr = rankOf(from);
    const uint16_t steps = kSteps[type];
    const uint16_t slides = kSlides[type];
    for (int i = 0; i < 10; ++i) {
      if ((steps & dirBit(i)) == 0) continue;
      const int f = ff + dirFile(i, seat);
      const int r = fr + dirRank(i, seat);
      if (!inside(f, r)) continue;
      const int to = indexOf(f, r);
      if (ownedBy(board.cell[to], seat)) continue;
      addBoardMove(list, type, from, to, seat);
    }
    for (int i = 0; i < 8; ++i) {
      if ((slides & dirBit(i)) == 0) continue;
      const int df = dirFile(i, seat);
      const int dr = dirRank(i, seat);
      int f = ff + df;
      int r = fr + dr;
      while (inside(f, r)) {
        const int to = indexOf(f, r);
        const uint8_t target = board.cell[to];
        if (ownedBy(target, seat)) break;
        addBoardMove(list, type, from, to, seat);
        if (target != 0) break;
        f += df;
        r += dr;
      }
    }
  }

  // Files already holding one of our unpromoted pawns, for nifu.
  bool pawnOnFile[kFiles] = {};
  const uint8_t ownPawn = makeCell(Pawn, seat);
  for (int i = 0; i < kCells; ++i) {
    if (board.cell[i] == ownPawn) pawnOnFile[fileOf(i)] = true;
  }

  for (uint8_t type = Pawn; type <= Rook; ++type) {
    if (board.hand[seat][type] == 0) continue;
    for (int to = 0; to < kCells; ++to) {
      if (board.cell[to] != 0) continue;
      if (deadEnd(type, rankOf(to), seat)) continue;
      if (type == Pawn && pawnOnFile[fileOf(to)]) continue;
      list.add(Move{kDropFrom, static_cast<uint8_t>(to), type, 0});
    }
  }
}

void makeMove(Board& board, const Move& move) {
  const uint8_t seat = board.turn;
  if (isDrop(move)) {
    board.cell[move.to] = makeCell(move.drop, seat);
    --board.hand[seat][move.drop];
  } else {
    const uint8_t captured = board.cell[move.to];
    if (captured != 0) ++board.hand[seat][baseOf(typeOf(captured))];
    uint8_t type = typeOf(board.cell[move.from]);
    if (move.promote != 0) type = static_cast<uint8_t>(type + 8);
    board.cell[move.to] = makeCell(type, seat);
    board.cell[move.from] = 0;
  }
  board.turn = other(seat);
}

namespace {

// Existence of a legal reply, with the uchifuzume test switched off below the
// first level. The only question uchifuzume asks of a reply is "is there one",
// and the answer cannot hinge on another pawn-drop mate one ply deeper: a
// side in check from a pawn can always answer it some other way if at all.
#if defined(ARDUINO_ARCH_ESP32)
RulesScratch* const kDefaultRulesScratch = nullptr;
#else
// Host tests call the rules with no activity around them.
RulesScratch gHostRulesScratch;
RulesScratch* const kDefaultRulesScratch = &gHostRulesScratch;
#endif
RulesScratch* gRulesScratch = kDefaultRulesScratch;

bool hasLegalMoveAt(const Board& board, int depth);

bool isLegalAt(const Board& board, const Move& move, const int depth) {
  Board next = board;
  makeMove(next, move);
  const uint8_t mover = board.turn;
  const int king = kingSquare(next, mover);
  if (king >= 0 && isAttacked(next, king, next.turn)) return false;
  // Uchifuzume: a pawn drop that checks must leave the defender a reply.
  if (isDrop(move) && move.drop == Pawn && depth == 0) {
    const int theirs = kingSquare(next, next.turn);
    const int ahead = mover == kSente ? move.to - kFiles : move.to + kFiles;
    if (theirs >= 0 && theirs == ahead && !hasLegalMoveAt(next, depth + 1)) return false;
  }
  return true;
}

bool hasLegalMoveAt(const Board& board, const int depth) {
  // Scratch so neither level puts a 2.4KB list on the stack. Level 0 serves
  // hasLegalMove(); level 1 serves the uchifuzume question, which never
  // recurses further.
  MoveList& list = gRulesScratch->legal[depth > 1 ? 1 : depth];
  generatePseudo(board, list);
  for (int i = 0; i < list.count; ++i) {
    if (isLegalAt(board, list.moves[i], depth)) return true;
  }
  return false;
}

}  // namespace

void setRulesScratch(RulesScratch* scratch) { gRulesScratch = scratch; }

void clearRulesScratch(const RulesScratch* scratch) {
  if (gRulesScratch == scratch) gRulesScratch = kDefaultRulesScratch;
}

bool isLegal(const Board& board, const Move& move) { return isLegalAt(board, move, 0); }

void generateLegal(const Board& board, MoveList& list) {
  // Filtered in place: a second 2.4KB list on the stack is the thing the
  // device cannot afford.
  generatePseudo(board, list);
  int kept = 0;
  for (int i = 0; i < list.count; ++i) {
    if (isLegal(board, list.moves[i])) list.moves[kept++] = list.moves[i];
  }
  list.count = kept;
}

bool hasLegalMove(const Board& board) { return hasLegalMoveAt(board, 0); }

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
  const auto mix = [&hash](const uint8_t byte) {
    hash ^= byte;
    hash *= 16777619u;
  };
  for (int i = 0; i < kCells; ++i) mix(board.cell[i]);
  for (int seat = 0; seat < 2; ++seat) {
    for (int t = 0; t < kHandSlots; ++t) mix(board.hand[seat][t]);
  }
  mix(board.turn);
  return hash;
}

void moveToUsi(const Move& move, char* out) {
  int n = 0;
  if (isDrop(move)) {
    out[n++] = kUsiDrop[move.drop & 7];
    out[n++] = '*';
  } else {
    out[n++] = static_cast<char>('0' + (kFiles - fileOf(move.from)));
    out[n++] = static_cast<char>('a' + rankOf(move.from));
  }
  out[n++] = static_cast<char>('0' + (kFiles - fileOf(move.to)));
  out[n++] = static_cast<char>('a' + rankOf(move.to));
  if (move.promote != 0) out[n++] = '+';
  out[n] = '\0';
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
  game.lastFrom = kNoSquare;
  game.lastTo = kNoSquare;
  remember(game);
}

bool play(Game& game, const Move& move) {
  // Main-thread scratch, like hasLegalMoveAt's. The rules never run on the
  // render task: activities cache what a paint needs.
  MoveList& pseudo = gRulesScratch->pseudo;
  generatePseudo(game.board, pseudo);
  for (int i = 0; i < pseudo.count; ++i) {
    if (!sameMove(pseudo.moves[i], move)) continue;
    if (!isLegal(game.board, move)) return false;
    makeMove(game.board, move);
    game.lastFrom = move.from;
    game.lastTo = move.to;
    if (game.plies < 0xFFFF) ++game.plies;
    remember(game);
    return true;
  }
  return false;
}

int repetitions(const Game& game) {
  if (game.keyCount == 0) return 0;
  const uint32_t now = positionKey(game.board);
  int count = 0;
  for (int i = 0; i < game.keyCount; ++i) {
    if (game.keys[i] == now) ++count;
  }
  return count;
}

Outcome outcome(const Game& game) {
  if (!hasLegalMove(game.board)) return game.board.turn == kSente ? Outcome::GoteWins : Outcome::SenteWins;
  if (repetitions(game) >= kRepetitionDraw) return Outcome::Draw;
  if (game.plies >= kMaxPlies) return Outcome::Draw;
  return Outcome::Running;
}

}  // namespace shogi
