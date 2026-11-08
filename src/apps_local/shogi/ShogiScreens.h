#pragma once

// Shogi on screen. Freestanding builders over plain models: no renderer, no
// Activity, no storage, so host-tests/ui can build every screen against a fake
// target and assert on what it drew and what it made tappable.

#include "../ui/ToyboxScreen.h"
#include "ShogiCore.h"
#include "ShogiFlow.h"

namespace shogiui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionHowToNext = 2,
  ActionAgain = 3,
  ActionDone = 4,
  ActionPromote = 5,
  ActionKeep = 6,
};

// Row identities. The menu shows CONTINUE only when there is a game to resume,
// so a row's action value is its identity rather than its position.
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
  shogi::Board board{};
  // The side drawn at the bottom. Its pieces point up the screen and its hand
  // is the strip under the board.
  uint8_t seat = shogi::kSente;
  shogi::Pick pick{};
  shogi::Targets targets{};
  uint8_t lastTo = shogi::kNoSquare;
  // The capsule's words, chosen by the activity because only it knows whether
  // the engine is thinking or a link match is waiting.
  const char* status = "YOUR MOVE";
  // A landing that may or may not promote has been tapped; the capsule becomes
  // the question.
  bool askPromotion = false;
  const char* opponentName = nullptr;
};

struct ResultModel {
  const char* title = "A DRAW";
  const char* reason = nullptr;
  int plies = 0;
};

// Board geometry, and its exact inverse. Eighty-one squares and fourteen hand
// slots against a twenty-four slot interaction buffer, so all of them are
// hit-tested arithmetically from the same functions that place the pixels.
// Both take the seat: the board is drawn from the bottom player's side.
fui::Rect squareRect(const fui::DeviceContext& device, int file, int rank, uint8_t seat);
bool squareAt(const fui::DeviceContext& device, int x, int y, uint8_t seat, int& file, int& rank);

// The hand strips. `bottom` is the strip under the board (the seat's own hand);
// `slot` indexes shogi::kHandOrder.
fui::Rect handSlotRect(const fui::DeviceContext& device, bool bottom, int slot);
bool handSlotAt(const fui::DeviceContext& device, int x, int y, bool& bottom, int& slot);

// The two promotion answers, side by side where the status capsule sits.
fui::Rect promoteRect(const fui::DeviceContext& device);
fui::Rect keepRect(const fui::DeviceContext& device);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);
void buildResult(toybox::Screen& screen, const ResultModel& model);

int howToPages();

// The letter a piece wears: "P", "R", "+P", "+R" and so on. Western shogi
// notation, because the shipped cuts carry ASCII only and a kanji glyph would
// simply not draw.
const char* pieceLabel(uint8_t type);

}  // namespace shogiui
