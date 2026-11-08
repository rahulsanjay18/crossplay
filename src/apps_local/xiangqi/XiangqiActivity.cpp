#include "XiangqiActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "XiangqiSave.h"
#include "XiangqiScreens.h"

namespace xq = xiangqi;

namespace {

#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kSavePath[] = "/.crosspoint/xiangqi.sav";
constexpr char kSaveTempPath[] = "/.crosspoint/xiangqi.sav.tmp";
#endif

// Xiangqi branches less than shogi (no drops), so the same kind of budget buys
// a ply more. Iterative: running out plays the deepest finished iteration.
constexpr int kEngineDepth = 5;
constexpr uint32_t kEngineNodes = 80000;

}  // namespace

std::unique_ptr<Activity> XiangqiActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<XiangqiActivity>(renderer, mappedInput);
}

void XiangqiActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = xq::Screen::Menu;
  xq::startGame(game);
  refresh();
  loadSave();
  requestUpdate();
}

void XiangqiActivity::onExit() {
  writeSave();
  LinkActivity::onExit();
}

void XiangqiActivity::loadSave() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  canContinue = false;
  if (!Storage.exists(kSavePath)) return;
  char buffer[xiangqisave::kMaxLine + 8] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer)) == 0) return;
  xiangqisave::Save save;
  if (!xiangqisave::unpack(buffer, save)) {
    LOG_ERR("XIANGQI", "Ignoring an unreadable save");
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
    canContinue = cachedOutcome == xq::Outcome::Running;
  }
#endif
}

void XiangqiActivity::writeSave() {
  if (inMatch()) return;
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  xiangqisave::Save save;
  save.wins = wins;
  save.losses = losses;
  save.draws = draws;
  save.inProgress = canContinue;
  save.opponent = opponent;
  save.seat = seat;
  save.game = game;
  char line[xiangqisave::kMaxLine];
  if (xiangqisave::pack(save, line, sizeof(line)) <= 0) {
    LOG_ERR("XIANGQI", "Save line did not fit %d bytes", static_cast<int>(sizeof(line)));
    return;
  }
  Storage.writeFile(kSaveTempPath, String(line));
  Storage.remove(kSavePath);
  Storage.rename(kSaveTempPath, kSavePath);
#endif
}

void XiangqiActivity::refresh() {
  xq::generateLegal(game.board, legal);
  cachedOutcome = xq::outcome(game);
  cachedCheck = xq::inCheck(game.board, game.board.turn);
}

void XiangqiActivity::goTo(const xq::Screen next) {
  screen = next;
  requestUpdate();
}

void XiangqiActivity::clearPick() {
  picked = xq::kNothingPicked;
  targets.count = 0;
}

void XiangqiActivity::startGame(const xq::Opponent who) {
  opponent = who;
  seat = xq::kRed;
  xq::startGame(game);
  refresh();
  clearPick();
  resultRecorded = false;
  engineThinking = false;
  canContinue = true;
  writeSave();
  goTo(xq::Screen::Board);
}

void XiangqiActivity::resumeGame() {
  clearPick();
  resultRecorded = false;
  engineThinking = engineToMove();
  goTo(xq::Screen::Board);
}

bool XiangqiActivity::engineToMove() const {
  return !inMatch() && opponent == xq::Opponent::Computer && game.board.turn != seat &&
         cachedOutcome == xq::Outcome::Running;
}

bool XiangqiActivity::myMove() const {
  if (cachedOutcome != xq::Outcome::Running || engineThinking) return false;
  if (inMatch()) return linkYourTurn();
  if (opponent == xq::Opponent::Friend) return true;
  return game.board.turn == seat;
}

bool XiangqiActivity::won() const { return (cachedOutcome == xq::Outcome::RedWins) == (seat == xq::kRed); }

const char* XiangqiActivity::statusText() const {
  if (engineThinking || engineToMove()) return "THINKING";
  if (inMatch() && !linkYourTurn()) return "THEIR MOVE";
  if (opponent == xq::Opponent::Friend && !inMatch()) {
    if (game.board.turn == xq::kRed) return cachedCheck ? "RED: CHECK" : "RED TO MOVE";
    return cachedCheck ? "BLACK: CHECK" : "BLACK TO MOVE";
  }
  return cachedCheck ? "CHECK: YOUR MOVE" : "YOUR MOVE";
}

void XiangqiActivity::afterMove() {
  clearPick();
  refresh();
  if (inMatch()) {
    play.play(game);
  } else {
    canContinue = cachedOutcome == xq::Outcome::Running;
    writeSave();
    if (engineToMove()) engineThinking = true;
  }
  requestUpdate();
}

void XiangqiActivity::handlePoint(const int point) {
  if (!myMove()) return;
  if (picked != xq::kNothingPicked) {
    xq::Move move{};
    if (xq::moveBetween(legal, picked, point, move)) {
      if (xq::play(game, move)) afterMove();
      return;
    }
  }
  if (xq::canPick(legal, point) && point != picked) {
    picked = point;
    xq::targetsFor(game.board, legal, point, targets);
    requestUpdate();
    return;
  }
  if (picked != xq::kNothingPicked) {
    clearPick();
    requestUpdate();
  }
}

void XiangqiActivity::playEngineMove() {
  engineThinking = false;
  const xq::SearchResult result = xq::search(game.board, kEngineDepth, searchBuffers, kEngineNodes);
  LOG_DBG("XIANGQI", "engine: %u nodes, depth %d, score %d%s", static_cast<unsigned>(result.nodes),
          result.depthCompleted, result.score, result.budgetExhausted ? " (budget hit)" : "");
  if (!result.hasMove) {
    refresh();
    requestUpdate();
    return;
  }
  if (xq::play(game, result.best)) afterMove();
}

void XiangqiActivity::recordResult() {
  if (resultRecorded) return;
  resultRecorded = true;
  canContinue = false;
  if (opponent == xq::Opponent::Friend && !inMatch()) {
    writeSave();
    return;
  }
  if (cachedOutcome == xq::Outcome::Draw) {
    ++draws;
  } else if (won()) {
    ++wins;
  } else {
    ++losses;
  }
  if (!inMatch()) writeSave();
}

const char* XiangqiActivity::linkHeadline() const {
  if (linkPhase() == linkplay::PlayBase::Phase::Searching) return "LOOKING FOR A PLAYER";
  if (cachedOutcome == xq::Outcome::Running) return "XIANGQI";
  if (cachedOutcome == xq::Outcome::Draw) return "A DRAW";
  return won() ? "YOU WIN" : "THEY WIN";
}

void XiangqiActivity::onMatchStart(const bool goesFirst) {
  // Red moves first, so whoever goes first IS Red. Both sides set up the fixed
  // opening: a zeroed Game has no generals and would read as finished.
  seat = goesFirst ? xq::kRed : xq::kBlack;
  xq::startGame(game);
  refresh();
  clearPick();
  resultRecorded = false;
  engineThinking = false;
  goTo(xq::Screen::Board);
}

bool XiangqiActivity::takeOpponentState() {
  const bool took = play.takeOpponent(game);
  if (took) {
    clearPick();
    refresh();
  }
  return took;
}

void XiangqiActivity::onRematch() { onMatchStart(play.goesFirst()); }

void XiangqiActivity::onLinkEnded() {
  // See ShogiActivity::onLinkEnded: the match's result is kept across the
  // reload of the solo save, and written now that writeSave() will allow it.
  const int keptWins = wins;
  const int keptLosses = losses;
  const int keptDraws = draws;
  seat = xq::kRed;
  clearPick();
  xq::startGame(game);
  refresh();
  loadSave();
  wins = keptWins;
  losses = keptLosses;
  draws = keptDraws;
  writeSave();
  goTo(xq::Screen::Menu);
}

void XiangqiActivity::onMatchEnded() {
  recordResult();
  goTo(xq::Screen::Result);
}

uint32_t XiangqiActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  meaning = paintclock::mixMeaning(meaning, seat);
  return paintclock::mixMeaning(meaning, myMove() ? 1u : 0u);
}

void XiangqiActivity::gameLoop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (inMatch()) {
      leaveLink();
      return;
    }
    if (xq::leavesApp(screen)) {
      writeSave();
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (screen == xq::Screen::Board) {
      engineThinking = false;
      clearPick();
      writeSave();
    }
    goTo(xq::back(screen));
    return;
  }

  if (screen == xq::Screen::HowTo) {
    const bool forward = mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool backward = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (forward || backward) {
      const int pages = xiangqiui::howToPages();
      howToPage = (howToPage + (forward ? 1 : pages - 1)) % pages;
      requestUpdate();
      return;
    }
  }

  if (screen == xq::Screen::Board) {
    if (inMatch()) {
      if (!linkYourTurn()) {
        if (takeOpponentState()) requestUpdate();
        return;
      }
    } else {
      if (cachedOutcome != xq::Outcome::Running) {
        recordResult();
        goTo(xq::Screen::Result);
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

  if (screen == xq::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int file = 0;
    int rank = 0;
    if (xiangqiui::pointAt(device, tapX, tapY, seat, file, rank)) {
      // Ninety points do not fit the interaction table, so this tap never
      // reaches route(). See Activity::surfaceMeaning().
      if (!surfaceRevealed()) return;
      handlePoint(xq::indexOf(file, rank));
      return;
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case xiangqiui::ActionMenuRow:
      switch (static_cast<xiangqiui::MenuRow>(event.value)) {
        case xiangqiui::MenuRow::Continue:
          resumeGame();
          return;
        case xiangqiui::MenuRow::Computer:
          startGame(xq::Opponent::Computer);
          return;
        case xiangqiui::MenuRow::Friend:
          startGame(xq::Opponent::Friend);
          return;
        case xiangqiui::MenuRow::Nearby:
          enterLink(linkplay::GameId::Xiangqi);
          return;
        case xiangqiui::MenuRow::HowTo:
          howToPage = 0;
          goTo(xq::Screen::HowTo);
          return;
        case xiangqiui::MenuRow::Count:
          return;
      }
      return;

    case xiangqiui::ActionHowToNext:
      if (howToPage + 1 < xiangqiui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(xq::Screen::Menu);
      return;

    case xiangqiui::ActionAgain:
      if (inMatch()) {
        proposeRematch();
        return;
      }
      startGame(opponent);
      return;

    case xiangqiui::ActionDone:
      if (inMatch()) {
        leaveLink();
        return;
      }
      goTo(xq::Screen::Menu);
      return;

    default:
      return;
  }
}

void XiangqiActivity::gameRender() {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case xq::Screen::Menu: {
      xiangqiui::MenuModel model;
      model.canContinue = canContinue;
      model.wins = wins;
      model.losses = losses;
      model.draws = draws;
      xiangqiui::buildMenu(surface, model);
      break;
    }
    case xq::Screen::HowTo: {
      xiangqiui::HowToModel model;
      model.page = howToPage;
      xiangqiui::buildHowTo(surface, model);
      break;
    }
    case xq::Screen::Board: {
      xiangqiui::BoardModel model;
      model.board = game.board;
      model.seat = seat;
      model.picked = picked;
      model.targets = targets;
      model.lastTo = game.lastTo;
      model.status = statusText();
      model.opponentName = inMatch() ? opponentName() : nullptr;
      xiangqiui::buildBoard(surface, model);
      break;
    }
    case xq::Screen::Result: {
      xiangqiui::ResultModel model;
      const bool friendGame = opponent == xq::Opponent::Friend && !inMatch();
      if (cachedOutcome == xq::Outcome::Draw) {
        model.title = "A DRAW";
      } else if (friendGame) {
        model.title = cachedOutcome == xq::Outcome::RedWins ? "RED WINS" : "BLACK WINS";
      } else {
        model.title = won() ? "YOU WIN" : "THEY WIN";
      }
      switch (xq::endingOf(game, cachedOutcome)) {
        case xq::Ending::Checkmate:
          model.reason = "CHECKMATE";
          break;
        case xq::Ending::Stalemate:
          model.reason = "NO LEGAL MOVES";
          break;
        case xq::Ending::Repetition:
          model.reason = "THREE REPEATS";
          break;
        case xq::Ending::NoCaptures:
          model.reason = "60 MOVES, NO CAPTURE";
          break;
        case xq::Ending::MoveLimit:
          model.reason = "MOVE LIMIT";
          break;
        case xq::Ending::None:
          break;
      }
      model.plies = game.plies;
      xiangqiui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Xiangqi");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
