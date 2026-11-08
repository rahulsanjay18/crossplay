#pragma once

// Xiangqi on the device: the thin layer over the freestanding rules, engine,
// flow and screens. Same three ways to play as Shogi -- the computer, a friend
// on the same device, or a nearby device through LinkActivity.

#include <memory>

#include "../link/LinkActivity.h"
#include "../ui/ToyboxScreen.h"
#include "XiangqiCore.h"
#include "XiangqiEngine.h"
#include "XiangqiFlow.h"

class XiangqiActivity final : public linkplay::LinkActivity {
 public:
  XiangqiActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : linkplay::LinkActivity("Xiangqi", renderer, mappedInput) {}
  ~XiangqiActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

 protected:
  linkplay::PlayBase& linkState() override { return play; }
  const linkplay::PlayBase& linkState() const override { return play; }
  const char* linkGameTitle() const override { return "XIANGQI"; }
  const char* linkHeadline() const override;
  void onMatchStart(bool goesFirst) override;
  bool takeOpponentState() override;
  void onRematch() override;
  void onLinkEnded() override;
  bool matchGameOver() const override { return cachedOutcome != xiangqi::Outcome::Running; }
  void onMatchEnded() override;
  void gameLoop() override;
  void gameRender() override;

 private:
  void startGame(xiangqi::Opponent who);
  void resumeGame();
  void goTo(xiangqi::Screen next);
  void clearPick();
  void refresh();
  void afterMove();
  void handlePoint(int point);
  void playEngineMove();
  bool myMove() const;
  bool engineToMove() const;
  bool won() const;
  const char* statusText() const;
  void recordResult();
  void loadSave();
  void writeSave();

  uint32_t surfaceMeaning() const override;

  xiangqi::Screen screen = xiangqi::Screen::Menu;
  int howToPage = 0;
  xiangqi::Opponent opponent = xiangqi::Opponent::Computer;
  uint8_t seat = xiangqi::kRed;
  xiangqi::Game game{};

  xiangqi::MoveList legal;
  xiangqi::Outcome cachedOutcome = xiangqi::Outcome::Running;
  bool cachedCheck = false;

  int picked = xiangqi::kNothingPicked;
  xiangqi::Targets targets{};

  bool engineThinking = false;
  xiangqi::SearchBuffers searchBuffers;

  int wins = 0;
  int losses = 0;
  int draws = 0;
  bool resultRecorded = false;
  bool canContinue = false;

  linkplay::Play<xiangqi::Game> play;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
