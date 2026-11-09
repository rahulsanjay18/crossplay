#pragma once

// Shogi on the device. The thin layer: renderer, input, storage, the shelf,
// the engine's deferral and the link.
//
// Three ways to play, one board: against the built-in engine, two people
// passing the device (TWO PLAYERS), and two devices (PLAY NEARBY, via
// LinkActivity). The board never knows which; the activity decides whose taps
// count and who answers.

#include <memory>

#include "../link/LinkActivity.h"
#include "../ui/ToyboxScreen.h"
#include "ShogiCore.h"
#include "ShogiEngine.h"
#include "ShogiFlow.h"

class ShogiActivity final : public linkplay::LinkActivity {
 public:
  ShogiActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : linkplay::LinkActivity("Shogi", renderer, mappedInput) {
    shogi::setRulesScratch(&rulesScratch);
  }
  ~ShogiActivity() override { shogi::clearRulesScratch(&rulesScratch); }

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

 protected:
  linkplay::PlayBase& linkState() override { return play; }
  const linkplay::PlayBase& linkState() const override { return play; }
  const char* linkGameTitle() const override { return "SHOGI"; }
  const char* linkHeadline() const override;
  void onMatchStart(bool goesFirst) override;
  bool takeOpponentState() override;
  void onRematch() override;
  void onLinkEnded() override;
  // Read from the cache, never from the rules: this can be asked from a
  // render, and the rules use main-thread scratch.
  bool matchGameOver() const override { return cachedOutcome != shogi::Outcome::Running; }
  void onMatchEnded() override;
  void gameLoop() override;
  void gameRender() override;

 private:
  void startGame(shogi::Opponent who);
  void resumeGame();
  void goTo(shogi::Screen next);
  void clearPick();
  // Recomputes everything a paint needs from the rules, on the main loop, so
  // the render task never runs move generation.
  void refresh();
  void afterMove();
  void commitMove(int to, bool promote);
  void handleSquare(int square);
  void handleHand(bool bottom, int slot);
  void playEngineMove();
  bool myMove() const;
  bool engineToMove() const;
  const char* statusText() const;
  void recordResult();
  void loadSave();
  void writeSave();

  uint32_t surfaceMeaning() const override;

  shogi::Screen screen = shogi::Screen::Menu;
  int howToPage = 0;
  shogi::Opponent opponent = shogi::Opponent::Computer;
  // The side drawn at the bottom. Sente against the computer and with a friend;
  // the coin toss decides it in a link match.
  uint8_t seat = shogi::kSente;
  shogi::Game game{};

  // Cached by refresh().
  shogi::MoveList legal;
  shogi::Outcome cachedOutcome = shogi::Outcome::Running;
  bool cachedCheck = false;

  shogi::Pick pick{};
  shogi::Targets targets{};
  bool askPromotion = false;
  int pendingTo = -1;

  bool engineThinking = false;
  shogi::SearchBuffers searchBuffers;
  shogi::RulesScratch rulesScratch;

  int wins = 0;
  int losses = 0;
  int draws = 0;
  bool resultRecorded = false;
  bool canContinue = false;

  linkplay::Play<shogi::Game> play;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
