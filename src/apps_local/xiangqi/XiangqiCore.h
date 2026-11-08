#pragma once

// Xiangqi (Chinese chess), the rules. Freestanding C++17: no Arduino, no
// renderer, no heap, verified on the host against published perft counts
// (host-tests/xiangqi/run.sh).
//
// Pieces stand on the 90 POINTS of a 9 x 10 grid, not in squares. Index =
// rank * 9 + file. Rank 0 is the top edge, Black's back rank; Red moves first
// and plays up the board from rank 9. The river runs between ranks 4 and 5.
// FEN lists points in exactly this order.
//
// The rules that make it xiangqi, all in move generation or legality:
//   - generals and advisors never leave their 3 x 3 palace;
//   - elephants never cross the river and are stopped by a piece on the "eye";
//   - horses are stopped by a piece on the "leg" next to them;
//   - cannons move like chariots but capture only by jumping exactly one piece;
//   - soldiers gain sideways steps only once across the river;
//   - the two generals may never face each other on an open file.
// Having no legal move loses, whether in check or not.

#include <cstdint>

namespace xiangqi {

constexpr int kFiles = 9;
constexpr int kRanks = 10;
constexpr int kPoints = kFiles * kRanks;

constexpr uint8_t kRed = 0;
constexpr uint8_t kBlack = 1;

enum Type : uint8_t {
  None = 0,
  General = 1,
  Advisor = 2,
  Elephant = 3,
  Horse = 4,
  Chariot = 5,
  Cannon = 6,
  Soldier = 7,
};

constexpr uint8_t kBlackFlag = 0x08;
constexpr uint8_t kTypeMask = 0x07;

constexpr uint8_t typeOf(const uint8_t cell) { return cell & kTypeMask; }
constexpr uint8_t ownerOf(const uint8_t cell) { return (cell & kBlackFlag) != 0 ? kBlack : kRed; }
constexpr uint8_t makeCell(const uint8_t type, const uint8_t owner) {
  return static_cast<uint8_t>(type | (owner == kBlack ? kBlackFlag : 0));
}
constexpr uint8_t other(const uint8_t seat) { return seat == kRed ? kBlack : kRed; }

struct Board {
  uint8_t cell[kPoints];
  uint8_t turn;
};

struct Move {
  uint8_t from;
  uint8_t to;
};

inline bool sameMove(const Move& a, const Move& b) { return a.from == b.from && a.to == b.to; }

// Real positions offer well under a hundred; 128 is headroom.
constexpr int kMaxMoves = 128;

struct MoveList {
  Move moves[kMaxMoves];
  int count = 0;
  void clear() { count = 0; }
  void add(const Move& move) {
    if (count < kMaxMoves) moves[count++] = move;
  }
};

inline int fileOf(const int index) { return index % kFiles; }
inline int rankOf(const int index) { return index / kFiles; }
inline int indexOf(const int file, const int rank) { return rank * kFiles + file; }
inline bool inside(const int file, const int rank) { return file >= 0 && file < kFiles && rank >= 0 && rank < kRanks; }

// Bounded on BOTH sides. The first version said only "rank >= 7" for Red, and
// perft caught Red's advisors and general stepping onto a rank 10 that does not
// exist: 47 moves from the opening instead of 44.
inline bool inPalace(const int file, const int rank, const uint8_t seat) {
  if (file < 3 || file > 5) return false;
  return seat == kRed ? (rank >= 7 && rank <= 9) : (rank >= 0 && rank <= 2);
}

// Whether `rank` is on `seat`'s own side of the river.
inline bool ownSide(const int rank, const uint8_t seat) { return seat == kRed ? rank >= 5 : rank <= 4; }

// Rank step toward the opponent.
inline int forward(const uint8_t seat) { return seat == kRed ? -1 : 1; }

void start(Board& board);

// Parses a xiangqi FEN ("rnbakabnr/9/1c5c1/... w"). Accepts both letter sets in
// common use (n or h for the horse, b or e for the elephant). Returns false and
// leaves `board` alone on anything malformed.
bool parseFen(const char* fen, Board& board);

int generalSquare(const Board& board, uint8_t seat);

// True when any piece of `by` attacks `point`, counting the facing-generals
// rule as an attack by the general along the file.
bool isAttacked(const Board& board, int point, uint8_t by);

bool inCheck(const Board& board, uint8_t seat);

void generatePseudo(const Board& board, MoveList& list);
void makeMove(Board& board, const Move& move);
bool isLegal(const Board& board, const Move& move);
void generateLegal(const Board& board, MoveList& list);
// Main-thread scratch; call from the loop, never from a render.
bool hasLegalMove(const Board& board);

uint64_t perft(const Board& board, int depth);
uint32_t positionKey(const Board& board);

// ICCS coordinates: "h2e2". Files a..i left to right from Red's side, ranks
// 0..9 from Red's back rank. `out` must hold 5 bytes.
void moveToIccs(const Move& move, char* out);

// ---------------------------------------------------------------------------
// The game, and the wire format for a two-device match: under 192 bytes.

// Threefold repetition is called a draw here. The full Asian rules (perpetual
// check and chase lose) need move-by-move intent tracking; a draw is the
// simplification every casual app makes.
constexpr int kKeyRing = 16;
constexpr int kRepetitionDraw = 3;
// Sixty moves each with no capture is a draw.
constexpr uint8_t kIdleLimit = 120;
constexpr uint16_t kMaxPlies = 400;

constexpr uint8_t kNoPoint = 0xFE;

struct Game {
  Board board;
  uint8_t lastFrom;
  uint8_t lastTo;
  uint8_t idlePlies;
  uint8_t keyCount;
  uint8_t keyHead;
  uint8_t pad[2];
  uint16_t plies;
  uint32_t keys[kKeyRing];
};

enum class Outcome : uint8_t { Running, RedWins, BlackWins, Draw };

void startGame(Game& game);
bool play(Game& game, const Move& move);
int repetitions(const Game& game);
Outcome outcome(const Game& game);
inline bool over(const Game& game) { return outcome(game) != Outcome::Running; }

}  // namespace xiangqi
