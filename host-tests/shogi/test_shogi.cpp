// Shogi rules, engine, flow and save tests. Freestanding: no device, no
// PlatformIO. Run with host-tests/shogi/run.sh.
//
// Perft is the backbone, against published counts: the start position (which
// exercises nothing interesting) AND the "Matsuri" position, whose hands are
// full, so drops, nifu, dead-end squares and promotion all combine. The named
// rule tests below then pin each rule on its own, so a perft failure can be
// read as "which rule" rather than "something somewhere".

#include <cstdio>
#include <cstring>
#include <type_traits>

#include "ShogiCore.h"
#include "ShogiEngine.h"
#include "ShogiFlow.h"
#include "ShogiSave.h"

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

using namespace shogi;

Board fromSfen(const char* sfen) {
  Board board{};
  const bool ok = parseSfen(sfen, board);
  check(ok, sfen, __LINE__);
  return board;
}

// A Game standing on an arbitrary position, with that position remembered once.
Game gameAt(const char* sfen) {
  Game game{};
  startGame(game);
  game.board = fromSfen(sfen);
  game.keyCount = 0;
  game.keyHead = 0;
  game.keys[0] = positionKey(game.board);
  game.keyCount = 1;
  game.keyHead = 1;
  return game;
}

bool hasMove(const Board& board, const Move& want) {
  MoveList list;
  generateLegal(board, list);
  for (int i = 0; i < list.count; ++i) {
    if (sameMove(list.moves[i], want)) return true;
  }
  return false;
}

int movesFrom(const Board& board, const int from) {
  MoveList list;
  generateLegal(board, list);
  int n = 0;
  for (int i = 0; i < list.count; ++i) {
    if (!isDrop(list.moves[i]) && list.moves[i].from == from) ++n;
  }
  return n;
}

Move boardMove(const int from, const int to, const bool promote) {
  return Move{static_cast<uint8_t>(from), static_cast<uint8_t>(to), None, static_cast<uint8_t>(promote ? 1 : 0)};
}

Move dropMove(const uint8_t type, const int to) { return Move{kDropFrom, static_cast<uint8_t>(to), type, 0}; }

// ---------------------------------------------------------------------------

void perftStartPosition() {
  Board board{};
  start(board);
  // Published by Patrice Duhamel and confirmed by others on CCC, "Perft for
  // Xiangqi & Shogi" (talkchess, 2016).
  CHECK(perft(board, 1) == 30);
  CHECK(perft(board, 2) == 900);
  CHECK(perft(board, 3) == 25470);
  CHECK(perft(board, 4) == 719731);
}

void perftMatsuri() {
  // The standard drop-heavy perft position. Both hands are full.
  const Board board = fromSfen("l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w RGgsn5p 1");
  CHECK(perft(board, 1) == 207);
  CHECK(perft(board, 2) == 28684);
  CHECK(perft(board, 3) == 4809015);
}

void sfenRefusesWhatIsNotABoard() {
  Board board{};
  CHECK(!parseSfen(nullptr, board));
  CHECK(!parseSfen("", board));
  CHECK(!parseSfen("lnsgkgsnl/1r5b1 b - 1", board));                                             // too few ranks
  CHECK(!parseSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNLL b - 1", board));  // long rank
  CHECK(!parseSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1", board));   // side
  CHECK(!parseSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSG+KGSNL b - 1", board));  // +K
  CHECK(!parseSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b K 1", board));   // K in hand
}

void nifuForbidsASecondPawnOnAFile() {
  // A Sente pawn already stands on file index 4.
  const Board board = fromSfen("4k4/9/9/9/9/9/4P4/9/4K4 b P 1");
  for (int rank = 1; rank < kRanks; ++rank) {
    const int to = indexOf(4, rank);
    if (board.cell[to] != 0) continue;
    CHECK(!hasMove(board, dropMove(Pawn, to)));
  }
  CHECK(hasMove(board, dropMove(Pawn, indexOf(3, 4))));
  // A TOKIN is not a pawn for nifu: the file is open again.
  const Board tokin = fromSfen("4k4/9/9/9/4+P4/9/9/9/4K4 b P 1");
  CHECK(hasMove(tokin, dropMove(Pawn, indexOf(4, 6))));
  // Gote's pawns do not count against Sente.
  const Board theirs = fromSfen("4k4/4p4/9/9/9/9/9/9/4K4 b P 1");
  CHECK(hasMove(theirs, dropMove(Pawn, indexOf(4, 5))));
}

void nothingIsDroppedWhereItCouldNeverMove() {
  const Board board = fromSfen("4k4/9/9/9/9/9/9/9/4K4 b NLP 1");
  for (int file = 0; file < kFiles; ++file) {
    if (board.cell[indexOf(file, 0)] != 0) continue;
    CHECK(!hasMove(board, dropMove(Pawn, indexOf(file, 0))));
    CHECK(!hasMove(board, dropMove(Lance, indexOf(file, 0))));
    CHECK(!hasMove(board, dropMove(Knight, indexOf(file, 0))));
    CHECK(!hasMove(board, dropMove(Knight, indexOf(file, 1))));
    CHECK(hasMove(board, dropMove(Lance, indexOf(file, 1))));
    CHECK(hasMove(board, dropMove(Knight, indexOf(file, 2))));
  }
  // The same, mirrored, for Gote.
  const Board gote = fromSfen("4k4/9/9/9/9/9/9/9/4K4 w nlp 1");
  CHECK(!hasMove(gote, dropMove(Pawn, indexOf(0, 8))));
  CHECK(!hasMove(gote, dropMove(Knight, indexOf(0, 7))));
  CHECK(hasMove(gote, dropMove(Knight, indexOf(0, 6))));
}

void promotionIsForcedOnlyWhereItMustBe() {
  // A pawn stepping onto the last rank must promote: only the promoted move.
  const Board last = fromSfen("4k4/P8/9/9/9/9/9/9/4K4 b - 1");
  CHECK(hasMove(last, boardMove(indexOf(0, 1), indexOf(0, 0), true)));
  CHECK(!hasMove(last, boardMove(indexOf(0, 1), indexOf(0, 0), false)));
  // Entering the zone short of the last rank: the player chooses.
  const Board zone = fromSfen("4k4/9/9/P8/9/9/9/9/4K4 b - 1");
  CHECK(hasMove(zone, boardMove(indexOf(0, 3), indexOf(0, 2), true)));
  CHECK(hasMove(zone, boardMove(indexOf(0, 3), indexOf(0, 2), false)));
  // A knight landing on the second rank must promote too.
  const Board knight = fromSfen("4k4/9/9/1N7/9/9/9/9/4K4 b - 1");
  CHECK(hasMove(knight, boardMove(indexOf(1, 3), indexOf(0, 1), true)));
  CHECK(!hasMove(knight, boardMove(indexOf(1, 3), indexOf(0, 1), false)));
  // Leaving the zone is a promotion chance too.
  const Board leaving = fromSfen("4k4/9/R8/9/9/9/9/9/4K4 b - 1");
  CHECK(hasMove(leaving, boardMove(indexOf(0, 2), indexOf(0, 5), true)));
  // Golds and kings never promote.
  const Board gold = fromSfen("4k4/9/9/G8/9/9/9/9/4K4 b - 1");
  CHECK(!hasMove(gold, boardMove(indexOf(0, 3), indexOf(0, 2), true)));
  CHECK(hasMove(gold, boardMove(indexOf(0, 3), indexOf(0, 2), false)));
}

void aDroppedPawnMayNotMate() {
  // Gote's king in the corner; a Sente silver covers its one flight square and
  // a gold covers the rest and protects the landing square. A pawn dropped in
  // front of the king would be mate, so it is not a legal move.
  const Board mated = fromSfen("8k/6S2/7G1/9/9/9/9/9/4K4 b P 1");
  CHECK(!hasMove(mated, dropMove(Pawn, indexOf(8, 1))));
  // Other pawn drops are still fine.
  CHECK(hasMove(mated, dropMove(Pawn, indexOf(2, 4))));
  // Without the silver the king escapes, so the same drop is only a check.
  const Board escapes = fromSfen("8k/9/7G1/9/9/9/9/9/4K4 b P 1");
  CHECK(hasMove(escapes, dropMove(Pawn, indexOf(8, 1))));
  // Mate by PUSHING a pawn is perfectly legal; the rule is about drops.
  const Board push = fromSfen("8k/6S2/7GP/9/9/9/9/9/4K4 b - 1");
  const Move pushMate = boardMove(indexOf(8, 2), indexOf(8, 1), false);
  CHECK(hasMove(push, pushMate));
  Board after = push;
  makeMove(after, pushMate);
  CHECK(!hasLegalMove(after));
  CHECK(inCheck(after, kGote));
}

void youMayNotLeaveYourKingAttacked() {
  // A Sente rook pins Gote's gold to its king along the file.
  const Board board = fromSfen("4k4/4g4/9/9/9/9/9/9/4RK3 w - 1");
  // The gold may only move along the file (toward the rook it shields).
  MoveList list;
  generateLegal(board, list);
  for (int i = 0; i < list.count; ++i) {
    if (isDrop(list.moves[i]) || list.moves[i].from != indexOf(4, 1)) continue;
    CHECK(fileOf(list.moves[i].to) == 4);
  }
  CHECK(movesFrom(board, indexOf(4, 1)) == 1);
}

void noMovesLosesEvenWithoutCheck() {
  // Shogi has no stalemate. A king boxed in by its own side's... a lone king
  // with every flight square attacked and not in check has no move and loses.
  Game game = gameAt("k8/2G6/1G7/9/9/9/9/9/4K4 w - 1");
  CHECK(!inCheck(game.board, kGote));
  CHECK(!hasLegalMove(game.board));
  CHECK(outcome(game) == Outcome::SenteWins);
  CHECK(endingOf(game, outcome(game)) == Ending::NoMoves);
}

void checkmateEndsTheGame() {
  Game game = gameAt("4k4/9/4P4/9/9/9/9/9/4K4 b G 1");
  CHECK(outcome(game) == Outcome::Running);
  CHECK(play(game, dropMove(Gold, indexOf(4, 1))));
  CHECK(outcome(game) == Outcome::SenteWins);
  CHECK(endingOf(game, outcome(game)) == Ending::Checkmate);
}

void playRefusesWhatIsNotLegal() {
  Game game{};
  startGame(game);
  const Game before = game;
  CHECK(!play(game, boardMove(indexOf(4, 6), indexOf(4, 4), false)));  // a pawn two squares
  CHECK(!play(game, dropMove(Pawn, indexOf(4, 4))));                   // nothing in hand
  CHECK(!play(game, boardMove(indexOf(4, 2), indexOf(4, 3), false)));  // Gote's pawn, Sente to move
  CHECK(std::memcmp(&before, &game, sizeof(Game)) == 0);
  CHECK(play(game, boardMove(indexOf(2, 6), indexOf(2, 5), false)));  // 7g7f
  CHECK(game.plies == 1);
  CHECK(game.lastTo == indexOf(2, 5));
  CHECK(game.board.turn == kGote);
}

void capturesGoToTheHandUnpromoted() {
  // A Sente rook takes a Gote dragon: Sente gains a ROOK in hand.
  Board board = fromSfen("4k4/9/9/9/9/9/+r8/9/R3K4 b - 1");
  makeMove(board, boardMove(indexOf(0, 8), indexOf(0, 6), false));
  CHECK(board.hand[kSente][Rook] == 1);
  CHECK(board.cell[indexOf(0, 6)] == makeCell(Rook, kSente));
  // And a drop takes it back out of the hand.
  board.turn = kSente;
  makeMove(board, dropMove(Rook, indexOf(5, 5)));
  CHECK(board.hand[kSente][Rook] == 0);
}

void fourRepeatsIsSennichite() {
  Game game{};
  startGame(game);
  const Move out1 = boardMove(indexOf(4, 8), indexOf(4, 7), false);
  const Move out2 = boardMove(indexOf(4, 0), indexOf(4, 1), false);
  const Move back1 = boardMove(indexOf(4, 7), indexOf(4, 8), false);
  const Move back2 = boardMove(indexOf(4, 1), indexOf(4, 0), false);
  for (int cycle = 0; cycle < 3; ++cycle) {
    CHECK(outcome(game) == Outcome::Running);
    CHECK(play(game, out1));
    CHECK(play(game, out2));
    CHECK(play(game, back1));
    CHECK(play(game, back2));
  }
  // The opening position has now occurred four times.
  CHECK(repetitions(game) == 4);
  CHECK(outcome(game) == Outcome::Draw);
  CHECK(endingOf(game, outcome(game)) == Ending::Repetition);
}

void theWireStateFitsOnePacket() {
  static_assert(std::is_trivially_copyable<Game>::value, "the link copies Game as bytes");
  static_assert(sizeof(Game) <= 192, "a shared game state must fit one packet");
  CHECK(sizeof(Game) <= 192);
}

void usiNotation() {
  char out[8];
  moveToUsi(boardMove(indexOf(2, 6), indexOf(2, 5), false), out);
  CHECK(std::strcmp(out, "7g7f") == 0);
  moveToUsi(boardMove(indexOf(1, 7), indexOf(7, 1), true), out);
  CHECK(std::strcmp(out, "8h2b+") == 0);
  moveToUsi(dropMove(Pawn, indexOf(4, 4)), out);
  CHECK(std::strcmp(out, "P*5e") == 0);
}

// ---------------------------------------------------------------------------

SearchBuffers buffers;

void theEngineFindsAMateByDrop() {
  const Board board = fromSfen("4k4/9/4P4/9/9/9/9/9/4K4 b G 1");
  const SearchResult result = search(board, 3, buffers, 0);
  CHECK(result.hasMove);
  CHECK(sameMove(result.best, dropMove(Gold, indexOf(4, 1))));
  CHECK(result.score >= kMateThreshold);
}

void theEngineTakesFreeMaterial() {
  // A Gote rook hangs to a Sente bishop's diagonal.
  const Board board = fromSfen("4k4/9/9/9/9/9/9/1r7/B3K4 b - 1");
  const SearchResult result = search(board, 2, buffers, 0);
  CHECK(result.hasMove);
  CHECK(sameMove(result.best, boardMove(indexOf(0, 8), indexOf(1, 7), false)) ||
        sameMove(result.best, boardMove(indexOf(0, 8), indexOf(1, 7), true)));
}

void theEngineNeverPlaysAnIllegalMove() {
  // Play the engine against itself for a while; every move it picks must be
  // one play() accepts, and the budget must hold.
  Game game{};
  startGame(game);
  for (int ply = 0; ply < 40 && outcome(game) == Outcome::Running; ++ply) {
    const SearchResult result = search(game.board, 3, buffers, 5000);
    CHECK(result.hasMove);
    // One node past the budget at most: the check happens as each node starts.
    CHECK(result.nodes <= 5001);
    CHECK(play(game, result.best));
  }
}

void theEngineIsDeterministic() {
  Board board{};
  start(board);
  const SearchResult a = search(board, 3, buffers, 20000);
  const SearchResult b = search(board, 3, buffers, 20000);
  CHECK(sameMove(a.best, b.best));
  CHECK(a.nodes == b.nodes);
  CHECK(a.score == b.score);
}

void aMatedSideHasNoMove() {
  Board board = fromSfen("4k4/9/4P4/9/9/9/9/9/4K4 b G 1");
  makeMove(board, dropMove(Gold, indexOf(4, 1)));
  const SearchResult result = search(board, 3, buffers, 0);
  CHECK(!result.hasMove);
}

// ---------------------------------------------------------------------------

void targetsSayWhichLandingsAskAboutPromotion() {
  const Board board = fromSfen("4k4/9/9/P8/9/9/9/9/4K4 b - 1");
  MoveList legal;
  generateLegal(board, legal);
  Pick pick;
  pick.from = static_cast<uint8_t>(indexOf(0, 3));
  Targets targets;
  targetsFor(board, legal, pick, targets);
  CHECK(targets.count == 1);
  Promotion promotion = Promotion::None;
  CHECK(promotionAt(targets, indexOf(0, 2), promotion));
  CHECK(promotion == Promotion::Optional);
  CHECK(!promotionAt(targets, indexOf(0, 4), promotion));

  const Board last = fromSfen("4k4/P8/9/9/9/9/9/9/4K4 b - 1");
  generateLegal(last, legal);
  pick.from = static_cast<uint8_t>(indexOf(0, 1));
  targetsFor(last, legal, pick, targets);
  CHECK(targets.count == 1);
  CHECK(promotionAt(targets, indexOf(0, 0), promotion));
  CHECK(promotion == Promotion::Forced);

  Move move{};
  CHECK(moveFor(legal, pick, indexOf(0, 0), true, move));
  CHECK(!moveFor(legal, pick, indexOf(0, 0), false, move));
}

void handPicksFollowTheRules() {
  // A pawn in hand that can go nowhere legal cannot be lifted.
  const Board board = fromSfen("4k4/9/9/9/9/9/PPPPPPPPP/9/4K4 b P 1");
  MoveList legal;
  generateLegal(board, legal);
  CHECK(!canPickHand(legal, Pawn));
  const Board open = fromSfen("4k4/9/9/9/9/9/9/9/4K4 b P 1");
  generateLegal(open, legal);
  CHECK(canPickHand(legal, Pawn));
  Pick pick;
  pick.from = kDropFrom;
  pick.drop = Pawn;
  Targets targets;
  targetsFor(open, legal, pick, targets);
  // Eight ranks (not the last) on every file, minus Sente's own king's square.
  // Gote's king stands on the last rank, which a pawn drop never reaches.
  CHECK(targets.count == kFiles * (kRanks - 1) - 1);
}

// ---------------------------------------------------------------------------

void theSaveRoundTrips() {
  shogisave::Save save;
  save.wins = 3;
  save.losses = 2;
  save.draws = 1;
  save.inProgress = true;
  save.opponent = Opponent::Friend;
  save.seat = kSente;
  startGame(save.game);
  play(save.game, boardMove(indexOf(2, 6), indexOf(2, 5), false));
  char line[shogisave::kMaxLine];
  const int bytes = shogisave::pack(save, line, sizeof(line));
  CHECK(bytes > 0);
  shogisave::Save back;
  CHECK(shogisave::unpack(line, back));
  CHECK(back.wins == 3 && back.losses == 2 && back.draws == 1);
  CHECK(back.inProgress);
  CHECK(back.opponent == Opponent::Friend);
  CHECK(std::memcmp(&back.game.board, &save.game.board, sizeof(Board)) == 0);
  CHECK(back.game.plies == 1);
  CHECK(back.game.lastTo == save.game.lastTo);
  // A buffer one byte too small refuses rather than truncating.
  CHECK(shogisave::pack(save, line, bytes) == 0);
}

void theSaveRefusesWhatCannotBeAGame() {
  shogisave::Save save;
  startGame(save.game);
  char line[shogisave::kMaxLine];
  CHECK(shogisave::pack(save, line, sizeof(line)) > 0);
  shogisave::Save out;
  out.wins = 99;

  char wrongMagic[shogisave::kMaxLine];
  std::memcpy(wrongMagic, line, sizeof(line));
  wrongMagic[5] = '9';
  CHECK(!shogisave::unpack(wrongMagic, out));

  char truncated[shogisave::kMaxLine];
  std::memcpy(truncated, line, sizeof(line));
  truncated[std::strlen(truncated) / 2] = '\0';
  CHECK(!shogisave::unpack(truncated, out));

  // Three kings: a board no game could reach.
  shogisave::Save bad = save;
  bad.game.board.cell[indexOf(0, 4)] = makeCell(King, kSente);
  CHECK(shogisave::pack(bad, line, sizeof(line)) > 0);
  CHECK(!shogisave::unpack(line, out));

  // Nineteen pawns.
  bad = save;
  bad.game.board.hand[kSente][Pawn] = 1;
  CHECK(shogisave::pack(bad, line, sizeof(line)) > 0);
  CHECK(!shogisave::unpack(line, out));

  // Nothing committed by any refusal.
  CHECK(out.wins == 99);
}

}  // namespace

int main() {
  perftStartPosition();
  perftMatsuri();
  sfenRefusesWhatIsNotABoard();
  nifuForbidsASecondPawnOnAFile();
  nothingIsDroppedWhereItCouldNeverMove();
  promotionIsForcedOnlyWhereItMustBe();
  aDroppedPawnMayNotMate();
  youMayNotLeaveYourKingAttacked();
  noMovesLosesEvenWithoutCheck();
  checkmateEndsTheGame();
  playRefusesWhatIsNotLegal();
  capturesGoToTheHandUnpromoted();
  fourRepeatsIsSennichite();
  theWireStateFitsOnePacket();
  usiNotation();
  theEngineFindsAMateByDrop();
  theEngineTakesFreeMaterial();
  theEngineNeverPlaysAnIllegalMove();
  theEngineIsDeterministic();
  aMatedSideHasNoMove();
  targetsSayWhichLandingsAskAboutPromotion();
  handPicksFollowTheRules();
  theSaveRoundTrips();
  theSaveRefusesWhatCannotBeAGame();
  std::printf("%d checks, %d failed\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}
