#pragma once

// Xiangqi on screen. Freestanding builders over plain models, host-tested in
// host-tests/ui like every other screen in the fork.

#include "../ui/ToyboxScreen.h"
#include "XiangqiCore.h"
#include "XiangqiFlow.h"

namespace xiangqiui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionHowToNext = 2,
  ActionAgain = 3,
  ActionDone = 4,
};

enum class MenuRow : int { Continue = 0, Computer, Friend, Nearby, HowTo, Count };

struct MenuModel {
  bool canContinue = false;
  int wins = 0;
  int losses = 0;
  int draws = 0;
  const char* nearbyName = nullptr;
};

struct HowToModel {
  int page = 0;
};

struct BoardModel {
  xiangqi::Board board{};
  // The side drawn at the bottom of the screen.
  uint8_t seat = xiangqi::kRed;
  int picked = xiangqi::kNothingPicked;
  xiangqi::Targets targets{};
  uint8_t lastTo = xiangqi::kNoPoint;
  const char* status = "YOUR MOVE";
  const char* opponentName = nullptr;
};

struct ResultModel {
  const char* title = "A DRAW";
  const char* reason = nullptr;
  int plies = 0;
};

// The cell around a point (pieces sit on the lines' crossings; each crossing
// owns the square cell centred on it), and its exact inverse. Ninety points
// against a twenty-four slot interaction buffer, so the board is hit-tested
// from the same geometry that drew it.
fui::Rect pointRect(const fui::DeviceContext& device, int file, int rank, uint8_t seat);
bool pointAt(const fui::DeviceContext& device, int x, int y, uint8_t seat, int& file, int& rank);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);
void buildResult(toybox::Screen& screen, const ResultModel& model);

int howToPages();

// Western letters for the pieces: K general, A advisor, E elephant, H horse,
// R chariot, C cannon, P soldier. The shipped cuts are ASCII only.
const char* pieceLabel(uint8_t type);

}  // namespace xiangqiui
