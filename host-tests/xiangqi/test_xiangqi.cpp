// Xiangqi rules, engine, flow and save tests. Freestanding: no device, no
// PlatformIO. Run with host-tests/xiangqi/run.sh.
//
// Perft against the Chessprogramming wiki's Chinese Chess Perft Results page:
// the opening plus ten positions built to exercise checks, evasions, cannon
// screens, blocked horses and the facing generals. Then each rule on its own.

#include <cstdio>
#include <cstring>
#include <type_traits>

#include "XiangqiCore.h"
#include "XiangqiEngine.h"
#include "XiangqiFlow.h"
#include "XiangqiSave.h"

namespace {

int checksRun = 0;
int checksFailed = 0;

void check(const bool ok, const char* what, const int line) {
  ++checksRun;
  if (!ok) {
    ++checksFailed;
    std::printf("FAIL line %d: %s\n", line, what);
  }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

using namespace xiangqi;

Board fromFen(const char* fen) {
  Board board{};
  const bool ok = parseFen(fen, board);
  check(ok, fen, __LINE__);
  return board;
}

Game gameAt(const char* fen) {
  Game game{};
  startGame(game);
  game.board = fromFen(fen);
  game.keyCount = 1;
  game.keyHead = 1;
  game.keys[0] = positionKey(game.board);
  return game;
}

Move mv(const int ff, const int fr, const int tf, const int tr) {
  return Move{static_cast<uint8_t>(indexOf(ff, fr)), static_cast<uint8_t>(indexOf(tf, tr))};
}

bool hasMove(const Board& board, const Move& want) {
  MoveList list;
  generateLegal(board, list);
  for (int i = 0; i < list.count; ++i) {
    if (sameMove(list.moves[i], want)) return true;
  }
  return false;
}

int movesFrom(const Board& board, const int ff, const int fr) {
  MoveList list;
  generateLegal(board, list);
  int n = 0;
  for (int i = 0; i < list.count; ++i) {
    if (list.moves[i].from == indexOf(ff, fr)) ++n;
  }
  return n;
}

// ---------------------------------------------------------------------------

void perftStartPosition() {
  Board board{};
  start(board);
  CHECK(perft(board, 1) == 44);
  CHECK(perft(board, 2) == 1920);
  CHECK(perft(board, 3) == 79666);
  CHECK(perft(board, 4) == 3290240);
}

void perftTacticalPositions() {
  struct Case {
    const char* fen;
    uint64_t nodes[4];
  };
  // https://www.chessprogramming.org/Chinese_Chess_Perft_Results, positions 2-11.
  static const Case kCases[] = {
      {"r1ea1a3/4kh3/2h1e4/pHp1p1p1p/4c4/6P2/P1P2R2P/1CcC5/9/2EAKAE2 w - - 0 1", {38, 1128, 43929, 1339047}},
      {"1ceak4/9/h2a5/2p1p3p/5cp2/2h2H3/6PCP/3AE4/2C6/3A1K1H1 w - - 0 1", {7, 281, 8620, 326201}},
      {"5a3/3k5/3aR4/9/5r3/5h3/9/3A1A3/5K3/2EC2E2 w - - 0 1", {25, 424, 9850, 202884}},
      {"CRH1k1e2/3ca4/4ea3/9/2hr5/9/9/4E4/4A4/4KA3 w - - 0 1", {28, 516, 14808, 395483}},
      {"R1H1k1e2/9/3aea3/9/2hr5/2E6/9/4E4/4A4/4KA3 w - - 0 1", {21, 364, 7626, 162837}},
      {"C1hHk4/9/9/9/9/9/h1pp5/E3C4/9/3A1K3 w - - 0 1", {28, 222, 6241, 64971}},
      {"4ka3/4a4/9/9/4H4/p8/9/4C3c/7h1/2EK5 w - - 0 1", {23, 345, 8124, 149272}},
      {"2e1ka3/9/e3H4/4h4/9/9/9/4C4/2p6/2EK5 w - - 0 1", {21, 195, 3883, 48060}},
      {"1C2ka3/9/C1Hae1h2/p3p3p/6p2/9/P3P3P/3AE4/3p2c2/c1EAK4 w - - 0 1", {30, 830, 22787, 649866}},
      {"ChH1k1e2/c3a4/4ea3/9/2hr5/9/9/4C4/4A4/4KA3 w - - 0 1", {19, 583, 11714, 376467}},
  };
  for (const Case& c : kCases) {
    const Board board = fromFen(c.fen);
    for (int depth = 1; depth <= 4; ++depth) {
      const uint64_t got = perft(board, depth);
      if (got != c.nodes[depth - 1]) {
        std::printf("  %s depth %d: %llu, want %llu\n", c.fen, depth, static_cast<unsigned long long>(got),
                    static_cast<unsigned long long>(c.nodes[depth - 1]));
      }
      CHECK(got == c.nodes[depth - 1]);
    }
  }
}

void fenRefusesWhatIsNotABoard() {
  Board board{};
  CHECK(!parseFen(nullptr, board));
  CHECK(!parseFen("rnbakabnr/9 w", board));
  CHECK(!parseFen("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNRR w", board));
  CHECK(!parseFen("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR x", board));
  CHECK(!parseFen("rnbakabnr/9/1c5c1/p1p1p1p1q/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w", board));
  // Both letter sets name the same pieces.
  Board a{};
  Board b{};
  CHECK(parseFen("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w", a));
  CHECK(parseFen("rheakaehr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RHEAKAEHR w", b));
  CHECK(std::memcmp(&a, &b, sizeof(Board)) == 0);
}

void thePalaceHasTwoEdges() {
  // Regression: inPalace once bounded Red's palace below only, and the
  // opening generated moves onto a rank 10 that does not exist.
  for (int file = 0; file < kFiles; ++file) {
    CHECK(!inPalace(file, 10, kRed));
    CHECK(!inPalace(file, -1, kBlack));
  }
  CHECK(inPalace(4, 9, kRed));
  CHECK(inPalace(3, 7, kRed));
  CHECK(!inPalace(2, 8, kRed));
  CHECK(inPalace(5, 0, kBlack));
  CHECK(!inPalace(4, 3, kBlack));
  Board board{};
  start(board);
  CHECK(movesFrom(board, 4, 9) == 1);  // the general: one step forward
  CHECK(movesFrom(board, 3, 9) == 1);  // each advisor: into the centre
}

void theGeneralsMayNotFace() {
  // Red's general may not step onto the open e-file facing Black's.
  const Board board = fromFen("4k4/9/9/9/9/9/9/9/9/3K5 w");
  CHECK(!hasMove(board, mv(3, 9, 4, 9)));
  CHECK(hasMove(board, mv(3, 9, 3, 8)));
  CHECK(movesFrom(board, 3, 9) == 1);
  // A piece between them on the file may not step aside.
  const Board shielded = fromFen("4k4/9/9/9/9/4R4/9/9/9/4K4 w");
  CHECK(!hasMove(shielded, mv(4, 5, 3, 5)));
  CHECK(hasMove(shielded, mv(4, 5, 4, 6)));
  // ...and a general facing another counts as attacked.
  const Board facing = fromFen("4k4/9/9/9/9/9/9/9/9/4K4 w");
  CHECK(isAttacked(facing, indexOf(4, 9), kBlack));
}

void theCannonTakesOnlyOverAScreen() {
  // Red cannon at e2; a black soldier at e5 is the screen, the chariot at e7 is
  // the target behind it.
  const Board board = fromFen("3k5/9/4r4/9/4p4/9/9/4C4/9/5K3 w");
  CHECK(hasMove(board, mv(4, 7, 4, 2)));   // over the screen onto the chariot
  CHECK(!hasMove(board, mv(4, 7, 4, 4)));  // not the screen itself
  CHECK(!hasMove(board, mv(4, 7, 4, 3)));  // not past the screen without a capture
  CHECK(hasMove(board, mv(4, 7, 4, 5)));   // quiet moves up to the screen
  // A cannon with no screen cannot take at all.
  const Board bare = fromFen("3k5/9/4r4/9/9/9/9/4C4/9/5K3 w");
  CHECK(!hasMove(bare, mv(4, 7, 4, 2)));
}

void theHorseIsStoppedByItsLeg() {
  // Red horse at e4 (rank 5) with a piece directly above it: both upward jumps
  // are blocked, the others are free.
  const Board board = fromFen("3k5/9/9/9/4P4/4H4/9/9/9/5K3 w");
  CHECK(!hasMove(board, mv(4, 5, 3, 3)));
  CHECK(!hasMove(board, mv(4, 5, 5, 3)));
  CHECK(hasMove(board, mv(4, 5, 3, 7)));
  CHECK(hasMove(board, mv(4, 5, 2, 4)));
  CHECK(hasMove(board, mv(4, 5, 6, 6)));
}

void theElephantStaysHomeAndIsBlockedByItsEye() {
  const Board board = fromFen("3k5/9/9/9/9/9/9/9/9/2E2K3 w");
  CHECK(hasMove(board, mv(2, 9, 0, 7)));
  CHECK(hasMove(board, mv(2, 9, 4, 7)));
  // From the river bank it may not cross.
  const Board bank = fromFen("3k5/9/9/9/9/4E4/9/9/9/5K3 w");
  CHECK(!hasMove(bank, mv(4, 5, 2, 3)));
  CHECK(!hasMove(bank, mv(4, 5, 6, 3)));
  CHECK(hasMove(bank, mv(4, 5, 2, 7)));
  // A piece on the eye blocks it.
  const Board eye = fromFen("3k5/9/9/9/9/9/9/9/3P5/2E2K3 w");
  CHECK(!hasMove(eye, mv(2, 9, 4, 7)));
  CHECK(hasMove(eye, mv(2, 9, 0, 7)));
}

void theSoldierTurnsOnlyAcrossTheRiver() {
  const Board home = fromFen("3k5/9/9/9/9/9/4P4/9/9/5K3 w");
  CHECK(movesFrom(home, 4, 6) == 1);
  const Board across = fromFen("3k5/9/9/4P4/9/9/9/9/9/5K3 w");
  CHECK(movesFrom(across, 4, 3) == 3);
  CHECK(!hasMove(across, mv(4, 3, 4, 4)));  // never backwards
}

void noMovesLosesEvenWithoutCheck() {
  // Black's general is boxed in: its only steps face Red's general or are
  // covered by Red's chariot, and it is not in check.
  Game game = gameAt("3k5/R8/9/9/9/9/9/9/9/4K4 b");
  CHECK(!inCheck(game.board, kBlack));
  CHECK(outcome(game) == Outcome::RedWins);
  CHECK(endingOf(game, outcome(game)) == Ending::Stalemate);
}

void checkmateEndsTheGame() {
  Game game = gameAt("3k5/1R7/9/9/9/9/9/9/9/R3K4 w");
  CHECK(play(game, mv(0, 9, 0, 0)));
  CHECK(outcome(game) == Outcome::RedWins);
  CHECK(endingOf(game, outcome(game)) == Ending::Checkmate);
}

void threeRepeatsIsADraw() {
  Game game{};
  startGame(game);
  const Move a = mv(0, 9, 0, 8);
  const Move b = mv(0, 0, 0, 1);
  const Move c = mv(0, 8, 0, 9);
  const Move d = mv(0, 1, 0, 0);
  for (int cycle = 0; cycle < 2; ++cycle) {
    CHECK(outcome(game) == Outcome::Running);
    CHECK(play(game, a));
    CHECK(play(game, b));
    CHECK(play(game, c));
    CHECK(play(game, d));
  }
  CHECK(repetitions(game) == 3);
  CHECK(outcome(game) == Outcome::Draw);
  CHECK(endingOf(game, outcome(game)) == Ending::Repetition);
}

void aCaptureForgetsOldPositions() {
  // Positions before a capture can never come back, so the ring restarts.
  Game game = gameAt("3k5/9/9/9/9/9/9/4p4/4R4/5K3 w");
  game.keyCount = 5;
  game.idlePlies = 30;
  CHECK(play(game, mv(4, 8, 4, 7)));
  CHECK(game.keyCount == 1);
  CHECK(game.idlePlies == 0);
}

void sixtyQuietMovesEachIsADraw() {
  Game game = gameAt("3k5/9/9/9/9/9/9/9/R8/5K3 w");
  game.idlePlies = kIdleLimit;
  CHECK(outcome(game) == Outcome::Draw);
  CHECK(endingOf(game, outcome(game)) == Ending::NoCaptures);
}

void playRefusesWhatIsNotLegal() {
  Game game{};
  startGame(game);
  const Game before = game;
  CHECK(!play(game, mv(4, 6, 4, 4)));  // a soldier two points
  CHECK(!play(game, mv(4, 3, 4, 4)));  // Black's soldier on Red's move
  CHECK(!play(game, mv(3, 9, 3, 8)));  // an advisor stepping straight
  CHECK(std::memcmp(&before, &game, sizeof(Game)) == 0);
  CHECK(play(game, mv(7, 7, 4, 7)));  // h2e2, the central cannon
  CHECK(game.board.turn == kBlack);
  CHECK(game.lastTo == indexOf(4, 7));
}

void theWireStateFitsOnePacket() {
  static_assert(std::is_trivially_copyable<Game>::value, "the link copies Game as bytes");
  static_assert(sizeof(Game) <= 192, "a shared game state must fit one packet");
  CHECK(sizeof(Game) <= 192);
}

void iccsNotation() {
  char out[6];
  moveToIccs(mv(7, 7, 4, 7), out);
  CHECK(std::strcmp(out, "h2e2") == 0);
  moveToIccs(mv(1, 9, 2, 7), out);
  CHECK(std::strcmp(out, "b0c2") == 0);
}

// ---------------------------------------------------------------------------

SearchBuffers buffers;

void theEngineFindsMate() {
  const Board board = fromFen("3k5/1R7/9/9/9/9/9/9/9/R3K4 w");
  const SearchResult result = search(board, 3, buffers, 0);
  CHECK(result.hasMove);
  CHECK(result.score >= kMateThreshold);
  Game game = gameAt("3k5/1R7/9/9/9/9/9/9/9/R3K4 w");
  CHECK(play(game, result.best));
  CHECK(outcome(game) == Outcome::RedWins);
}

void theEngineTakesAHangingChariot() {
  // Black's chariot stands undefended on Red's chariot's file.
  const Board board = fromFen("3k5/9/9/9/9/r8/9/9/9/R3K4 w");
  const SearchResult result = search(board, 3, buffers, 0);
  CHECK(sameMove(result.best, mv(0, 9, 0, 5)));
}

void theEngineNeverPlaysAnIllegalMove() {
  Game game{};
  startGame(game);
  for (int ply = 0; ply < 60 && outcome(game) == Outcome::Running; ++ply) {
    const SearchResult result = search(game.board, 4, buffers, 8000);
    CHECK(result.hasMove);
    CHECK(result.nodes <= 8001);
    CHECK(play(game, result.best));
  }
}

void theEngineIsDeterministic() {
  Board board{};
  start(board);
  const SearchResult a = search(board, 4, buffers, 30000);
  const SearchResult b = search(board, 4, buffers, 30000);
  CHECK(sameMove(a.best, b.best));
  CHECK(a.nodes == b.nodes);
}

// ---------------------------------------------------------------------------

void targetsComeFromTheLegalList() {
  const Board board = fromFen("3k5/9/4r4/9/4p4/9/9/4C4/9/5K3 w");
  MoveList legal;
  generateLegal(board, legal);
  Targets targets;
  targetsFor(board, legal, indexOf(4, 7), targets);
  int captures = 0;
  for (int i = 0; i < targets.count; ++i) {
    if (targets.capture[i]) {
      ++captures;
      CHECK(targets.to[i] == indexOf(4, 2));
    }
  }
  CHECK(captures == 1);
  CHECK(canPick(legal, indexOf(4, 7)));
  CHECK(!canPick(legal, indexOf(4, 4)));  // their soldier
  Move move{};
  CHECK(moveBetween(legal, indexOf(4, 7), indexOf(4, 2), move));
  CHECK(!moveBetween(legal, indexOf(4, 7), indexOf(4, 4), move));
}

void lostPiecesAreCounted() {
  Board board{};
  start(board);
  CHECK(countOf(board, kRed, Soldier) == 5);
  CHECK(countOf(board, kBlack, Cannon) == 2);
  board.cell[indexOf(1, 7)] = 0;
  CHECK(countOf(board, kRed, Cannon) == 1);
}

// ---------------------------------------------------------------------------

void theSaveRoundTrips() {
  xiangqisave::Save save;
  save.wins = 7;
  save.losses = 1;
  save.draws = 2;
  save.inProgress = true;
  save.opponent = Opponent::Computer;
  save.seat = kRed;
  startGame(save.game);
  play(save.game, mv(7, 7, 4, 7));
  char line[xiangqisave::kMaxLine];
  const int bytes = xiangqisave::pack(save, line, sizeof(line));
  CHECK(bytes > 0);
  xiangqisave::Save back;
  CHECK(xiangqisave::unpack(line, back));
  CHECK(back.wins == 7 && back.losses == 1 && back.draws == 2 && back.inProgress);
  CHECK(std::memcmp(&back.game.board, &save.game.board, sizeof(Board)) == 0);
  CHECK(back.game.plies == 1);
  CHECK(xiangqisave::pack(save, line, bytes) == 0);
}

void theSaveRefusesWhatCannotBeAGame() {
  xiangqisave::Save save;
  startGame(save.game);
  char line[xiangqisave::kMaxLine];
  xiangqisave::Save out;
  out.wins = 42;

  xiangqisave::Save bad = save;
  bad.game.board.cell[indexOf(0, 4)] = makeCell(Elephant, kRed);  // across the river
  CHECK(xiangqisave::pack(bad, line, sizeof(line)) > 0);
  CHECK(!xiangqisave::unpack(line, out));

  bad = save;
  bad.game.board.cell[indexOf(4, 9)] = 0;  // no red general
  CHECK(xiangqisave::pack(bad, line, sizeof(line)) > 0);
  CHECK(!xiangqisave::unpack(line, out));

  bad = save;
  bad.game.board.cell[indexOf(4, 8)] = makeCell(Soldier, kRed);  // behind its start
  CHECK(xiangqisave::pack(bad, line, sizeof(line)) > 0);
  CHECK(!xiangqisave::unpack(line, out));

  CHECK(xiangqisave::pack(save, line, sizeof(line)) > 0);
  line[3] = 'X';
  CHECK(!xiangqisave::unpack(line, out));
  CHECK(out.wins == 42);
}

}  // namespace

int main() {
  perftStartPosition();
  perftTacticalPositions();
  fenRefusesWhatIsNotABoard();
  thePalaceHasTwoEdges();
  theGeneralsMayNotFace();
  theCannonTakesOnlyOverAScreen();
  theHorseIsStoppedByItsLeg();
  theElephantStaysHomeAndIsBlockedByItsEye();
  theSoldierTurnsOnlyAcrossTheRiver();
  noMovesLosesEvenWithoutCheck();
  checkmateEndsTheGame();
  threeRepeatsIsADraw();
  aCaptureForgetsOldPositions();
  sixtyQuietMovesEachIsADraw();
  playRefusesWhatIsNotLegal();
  theWireStateFitsOnePacket();
  iccsNotation();
  theEngineFindsMate();
  theEngineTakesAHangingChariot();
  theEngineNeverPlaysAnIllegalMove();
  theEngineIsDeterministic();
  targetsComeFromTheLegalList();
  lostPiecesAreCounted();
  theSaveRoundTrips();
  theSaveRefusesWhatCannotBeAGame();
  std::printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}
