#pragma once

// Shogi, the rules. Freestanding C++17: no Arduino, no renderer, no heap, so
// the whole ruleset is verified on the host (host-tests/shogi/run.sh) against
// published perft counts before a single square is drawn.
//
// Board: 81 cells, index = rank * 9 + file. Rank 0 is the top row, which is
// Gote's back rank; Sente (who moves first) sits at the bottom and advances
// toward rank 0. File 0 is the leftmost column as Sente sees it, which is file
// 9 in Japanese notation. The SFEN string lists cells in exactly this order.
//
// What makes this shogi rather than chess-with-letters, and where the bugs
// live, is all built into move generation rather than validated afterwards:
//   - captured pieces change sides and can be DROPPED back on an empty square;
//   - a dropped pawn may not share a file with your own unpromoted pawn (nifu);
//   - nothing may be dropped or moved where it could never move again, which
//     also makes promotion compulsory for pawns, lances and knights there;
//   - a pawn drop may not deliver checkmate (uchifuzume).
//
// Moving into check is illegal, and having no legal move loses -- shogi has no
// stalemate.

#include <cstdint>

namespace shogi {

constexpr int kFiles = 9;
constexpr int kRanks = 9;
constexpr int kCells = kFiles * kRanks;

// Seats. Sente moves first and plays up the board.
constexpr uint8_t kSente = 0;
constexpr uint8_t kGote = 1;

// Piece types. A promoted piece is its base type + 8, so promotion and
// demotion are an add and a subtract. King is 8 and never promotes, and 13
// (promoted gold) does not exist.
enum Type : uint8_t {
  None = 0,
  Pawn = 1,
  Lance = 2,
  Knight = 3,
  Silver = 4,
  Gold = 5,
  Bishop = 6,
  Rook = 7,
  King = 8,
  Tokin = 9,
  PLance = 10,
  PKnight = 11,
  PSilver = 12,
  Horse = 14,
  Dragon = 15,
};

// A cell is a type in the low four bits plus an owner bit.
constexpr uint8_t kGoteFlag = 0x10;
constexpr uint8_t kTypeMask = 0x0F;

constexpr uint8_t typeOf(const uint8_t cell) { return cell & kTypeMask; }
constexpr uint8_t ownerOf(const uint8_t cell) { return (cell & kGoteFlag) != 0 ? kGote : kSente; }
constexpr uint8_t makeCell(const uint8_t type, const uint8_t owner) {
  return static_cast<uint8_t>(type | (owner == kGote ? kGoteFlag : 0));
}
constexpr bool isPromoted(const uint8_t type) { return type > King; }
constexpr bool canPromoteType(const uint8_t type) { return type != None && type != Gold && type < King; }
// What a captured piece becomes in the captor's hand.
constexpr uint8_t baseOf(const uint8_t type) { return isPromoted(type) ? static_cast<uint8_t>(type - 8) : type; }
constexpr uint8_t other(const uint8_t seat) { return seat == kSente ? kGote : kSente; }

// Pieces in hand, indexed by base type 1..7 (index 0 unused, so a type IS its
// index and no caller has to remember an offset).
constexpr int kHandSlots = 8;

struct Board {
  uint8_t cell[kCells];
  uint8_t hand[2][kHandSlots];
  uint8_t turn;
};

// `from` is kDropFrom for a drop, in which case `drop` names the base type.
constexpr uint8_t kDropFrom = 0xFF;

struct Move {
  uint8_t from;
  uint8_t to;
  uint8_t drop;
  uint8_t promote;
};

constexpr bool isDrop(const Move& move) { return move.from == kDropFrom; }

inline bool sameMove(const Move& a, const Move& b) {
  return a.from == b.from && a.to == b.to && a.drop == b.drop && a.promote == b.promote;
}

// 593 is the most legal moves any shogi position is known to have.
constexpr int kMaxMoves = 600;

struct MoveList {
  Move moves[kMaxMoves];
  int count = 0;
  void clear() { count = 0; }
  void add(const Move& move) {
    if (count < kMaxMoves) moves[count++] = move;
  }
};

// Scratch for the rules. On a device the activity owns one, so it exists only
// while Shogi is open, and it must be registered before the rules are asked
// anything. Main thread only: the rules never run on the render task.
struct RulesScratch {
  MoveList legal[2];
  MoveList pseudo;
};
void setRulesScratch(RulesScratch* scratch);
void clearRulesScratch(const RulesScratch* scratch);

inline int fileOf(const int index) { return index % kFiles; }
inline int rankOf(const int index) { return index / kFiles; }
inline int indexOf(const int file, const int rank) { return rank * kFiles + file; }
inline bool inside(const int file, const int rank) { return file >= 0 && file < kFiles && rank >= 0 && rank < kRanks; }

// The three ranks nearest the opponent, where promotion becomes possible.
inline bool inZone(const int rank, const uint8_t seat) { return seat == kSente ? rank <= 2 : rank >= 6; }

// How many ranks from the far edge `rank` is, for `seat`: 0 on the last rank.
inline int ranksToEdge(const int rank, const uint8_t seat) { return seat == kSente ? rank : kRanks - 1 - rank; }

// True when a piece of `type` standing on `rank` could never move again.
inline bool deadEnd(const uint8_t type, const int rank, const uint8_t seat) {
  const int edge = ranksToEdge(rank, seat);
  if (type == Pawn || type == Lance) return edge == 0;
  if (type == Knight) return edge <= 1;
  return false;
}

void start(Board& board);

// Parses an SFEN record ("lnsgkgsnl/1r5b1/... b - 1"). The move number is
// optional. Returns false and leaves `board` alone on anything malformed.
bool parseSfen(const char* sfen, Board& board);

// The square of `seat`'s king, or -1 when it has none (test positions only).
int kingSquare(const Board& board, uint8_t seat);

// True when any piece of `by` attacks `square`.
bool isAttacked(const Board& board, int square, uint8_t by);

bool inCheck(const Board& board, uint8_t seat);

// Every move the side to move could make if leaving the king attacked were
// allowed. Already obeys nifu, dead ends and compulsory promotion; only king
// safety and uchifuzume are left to isLegal().
void generatePseudo(const Board& board, MoveList& list);

// Applies a move without asking whether it is legal. Search and legality
// checks call this on copies.
void makeMove(Board& board, const Move& move);

// Whether a pseudo-legal move is legal: the mover's king is not left attacked,
// and a pawn drop does not deliver mate.
bool isLegal(const Board& board, const Move& move);

void generateLegal(const Board& board, MoveList& list);

// Whether the side to move has any legal move. Uses main-thread scratch lists
// rather than the stack, so call it from the loop, never from a render.
bool hasLegalMove(const Board& board);

// Node count at `depth`. The standard correctness measure for a move
// generator; see host-tests/shogi.
uint64_t perft(const Board& board, int depth);

// A 32-bit fingerprint of the position (cells, hands, side to move), for
// repetition. FNV-1a: it is computed once per ply, so there is nothing for an
// incremental Zobrist table to save.
uint32_t positionKey(const Board& board);

// USI notation: "7g7f", "8h2b+", "P*5e". `out` must hold 6 bytes.
void moveToUsi(const Move& move, char* out);

// ---------------------------------------------------------------------------
// The game: a board plus what it takes to end one. Trivially copyable and
// under 192 bytes, because it is also the wire format for a two-device match.

// Four occurrences of the same position is sennichite, a draw. Sixteen recent
// keys catch every repetition a real game produces (a cycle is four plies).
constexpr int kKeyRing = 16;
constexpr int kRepetitionDraw = 4;
// A game that has not ended by here is called a draw (jishogi, simplified).
constexpr uint16_t kMaxPlies = 400;

constexpr uint8_t kNoSquare = 0xFE;

struct Game {
  Board board;
  uint8_t lastFrom;
  uint8_t lastTo;
  uint16_t plies;
  uint8_t keyCount;
  uint8_t keyHead;
  uint8_t pad[2];
  uint32_t keys[kKeyRing];
};

enum class Outcome : uint8_t { Running, SenteWins, GoteWins, Draw };

void startGame(Game& game);

// Plays `move` only if it is legal here. Refuses anything else, so a stale tap
// cannot half-apply and desync a two-device match.
bool play(Game& game, const Move& move);

// How many of the remembered positions equal the current one, itself included.
int repetitions(const Game& game);

Outcome outcome(const Game& game);
inline bool over(const Game& game) { return outcome(game) != Outcome::Running; }

}  // namespace shogi
