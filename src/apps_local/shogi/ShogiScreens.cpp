#include "ShogiScreens.h"

#include <cstdio>

#include "../link/LinkScreens.h"
#include "../ui/ToyboxFormat.h"

namespace shogiui {

namespace {

namespace sh = shogi;

// 48px squares: nine of them are 432px, which leaves the 24px side margins the
// rest of the fork uses and room for a hand strip above and below.
constexpr int16_t kSquare = 48;
constexpr int16_t kBoardSide = kSquare * sh::kFiles;
constexpr int16_t kHandHeight = 48;
constexpr int16_t kHandSlotWidth = kBoardSide / sh::kHandOrderCount;

int16_t boardLeft(const fui::DeviceContext& device) { return static_cast<int16_t>((device.width - kBoardSide) / 2); }

int16_t topHandTop() { return static_cast<int16_t>(toybox::kChromeHeight + toybox::kGutter); }

int16_t boardTop() { return static_cast<int16_t>(topHandTop() + kHandHeight + toybox::kGutter + toybox::kBoardFrame); }

int16_t bottomHandTop() {
  return static_cast<int16_t>(boardTop() + kBoardSide + toybox::kBoardFrame + toybox::kGutter);
}

int16_t capsuleTop() { return static_cast<int16_t>(bottomHandTop() + kHandHeight + toybox::kGutter * 2); }

// The header band with the offset rule under it, as checkers wears it.
void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  // rightLabel is drawn with subtitleText, whose theme default is black on the
  // black band. See the checkers/jaipur toyboxChrome.
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

// A filled pentagon -- the shape of a shogi piece -- pointing up or down. Built
// from solid triangles, because that is all the target's polygon path draws.
void pentagonFill(toybox::Screen& screen, const fui::Rect& box, const bool up, const fui::Color colour) {
  const fui::Paint paint = fui::Paint::solid(colour);
  const int16_t inset = static_cast<int16_t>(box.width / 7);
  const int16_t shoulder = static_cast<int16_t>(box.height * 3 / 10);
  const int16_t cx = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t top = box.y;
  const int16_t bottom = static_cast<int16_t>(box.y + box.height);
  const int16_t left = box.x;
  const int16_t right = static_cast<int16_t>(box.x + box.width);
  const int16_t tip = up ? top : bottom;
  const int16_t shoulderY = up ? static_cast<int16_t>(top + shoulder) : static_cast<int16_t>(bottom - shoulder);
  const int16_t baseY = up ? bottom : top;
  const fui::Point apex{cx, tip};
  const fui::Point sL{static_cast<int16_t>(left + inset), shoulderY};
  const fui::Point sR{static_cast<int16_t>(right - inset), shoulderY};
  const fui::Point bL{left, baseY};
  const fui::Point bR{right, baseY};
  screen.target().triangle(apex, sL, sR, paint);
  screen.target().triangle(sL, sR, bR, paint);
  screen.target().triangle(sL, bR, bL, paint);
}

// One piece in `box`. The owner reads twice over: by which way it points (at
// the opponent, as on a real board) and by its colour (the bottom player's
// pieces are paper, the top player's are ink).
void drawPiece(toybox::Screen& screen, const fui::Rect& box, const uint8_t type, const bool pointsUp, const bool inked,
               const toybox::CutMetrics& cut, const fui::FontId font) {
  pentagonFill(screen, box, pointsUp, fui::Color::Black);
  if (!inked) {
    const fui::Rect inner = fui::makeRect(static_cast<int16_t>(box.x + 3), static_cast<int16_t>(box.y + 3),
                                          static_cast<int16_t>(box.width - 6), static_cast<int16_t>(box.height - 6));
    pentagonFill(screen, inner, pointsUp, fui::Color::White);
  }
  // The label sits in the wide part of the pentagon, away from the point.
  const int16_t labelTop = pointsUp ? static_cast<int16_t>(box.y + box.height / 4) : box.y;
  const fui::Rect labelBox = fui::makeRect(box.x, labelTop, box.width, static_cast<int16_t>(box.height * 3 / 4));
  fui::TextStyle style;
  style.font = font;
  style.align = fui::TextAlign::Center;
  style.color = inked ? fui::Color::White : fui::Color::Black;
  screen.target().text(toybox::inkCentred(labelBox, cut), pieceLabel(type), style);
}

fui::Rect pieceBox(const fui::Rect& square) {
  return fui::makeRect(static_cast<int16_t>(square.x + 4), static_cast<int16_t>(square.y + 3),
                       static_cast<int16_t>(square.width - 8), static_cast<int16_t>(square.height - 6));
}

void drawHand(toybox::Screen& screen, const fui::DeviceContext& device, const sh::Board& board, const uint8_t owner,
              const bool bottom, const uint8_t pickedDrop) {
  const bool inked = !bottom;
  for (int slot = 0; slot < sh::kHandOrderCount; ++slot) {
    const uint8_t type = sh::kHandOrder[slot];
    const int count = board.hand[owner][type];
    if (count == 0) continue;
    const fui::Rect cell = handSlotRect(device, bottom, slot);
    const fui::Rect box = fui::makeRect(static_cast<int16_t>(cell.x + 4), static_cast<int16_t>(cell.y + 4), 34,
                                        static_cast<int16_t>(cell.height - 8));
    drawPiece(screen, box, type, bottom, inked, toybox::kButtonCut, toybox::kSmallFont);
    if (count > 1) {
      char n[12];
      std::snprintf(n, sizeof(n), "%d", count > 99 ? 99 : count);
      fui::TextStyle style;
      style.font = toybox::kSmallFont;
      style.align = fui::TextAlign::Left;
      const fui::Rect countBox = fui::makeRect(static_cast<int16_t>(box.x + box.width + 3), cell.y,
                                               static_cast<int16_t>(cell.width - box.width - 7), cell.height);
      screen.target().text(toybox::inkCentred(countBox, toybox::kTileCut), n, style);
    }
    if (bottom && pickedDrop == type) {
      screen.target().stroke(cell, fui::Paint::solid(fui::Color::Black), toybox::kFrame);
    }
  }
}

}  // namespace

const char* pieceLabel(const uint8_t type) {
  switch (type) {
    case sh::Pawn:
      return "P";
    case sh::Lance:
      return "L";
    case sh::Knight:
      return "N";
    case sh::Silver:
      return "S";
    case sh::Gold:
      return "G";
    case sh::Bishop:
      return "B";
    case sh::Rook:
      return "R";
    case sh::King:
      return "K";
    case sh::Tokin:
      return "+P";
    case sh::PLance:
      return "+L";
    case sh::PKnight:
      return "+N";
    case sh::PSilver:
      return "+S";
    case sh::Horse:
      return "+B";
    case sh::Dragon:
      return "+R";
    default:
      return "";
  }
}

fui::Rect squareRect(const fui::DeviceContext& device, const int file, const int rank, const uint8_t seat) {
  const int drawFile = seat == sh::kSente ? file : sh::kFiles - 1 - file;
  const int drawRank = seat == sh::kSente ? rank : sh::kRanks - 1 - rank;
  return fui::makeRect(static_cast<int16_t>(boardLeft(device) + drawFile * kSquare),
                       static_cast<int16_t>(boardTop() + drawRank * kSquare), kSquare, kSquare);
}

bool squareAt(const fui::DeviceContext& device, const int x, const int y, const uint8_t seat, int& file, int& rank) {
  const int dx = x - boardLeft(device);
  const int dy = y - boardTop();
  if (dx < 0 || dy < 0 || dx >= kBoardSide || dy >= kBoardSide) return false;
  const int drawFile = dx / kSquare;
  const int drawRank = dy / kSquare;
  file = seat == sh::kSente ? drawFile : sh::kFiles - 1 - drawFile;
  rank = seat == sh::kSente ? drawRank : sh::kRanks - 1 - drawRank;
  return true;
}

fui::Rect handSlotRect(const fui::DeviceContext& device, const bool bottom, const int slot) {
  return fui::makeRect(static_cast<int16_t>(boardLeft(device) + slot * kHandSlotWidth),
                       bottom ? bottomHandTop() : topHandTop(), kHandSlotWidth, kHandHeight);
}

bool handSlotAt(const fui::DeviceContext& device, const int x, const int y, bool& bottom, int& slot) {
  const int dx = x - boardLeft(device);
  if (dx < 0 || dx >= kHandSlotWidth * sh::kHandOrderCount) return false;
  if (y >= topHandTop() && y < topHandTop() + kHandHeight) {
    bottom = false;
  } else if (y >= bottomHandTop() && y < bottomHandTop() + kHandHeight) {
    bottom = true;
  } else {
    return false;
  }
  slot = dx / kHandSlotWidth;
  return true;
}

fui::Rect promoteRect(const fui::DeviceContext& device) {
  const int16_t width = static_cast<int16_t>((kBoardSide - toybox::kGutter) / 2);
  return fui::makeRect(boardLeft(device), capsuleTop(), width, toybox::kPillHeight);
}

fui::Rect keepRect(const fui::DeviceContext& device) {
  const fui::Rect left = promoteRect(device);
  return fui::makeRect(static_cast<int16_t>(left.x + left.width + toybox::kGutter), left.y, left.width, left.height);
}

int howToPages() { return 4; }

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "SHOGI");

  char record[48];
  std::snprintf(record, sizeof(record), "%d PLAYED   %d WON", model.wins + model.losses + model.draws, model.wins);
  const fui::Rect line = screen.takeTop(26);
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Left;
  screen.target().text(line, record, small);
  screen.target().fill(fui::makeRect(line.x, static_cast<int16_t>(line.bottom() + 6), line.width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));

  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  int count = 0;
  int nearbyIndex = -1;
  const auto add = [&](const MenuRow row, const char* label) {
    rows[count].label = label;
    rows[count].actionValue = static_cast<int16_t>(row);
    ++count;
  };
  if (model.canContinue) add(MenuRow::Continue, "CONTINUE");
  add(MenuRow::Computer, "PLAY THE COMPUTER");
  add(MenuRow::Friend, "TWO PLAYERS");
  nearbyIndex = count;
  add(MenuRow::Nearby, "PLAY NEARBY");
  rows[nearbyIndex].subtitle = model.nearbyName;
  add(MenuRow::HowTo, "HOW TO PLAY");

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(count);
  list.selectedIndex = 0;
  list.action = ActionMenuRow;
  const int16_t listHeight =
      static_cast<int16_t>(count * toybox::kRowHeight + (count - 1) * toybox::kGutter / 2 + toybox::kGutter);
  const fui::Rect content = screen.contentRect();
  const fui::Rect listBand =
      fui::makeRect(content.x, static_cast<int16_t>(content.bottom() - listHeight), content.width, listHeight);
  screen.list(list, listHeight, fui::LayoutAnchor::Bottom);
  toybox::iconAtRowRight(screen, listBand, nearbyIndex, 0, linkui::nearbyMark(), nearbyIndex == 0);

  // The ornament: the opening position's two kings and their golds, drawn with
  // the board's own pieces, so the front door is made of the game's material.
  const int16_t areaTop = static_cast<int16_t>(line.bottom() + 6 + toybox::kRule);
  const int16_t room = static_cast<int16_t>(listBand.y - areaTop);
  if (room < 140) return;
  constexpr int16_t kW = 58;
  constexpr int16_t kH = 64;
  const int16_t rowW = static_cast<int16_t>(kW * 3 + toybox::kGutter * 2);
  const fui::DeviceContext device = screen.device();
  const int16_t left = static_cast<int16_t>((device.width - rowW) / 2);
  const int16_t top = static_cast<int16_t>(areaTop + (room - kH * 2 - toybox::kGutter) / 2);
  const uint8_t ornament[3] = {sh::Gold, sh::King, sh::Gold};
  for (int i = 0; i < 3; ++i) {
    const int16_t x = static_cast<int16_t>(left + i * (kW + toybox::kGutter));
    drawPiece(screen, fui::makeRect(x, top, kW, kH), ornament[i], false, true, toybox::kUiCut, toybox::kUiFont);
    drawPiece(screen, fui::makeRect(x, static_cast<int16_t>(top + kH + toybox::kGutter), kW, kH), ornament[i], true,
              false, toybox::kUiCut, toybox::kUiFont);
  }
}

namespace {

void doBuildHowTo(toybox::Screen& screen, const HowToModel& model) {
  const int pages = howToPages();
  const int page = model.page < 0 ? 0 : (model.page >= pages ? pages - 1 : model.page);

  char progress[toybox::kOfCounterChars];
  std::snprintf(progress, sizeof(progress), "%d OF %d", page + 1, pages);
  toyboxChrome(screen, "HOW TO PLAY", progress);
  const fui::Rect body = screen.body();
  screen.frame().hit(body, ActionHowToNext, 0);

  constexpr int16_t kDot = 14;
  constexpr int16_t kDotGap = 10;
  const int16_t dotRow = static_cast<int16_t>(pages * kDot + (pages - 1) * kDotGap);
  const int16_t dotX = static_cast<int16_t>(body.x + (body.width - dotRow) / 2);
  const int16_t dotY = static_cast<int16_t>(body.bottom() - kDot);
  for (int i = 0; i < pages; ++i) {
    const fui::Rect dotAt = fui::makeRect(static_cast<int16_t>(dotX + i * (kDot + kDotGap)), dotY, kDot, kDot);
    if (i == page) {
      screen.target().fill(dotAt, fui::Paint::solid(fui::Color::Black), 7);
    } else {
      screen.target().stroke(dotAt, fui::Paint::dither(fui::Color::DarkGray), toybox::kHairline, 7);
    }
  }
  fui::TextStyle tapLine;
  tapLine.font = toybox::kTileFont;
  tapLine.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(body.x, static_cast<int16_t>(dotY - 34), body.width, 22),
                       page + 1 == pages ? "TAP TO FINISH" : "TAP TO CONTINUE", tapLine);

  static const char* const kTitles[] = {"THE PIECES", "PROMOTION", "DROPS", "WINNING"};
  fui::TextStyle title;
  title.font = toybox::kDisplayFont;
  title.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(fui::makeRect(body.x, body.y, body.width, 46), toybox::kDisplayCut),
                       kTitles[page], title);

  static const char* const kLines[] = {
      "K KING ONE STEP ANY WAY. R ROOK AND B BISHOP SLIDE. G GOLD AND S SILVER STEP. "
      "N KNIGHT JUMPS TWO FORWARD ONE ACROSS. L LANCE SLIDES FORWARD. P PAWN STEPS FORWARD.",
      "ENTER, LEAVE OR MOVE IN THE FAR THREE ROWS AND A PIECE MAY PROMOTE. "
      "+R AND +B GAIN A STEP; THE OTHERS MOVE AS A GOLD. YOU WILL BE ASKED.",
      "WHAT YOU TAKE CHANGES SIDES. TAP IT IN YOUR HAND, THEN AN EMPTY SQUARE. "
      "NO TWO UNPROMOTED PAWNS ON ONE FILE, AND NO MATE BY A DROPPED PAWN.",
      "CHECKMATE THE KING. HAVING NO LEGAL MOVE LOSES. "
      "THE SAME POSITION FOUR TIMES IS A DRAW.",
  };
  fui::TextStyle cap;
  cap.font = toybox::kUiFont;
  cap.align = fui::TextAlign::Center;
  cap.maxLines = 9;
  const int16_t capTop = static_cast<int16_t>(body.y + 76);
  screen.target().text(fui::makeRect(body.x, capTop, body.width, static_cast<int16_t>(dotY - 50 - capTop)),
                       kLines[page], cap);
}

}  // namespace

void buildHowTo(toybox::Screen& screen, const HowToModel& model) { doBuildHowTo(screen, model); }

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  fui::HeaderProps header;
  header.title = "SHOGI";
  header.borderEdges = fui::EdgesNone;
  toybox::headerBand(screen, header);

  const fui::DeviceContext device = screen.device();
  const uint8_t top = sh::other(model.seat);

  // The capsule, or the promotion question in its place.
  if (model.askPromotion) {
    fui::ButtonProps promote;
    promote.label = "PROMOTE";
    promote.action = ActionPromote;
    screen.button(promote, promoteRect(device));
    fui::ButtonProps keep;
    keep.label = "DON'T";
    keep.action = ActionKeep;
    screen.button(keep, keepRect(device));
  } else {
    fui::ButtonProps status;
    status.label = model.status;
    status.action = fui::NO_ACTION;
    status.borderEdges = fui::EdgesNone;
    const fui::Rect capsule = fui::makeRect(boardLeft(device), capsuleTop(), kBoardSide, toybox::kPillHeight);
    screen.button(status, model.opponentName != nullptr ? linkui::withOpponentFace(screen, capsule, model.opponentName)
                                                        : capsule);
  }

  const fui::Rect frame = fui::makeRect(static_cast<int16_t>(boardLeft(device) - toybox::kBoardFrame),
                                        static_cast<int16_t>(boardTop() - toybox::kBoardFrame),
                                        static_cast<int16_t>(kBoardSide + toybox::kBoardFrame * 2),
                                        static_cast<int16_t>(kBoardSide + toybox::kBoardFrame * 2));
  screen.target().stroke(frame, fui::Paint::solid(fui::Color::Black), toybox::kBoardFrame);

  // Grid lines rather than checkered squares: a shogi board is a plain grid.
  for (int i = 1; i < sh::kFiles; ++i) {
    const int16_t x = static_cast<int16_t>(boardLeft(device) + i * kSquare);
    screen.target().fill(fui::makeRect(x, boardTop(), toybox::kHairline, kBoardSide),
                         fui::Paint::solid(fui::Color::Black));
    const int16_t y = static_cast<int16_t>(boardTop() + i * kSquare);
    screen.target().fill(fui::makeRect(boardLeft(device), y, kBoardSide, toybox::kHairline),
                         fui::Paint::solid(fui::Color::Black));
  }
  // The promotion zones, as faint shading on the three far ranks of each side,
  // so where promotion starts is something the board shows.
  for (int rank = 0; rank < sh::kRanks; ++rank) {
    if (rank > 2 && rank < 6) continue;
    for (int file = 0; file < sh::kFiles; ++file) {
      const fui::Rect box = squareRect(device, file, rank, model.seat);
      if (model.board.cell[sh::indexOf(file, rank)] != 0) continue;
      screen.target().fill(fui::makeRect(static_cast<int16_t>(box.x + kSquare / 2 - 1),
                                         static_cast<int16_t>(box.y + kSquare / 2 - 1), 3, 3),
                           fui::Paint::dither(fui::Color::DarkGray));
    }
  }

  for (int rank = 0; rank < sh::kRanks; ++rank) {
    for (int file = 0; file < sh::kFiles; ++file) {
      const uint8_t cell = model.board.cell[sh::indexOf(file, rank)];
      if (cell == 0) continue;
      const fui::Rect box = squareRect(device, file, rank, model.seat);
      const bool mineAtBottom = sh::ownerOf(cell) == model.seat;
      drawPiece(screen, pieceBox(box), sh::typeOf(cell), mineAtBottom, !mineAtBottom, toybox::kUiCut, toybox::kUiFont);
    }
  }

  drawHand(screen, device, model.board, top, false, static_cast<uint8_t>(sh::None));
  drawHand(screen, device, model.board, model.seat, true,
           model.pick.fromHand() ? model.pick.drop : static_cast<uint8_t>(sh::None));

  // The last move's landing square, so the reply is findable after the panel
  // refreshes. Drawn after the pieces: a decoration outside its own cell needs
  // its own pass.
  if (model.lastTo < sh::kCells) {
    const fui::Rect box = squareRect(device, sh::fileOf(model.lastTo), sh::rankOf(model.lastTo), model.seat);
    toybox::bracket(screen, box, 10, 3);
  }

  if (model.pick.any() && !model.pick.fromHand()) {
    const fui::Rect box = squareRect(device, sh::fileOf(model.pick.from), sh::rankOf(model.pick.from), model.seat);
    screen.target().stroke(
        fui::makeRect(static_cast<int16_t>(box.x - toybox::kFrame), static_cast<int16_t>(box.y - toybox::kFrame),
                      static_cast<int16_t>(box.width + toybox::kFrame * 2),
                      static_cast<int16_t>(box.height + toybox::kFrame * 2)),
        fui::Paint::solid(fui::Color::Black), toybox::kFrame);
  }

  for (int i = 0; i < model.targets.count; ++i) {
    const int square = model.targets.to[i];
    const fui::Rect box = squareRect(device, sh::fileOf(square), sh::rankOf(square), model.seat);
    if (model.targets.capture[i]) {
      toybox::bracket(screen, box, 16, 4);
    } else {
      // A ring for a landing that may promote, a dot for one that may not.
      if (model.targets.promotion[i] != sh::Promotion::None) {
        toybox::ring(screen, static_cast<int16_t>(box.x + kSquare / 2), static_cast<int16_t>(box.y + kSquare / 2), 10,
                     3, fui::Color::Black, fui::Color::White);
      } else {
        toybox::disc(screen, static_cast<int16_t>(box.x + kSquare / 2), static_cast<int16_t>(box.y + kSquare / 2), 8,
                     fui::Color::Black);
      }
    }
  }
}

void buildResult(toybox::Screen& screen, const ResultModel& model) {
  fui::HeaderProps header;
  header.title = model.title;
  header.borderEdges = fui::EdgesNone;
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  fui::ButtonProps done;
  done.label = "DONE";
  done.action = ActionDone;
  screen.button(done, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  fui::ButtonProps again;
  again.label = "PLAY AGAIN";
  again.action = ActionAgain;
  screen.button(again, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect area = screen.body();
  if (model.reason != nullptr) {
    fui::TextStyle big;
    big.font = toybox::kDisplayFont;
    big.align = fui::TextAlign::Center;
    big.maxLines = 2;
    screen.target().text(fui::makeRect(area.x, static_cast<int16_t>(area.y + 40), area.width, 130), model.reason, big);
  }
  char line[32];
  std::snprintf(line, sizeof(line), "%d MOVES", model.plies);
  fui::TextStyle body;
  body.font = toybox::kUiFont;
  body.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(area.x, static_cast<int16_t>(area.y + 190), area.width, 42), line, body);
}

}  // namespace shogiui
