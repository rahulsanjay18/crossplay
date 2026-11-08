#include "ShogiActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "ShogiSave.h"
#include "ShogiScreens.h"

namespace sh = shogi;

namespace {

#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kSavePath[] = "/.crosspoint/shogi.sav";
constexpr char kSaveTempPath[] = "/.crosspoint/shogi.sav.tmp";
#endif

// Depth 4 with a node cap. The cap is the real limit on the device: the search
// is iterative, so when it runs out it plays the best move of the deepest
// iteration it finished rather than stalling the loop.
constexpr int kEngineDepth = 4;
constexpr uint32_t kEngineNodes = 60000;

}  // namespace

std::unique_ptr<Activity> ShogiActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ShogiActivity>(renderer, mappedInput);
}

void ShogiActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = sh::Screen::Menu;
  sh::startGame(game);
  refresh();
  loadSave();
  requestUpdate();
}

void ShogiActivity::onExit() {
  writeSave();
  LinkActivity::onExit();
}

void ShogiActivity::loadSave() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  canContinue = false;
  if (!Storage.exists(kSavePath)) return;
  char buffer[shogisave::kMaxLine + 8] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer)) == 0) return;
  shogisave::Save save;
  // A save that does not parse costs a resume, not a crash: the menu simply
  // offers no CONTINUE.
  if (!shogisave::unpack(buffer, save)) {
    LOG_ERR("SHOGI", "Ignoring an unreadable save");
    return;
  }
  wins = save.wins;
  losses = save.losses;
  draws = save.draws;
  if (save.inProgress) {
    game = save.game;
    opponent = save.opponent;
    seat = save.seat;
    refresh();
    canContinue = cachedOutcome == sh::Outcome::Running;
  }
#endif
}

void ShogiActivity::writeSave() {
  // Never during a match: the board on screen is the shared game, and this file
  // is what a solo game resumes from.
  if (inMatch()) return;
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  shogisave::Save save;
  save.wins = wins;
  save.losses = losses;
  save.draws = draws;
  save.inProgress = canContinue;
  save.opponent = opponent;
  save.seat = seat;
  save.game = game;
  char line[shogisave::kMaxLine];
  if (shogisave::pack(save, line, sizeof(line)) <= 0) {
    LOG_ERR("SHOGI", "Save line did not fit %d bytes", static_cast<int>(sizeof(line)));
    return;
  }
  // Temp file then rename: writing in place truncates at open, and this app
  // writes on every move.
  Storage.writeFile(kSaveTempPath, String(line));
  Storage.remove(kSavePath);
  Storage.rename(kSaveTempPath, kSavePath);
#endif
}

void ShogiActivity::refresh() {
  sh::generateLegal(game.board, legal);
  cachedOutcome = sh::outcome(game);
  cachedCheck = sh::inCheck(game.board, game.board.turn);
}

void ShogiActivity::goTo(const sh::Screen next) {
  screen = next;
  requestUpdate();
}

void ShogiActivity::clearPick() {
  pick = sh::Pick{};
  targets.count = 0;
  askPromotion = false;
  pendingTo = -1;
}

void ShogiActivity::startGame(const sh::Opponent who) {
  opponent = who;
  seat = sh::kSente;
  sh::startGame(game);
  refresh();
  clearPick();
  resultRecorded = false;
  engineThinking = false;
  canContinue = true;
  writeSave();
  goTo(sh::Screen::Board);
}

void ShogiActivity::resumeGame() {
  clearPick();
  resultRecorded = false;
  engineThinking = engineToMove();
  goTo(sh::Screen::Board);
}

bool ShogiActivity::engineToMove() const {
  return !inMatch() && opponent == sh::Opponent::Computer && game.board.turn != seat &&
         cachedOutcome == sh::Outcome::Running;
}

bool ShogiActivity::myMove() const {
  if (cachedOutcome != sh::Outcome::Running || engineThinking) return false;
  if (inMatch()) return linkYourTurn();
  if (opponent == sh::Opponent::Friend) return true;
  return game.board.turn == seat;
}

const char* ShogiActivity::statusText() const {
  if (engineThinking || engineToMove()) return "THINKING";
  if (inMatch() && !linkYourTurn()) return "THEIR MOVE";
  if (opponent == sh::Opponent::Friend && !inMatch()) {
    if (game.board.turn == sh::kSente) return cachedCheck ? "SENTE: CHECK" : "SENTE TO MOVE";
    return cachedCheck ? "GOTE: CHECK" : "GOTE TO MOVE";
  }
  return cachedCheck ? "CHECK: YOUR MOVE" : "YOUR MOVE";
}

void ShogiActivity::afterMove() {
  clearPick();
  refresh();
  if (inMatch()) {
    play.play(game);
  } else {
    canContinue = cachedOutcome == sh::Outcome::Running;
    writeSave();
    // Deferred one pass, so the repaint showing this move and THINKING is
    // requested before the search starts.
    if (engineToMove()) engineThinking = true;
  }
  requestUpdate();
}

void ShogiActivity::commitMove(const int to, const bool promote) {
  sh::Move move{};
  if (!sh::moveFor(legal, pick, to, promote, move)) {
    clearPick();
    requestUpdate();
    return;
  }
  // The rules built this move; nothing here constructs one.
  if (sh::play(game, move)) afterMove();
}

void ShogiActivity::handleSquare(const int square) {
  if (!myMove()) return;
  if (askPromotion) {
    // A tap on the board instead of an answer puts the piece back.
    clearPick();
    requestUpdate();
    return;
  }
  if (pick.any()) {
    sh::Promotion promotion = sh::Promotion::None;
    if (sh::promotionAt(targets, square, promotion)) {
      if (promotion == sh::Promotion::Optional) {
        askPromotion = true;
        pendingTo = square;
        requestUpdate();
        return;
      }
      commitMove(square, promotion == sh::Promotion::Forced);
      return;
    }
  }
  if (sh::canPickSquare(legal, square) && !(pick.any() && !pick.fromHand() && pick.from == square)) {
    pick = sh::Pick{};
    pick.from = static_cast<uint8_t>(square);
    sh::targetsFor(game.board, legal, pick, targets);
    requestUpdate();
    return;
  }
  // Tapping something that cannot move with nothing in hand changes nothing,
  // so it must not repaint: an identical refresh on e-ink looks like a bug.
  if (pick.any()) {
    clearPick();
    requestUpdate();
  }
}

void ShogiActivity::handleHand(const bool bottom, const int slot) {
  if (!myMove() || slot < 0 || slot >= sh::kHandOrderCount) return;
  const uint8_t owner = bottom ? seat : sh::other(seat);
  if (owner != game.board.turn) return;
  // Only a friend game hands the device across; otherwise the top strip is
  // the opponent's and never yours to touch.
  if (!bottom && (inMatch() || opponent != sh::Opponent::Friend)) return;
  const uint8_t type = sh::kHandOrder[slot];
  if (game.board.hand[owner][type] == 0 || !sh::canPickHand(legal, type)) return;
  if (pick.fromHand() && pick.drop == type) {
    clearPick();
  } else {
    clearPick();
    pick.from = sh::kDropFrom;
    pick.drop = type;
    sh::targetsFor(game.board, legal, pick, targets);
  }
  requestUpdate();
}

void ShogiActivity::playEngineMove() {
  engineThinking = false;
  // The engine copies the board per node and never writes `game`, so the
  // render task can keep drawing the real position while it thinks.
  const sh::SearchResult result = sh::search(game.board, kEngineDepth, searchBuffers, kEngineNodes);
  LOG_DBG("SHOGI", "engine: %u nodes, depth %d, score %d%s", static_cast<unsigned>(result.nodes), result.depthCompleted,
          result.score, result.budgetExhausted ? " (budget hit)" : "");
  if (!result.hasMove) {
    refresh();
    requestUpdate();
    return;
  }
  if (sh::play(game, result.best)) afterMove();
}

void ShogiActivity::recordResult() {
  if (resultRecorded) return;
  resultRecorded = true;
  canContinue = false;
  // A friend game has no "you": both players are on this device.
  if (opponent == sh::Opponent::Friend && !inMatch()) {
    writeSave();
    return;
  }
  if (cachedOutcome == sh::Outcome::Draw) {
    ++draws;
  } else {
    const bool won = (cachedOutcome == sh::Outcome::SenteWins) == (seat == sh::kSente);
    if (won) {
      ++wins;
    } else {
      ++losses;
    }
  }
  // The record is this device's whatever the game was; the in-progress game
  // is not written during a match, so write the record by itself afterwards.
  if (!inMatch()) writeSave();
}

const char* ShogiActivity::linkHeadline() const {
  if (linkPhase() == linkplay::PlayBase::Phase::Searching) return "LOOKING FOR A PLAYER";
  if (cachedOutcome == sh::Outcome::Running) return "SHOGI";
  if (cachedOutcome == sh::Outcome::Draw) return "A DRAW";
  const bool won = (cachedOutcome == sh::Outcome::SenteWins) == (seat == sh::kSente);
  return won ? "YOU WIN" : "THEY WIN";
}

void ShogiActivity::onMatchStart(const bool goesFirst) {
  // Sente moves first, so whoever goes first IS Sente. Both sides set up the
  // fixed opening position: the follower must not start from a zeroed Game,
  // which has no kings and reads as a finished game.
  seat = goesFirst ? sh::kSente : sh::kGote;
  sh::startGame(game);
  refresh();
  clearPick();
  resultRecorded = false;
  engineThinking = false;
  goTo(sh::Screen::Board);
}

bool ShogiActivity::takeOpponentState() {
  const bool took = play.takeOpponent(game);
  if (took) {
    // Their move invalidates anything in hand: the piece may have been taken.
    clearPick();
    refresh();
  }
  return took;
}

void ShogiActivity::onRematch() { onMatchStart(play.goesFirst()); }

void ShogiActivity::onLinkEnded() {
  seat = sh::kSente;
  clearPick();
  // Back to whatever solo game was saved before the match. The tally is kept
  // across the reload: a match result was counted in memory but could not be
  // written while the match was live (writeSave refuses then), and leaveLink()
  // has cleared the match by the time this runs, so it can be written now.
  const int keptWins = wins;
  const int keptLosses = losses;
  const int keptDraws = draws;
  sh::startGame(game);
  refresh();
  loadSave();
  wins = keptWins;
  losses = keptLosses;
  draws = keptDraws;
  writeSave();
  goTo(sh::Screen::Menu);
}

void ShogiActivity::onMatchEnded() {
  recordResult();
  goTo(sh::Screen::Result);
}

// The screen, the seat (which flips the pixel-to-square map), whether the board
// is live, and whether a promotion question has turned the capsule into two
// buttons. The pick is deliberately absent: lifting then placing is every
// move, and hashing it would eat the second tap of each.
uint32_t ShogiActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  meaning = paintclock::mixMeaning(meaning, seat);
  meaning = paintclock::mixMeaning(meaning, myMove() ? 1u : 0u);
  return paintclock::mixMeaning(meaning, askPromotion ? 1u : 0u);
}

void ShogiActivity::gameLoop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (inMatch()) {
      leaveLink();
      return;
    }
    if (sh::leavesApp(screen)) {
      writeSave();
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (screen == sh::Screen::Board) {
      engineThinking = false;
      clearPick();
      writeSave();
    }
    goTo(sh::back(screen));
    return;
  }

  if (screen == sh::Screen::HowTo) {
    const bool forward = mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool backward = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (forward || backward) {
      const int pages = shogiui::howToPages();
      howToPage = (howToPage + (forward ? 1 : pages - 1)) % pages;
      requestUpdate();
      return;
    }
  }

  if (screen == sh::Screen::Board) {
    if (inMatch()) {
      if (!linkYourTurn()) {
        if (takeOpponentState()) requestUpdate();
        return;
      }
    } else {
      if (cachedOutcome != sh::Outcome::Running) {
        recordResult();
        goTo(sh::Screen::Result);
        return;
      }
      if (engineThinking) {
        playEngineMove();
        return;
      }
      if (engineToMove()) {
        engineThinking = true;
        requestUpdate();
        return;
      }
    }
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  if (screen == sh::Screen::Board) {
    // The promotion answers are registered controls; they are tried first, so
    // a tap on PROMOTE is never read as a tap on the board behind it.
    if (askPromotion) {
      const fui::ActionEvent event = interactions.route(input);
      if (event.action == shogiui::ActionPromote || event.action == shogiui::ActionKeep) {
        const int to = pendingTo;
        askPromotion = false;
        commitMove(to, event.action == shogiui::ActionPromote);
        return;
      }
    }
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int file = 0;
    int rank = 0;
    if (shogiui::squareAt(device, tapX, tapY, seat, file, rank)) {
      // Eighty-one squares do not fit the interaction table, so this tap never
      // reaches route(). See Activity::surfaceMeaning().
      if (!surfaceRevealed()) return;
      handleSquare(sh::indexOf(file, rank));
      return;
    }
    bool bottom = false;
    int slot = 0;
    if (shogiui::handSlotAt(device, tapX, tapY, bottom, slot)) {
      if (!surfaceRevealed()) return;
      handleHand(bottom, slot);
      return;
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case shogiui::ActionMenuRow:
      switch (static_cast<shogiui::MenuRow>(event.value)) {
        case shogiui::MenuRow::Continue:
          resumeGame();
          return;
        case shogiui::MenuRow::Computer:
          startGame(sh::Opponent::Computer);
          return;
        case shogiui::MenuRow::Friend:
          startGame(sh::Opponent::Friend);
          return;
        case shogiui::MenuRow::Nearby:
          enterLink(linkplay::GameId::Shogi);
          return;
        case shogiui::MenuRow::HowTo:
          howToPage = 0;
          goTo(sh::Screen::HowTo);
          return;
        case shogiui::MenuRow::Count:
          return;
      }
      return;

    case shogiui::ActionHowToNext:
      if (howToPage + 1 < shogiui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(sh::Screen::Menu);
      return;

    case shogiui::ActionAgain:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startGame(opponent);
      return;

    case shogiui::ActionDone:
      if (inMatch()) {
        leaveLink();
        return;
      }
      goTo(sh::Screen::Menu);
      return;

    default:
      return;
  }
}

void ShogiActivity::gameRender() {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case sh::Screen::Menu: {
      shogiui::MenuModel model;
      model.canContinue = canContinue;
      model.wins = wins;
      model.losses = losses;
      model.draws = draws;
      shogiui::buildMenu(surface, model);
      break;
    }
    case sh::Screen::HowTo: {
      shogiui::HowToModel model;
      model.page = howToPage;
      shogiui::buildHowTo(surface, model);
      break;
    }
    case sh::Screen::Board: {
      shogiui::BoardModel model;
      model.board = game.board;
      // A friend game is drawn from Sente's side throughout: the pieces point
      // at their owners' opponents, so either player can read it.
      model.seat = seat;
      model.pick = pick;
      model.targets = targets;
      model.lastTo = game.lastTo;
      model.status = statusText();
      model.askPromotion = askPromotion;
      model.opponentName = inMatch() ? opponentName() : nullptr;
      shogiui::buildBoard(surface, model);
      break;
    }
    case sh::Screen::Result: {
      shogiui::ResultModel model;
      const bool friendGame = opponent == sh::Opponent::Friend && !inMatch();
      if (cachedOutcome == sh::Outcome::Draw) {
        model.title = "A DRAW";
      } else if (friendGame) {
        model.title = cachedOutcome == sh::Outcome::SenteWins ? "SENTE WINS" : "GOTE WINS";
      } else {
        const bool won = (cachedOutcome == sh::Outcome::SenteWins) == (seat == sh::kSente);
        model.title = won ? "YOU WIN" : "THEY WIN";
      }
      switch (sh::endingOf(game, cachedOutcome)) {
        case sh::Ending::Checkmate:
          model.reason = "CHECKMATE";
          break;
        case sh::Ending::NoMoves:
          model.reason = "NO LEGAL MOVES";
          break;
        case sh::Ending::Repetition:
          model.reason = "SENNICHITE: FOUR REPEATS";
          break;
        case sh::Ending::MoveLimit:
          model.reason = "MOVE LIMIT";
          break;
        case sh::Ending::None:
          break;
      }
      model.plies = game.plies;
      shogiui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Shogi");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
