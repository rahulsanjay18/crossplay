#include "XiangqiScreens.h"

#include <cstdio>

#include "../link/LinkScreens.h"
#include "../ui/ToyboxFormat.h"

namespace xiangqiui {

namespace {

namespace xq = xiangqi;

// 46px cells: nine across is 414px and ten down is 460px, which leaves room
// under the board for the material strip and the status capsule.
constexpr int16_t kCell = 46;
constexpr int16_t kBoardW = kCell * xq::kFiles;
constexpr int16_t kBoardH = kCell * xq::kRanks;
constexpr int16_t kPieceRadius = 20;
constexpr int16_t kStripRow = 34;

int16_t boardLeft(const fui::DeviceContext& device) { return static_cast<int16_t>((device.width - kBoardW) / 2); }
int16_t boardTop() { return static_cast<int16_t>(toybox::kChromeHeight + toybox::kGutter + toybox::kBoardFrame); }
int16_t stripTop() { return static_cast<int16_t>(boardTop() + kBoardH + toybox::kBoardFrame + toybox::kGutter); }
int16_t capsuleTop() { return static_cast<int16_t>(stripTop() + kStripRow * 2 + toybox::kGutter * 2); }

// Screen x/y of a crossing, by its DRAWN column and row (already flipped).
int16_t lineX(const fui::DeviceContext& device, const int column) {
  return static_cast<int16_t>(boardLeft(device) + column * kCell + kCell / 2);
}
int16_t lineY(const int row) { return static_cast<int16_t>(boardTop() + row * kCell + kCell / 2); }

void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

// A disc piece. Red is paper with an inked double ring, Black is solid ink:
// the two colours a printed set uses, translated to the two this panel has.
void drawPiece(toybox::Screen& screen, const int16_t cx, const int16_t cy, const int16_t radius, const uint8_t type,
               const bool black, const toybox::CutMetrics& cut, const fui::FontId font) {
  if (black) {
    toybox::disc(screen, cx, cy, radius, fui::Color::Black);
  } else {
    toybox::ring(screen, cx, cy, radius, 3, fui::Color::Black, fui::Color::White);
    if (radius > 12)
      toybox::ring(screen, cx, cy, static_cast<int16_t>(radius - 5), 1, fui::Color::Black, fui::Color::White);
  }
  fui::TextStyle style;
  style.font = font;
  style.align = fui::TextAlign::Center;
  style.color = black ? fui::Color::White : fui::Color::Black;
  const fui::Rect box = fui::makeRect(static_cast<int16_t>(cx - radius), static_cast<int16_t>(cy - radius),
                                      static_cast<int16_t>(radius * 2), static_cast<int16_t>(radius * 2));
  screen.target().text(toybox::inkCentred(box, cut), pieceLabel(type), style);
}

void drawLines(toybox::Screen& screen, const fui::DeviceContext& device) {
  const fui::Paint ink = fui::Paint::solid(fui::Color::Black);
  constexpr uint8_t kLine = 2;
  const int16_t x0 = lineX(device, 0);
  const int16_t x8 = lineX(device, xq::kFiles - 1);
  const int16_t y0 = lineY(0);
  const int16_t y9 = lineY(xq::kRanks - 1);
  // The river, shaded, so the halves read before a single piece is drawn.
  screen.target().fill(
      fui::makeRect(x0, lineY(4), static_cast<int16_t>(x8 - x0), static_cast<int16_t>(lineY(5) - lineY(4))),
      fui::Paint::dither(fui::Color::LightGray));
  for (int row = 0; row < xq::kRanks; ++row) {
    screen.target().line(fui::Point{x0, lineY(row)}, fui::Point{x8, lineY(row)}, kLine, ink);
  }
  for (int column = 0; column < xq::kFiles; ++column) {
    const int16_t x = lineX(device, column);
    if (column == 0 || column == xq::kFiles - 1) {
      screen.target().line(fui::Point{x, y0}, fui::Point{x, y9}, kLine, ink);
    } else {
      // The river interrupts every inner file.
      screen.target().line(fui::Point{x, y0}, fui::Point{x, lineY(4)}, kLine, ink);
      screen.target().line(fui::Point{x, lineY(5)}, fui::Point{x, y9}, kLine, ink);
    }
  }
  // Both palaces' diagonals. Symmetric, so the seat does not change them.
  const int16_t xl = lineX(device, 3);
  const int16_t xr = lineX(device, 5);
  screen.target().line(fui::Point{xl, lineY(0)}, fui::Point{xr, lineY(2)}, kLine, ink);
  screen.target().line(fui::Point{xr, lineY(0)}, fui::Point{xl, lineY(2)}, kLine, ink);
  screen.target().line(fui::Point{xl, lineY(7)}, fui::Point{xr, lineY(9)}, kLine, ink);
  screen.target().line(fui::Point{xr, lineY(7)}, fui::Point{xl, lineY(9)}, kLine, ink);
}

// One row of the material strip: the pieces this side has LOST, so a capture
// shows up where the eye already is rather than as a hole somewhere on the
// board. Empty at the start, which is honest: nothing has happened yet.
void lostRow(toybox::Screen& screen, const fui::DeviceContext& device, const xq::Board& board, const uint8_t owner,
             const int16_t top, const char* label) {
  static constexpr int kSet[8] = {0, 1, 2, 2, 2, 2, 2, 5};
  static constexpr uint8_t kOrder[6] = {xq::Chariot, xq::Cannon, xq::Horse, xq::Elephant, xq::Advisor, xq::Soldier};
  constexpr int16_t kLabelW = 72;
  constexpr int16_t kSmall = 11;
  // 23, not 25: fifteen lost pieces at 25 ran the last disc to the panel edge.
  constexpr int16_t kPitch = 23;
  fui::TextStyle style;
  style.font = toybox::kTileFont;
  style.align = fui::TextAlign::Left;
  screen.target().text(toybox::inkCentred(fui::makeRect(boardLeft(device), top, kLabelW, kStripRow), toybox::kTileCut),
                       label, style);
  int at = 0;
  const int16_t cy = static_cast<int16_t>(top + kStripRow / 2);
  for (const uint8_t type : kOrder) {
    const int lost = kSet[type] - xq::countOf(board, owner, type);
    for (int i = 0; i < lost; ++i) {
      const int16_t cx = static_cast<int16_t>(boardLeft(device) + kLabelW + kSmall + at * kPitch);
      drawPiece(screen, cx, cy, kSmall, type, owner == xq::kBlack, toybox::kTileCut, toybox::kTileFont);
      ++at;
    }
  }
}

}  // namespace

const char* pieceLabel(const uint8_t type) {
  switch (type) {
    case xq::General:
      return "K";
    case xq::Advisor:
      return "A";
    case xq::Elephant:
      return "E";
    case xq::Horse:
      return "H";
    case xq::Chariot:
      return "R";
    case xq::Cannon:
      return "C";
    case xq::Soldier:
      return "P";
    default:
      return "";
  }
}

fui::Rect pointRect(const fui::DeviceContext& device, const int file, const int rank, const uint8_t seat) {
  const int column = seat == xq::kRed ? file : xq::kFiles - 1 - file;
  const int row = seat == xq::kRed ? rank : xq::kRanks - 1 - rank;
  return fui::makeRect(static_cast<int16_t>(boardLeft(device) + column * kCell),
                       static_cast<int16_t>(boardTop() + row * kCell), kCell, kCell);
}

bool pointAt(const fui::DeviceContext& device, const int x, const int y, const uint8_t seat, int& file, int& rank) {
  const int dx = x - boardLeft(device);
  const int dy = y - boardTop();
  if (dx < 0 || dy < 0 || dx >= kBoardW || dy >= kBoardH) return false;
  const int column = dx / kCell;
  const int row = dy / kCell;
  file = seat == xq::kRed ? column : xq::kFiles - 1 - column;
  rank = seat == xq::kRed ? row : xq::kRanks - 1 - row;
  return true;
}

int howToPages() { return 4; }

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "XIANGQI");

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
  const auto add = [&](const MenuRow row, const char* label) {
    rows[count].label = label;
    rows[count].actionValue = static_cast<int16_t>(row);
    ++count;
  };
  if (model.canContinue) add(MenuRow::Continue, "CONTINUE");
  add(MenuRow::Computer, "PLAY THE COMPUTER");
  add(MenuRow::Friend, "TWO PLAYERS");
  const int nearbyIndex = count;
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

  // The ornament: the two generals facing across an empty file, which is the
  // one position the rules forbid, and the one every player learns first.
  const int16_t areaTop = static_cast<int16_t>(line.bottom() + 6 + toybox::kRule);
  const int16_t room = static_cast<int16_t>(listBand.y - areaTop);
  if (room < 150) return;
  constexpr int16_t kR = 34;
  const fui::DeviceContext device = screen.device();
  const int16_t cx = static_cast<int16_t>(device.width / 2);
  const int16_t gap = static_cast<int16_t>(room - kR * 4 > 60 ? 60 : room - kR * 4);
  const int16_t topY = static_cast<int16_t>(areaTop + (room - kR * 4 - gap) / 2 + kR);
  const int16_t bottomY = static_cast<int16_t>(topY + kR * 2 + gap);
  screen.target().line(fui::Point{cx, static_cast<int16_t>(topY + kR)},
                       fui::Point{cx, static_cast<int16_t>(bottomY - kR)}, 2, fui::Paint::solid(fui::Color::Black));
  drawPiece(screen, cx, topY, kR, xq::General, true, toybox::kDisplayCut, toybox::kDisplayFont);
  drawPiece(screen, cx, bottomY, kR, xq::General, false, toybox::kDisplayCut, toybox::kDisplayFont);
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

  static const char* const kTitles[] = {"THE BOARD", "THE PIECES", "MORE PIECES", "WINNING"};
  fui::TextStyle title;
  title.font = toybox::kDisplayFont;
  title.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(fui::makeRect(body.x, body.y, body.width, 46), toybox::kDisplayCut),
                       kTitles[page], title);

  static const char* const kLines[] = {
      "PIECES STAND ON THE CROSSINGS. THE SHADED BAND IS THE RIVER. "
      "THE BOXES WITH DIAGONALS ARE THE PALACES. RED MOVES FIRST.",
      "K GENERAL AND A ADVISOR NEVER LEAVE THE PALACE: K STEPS STRAIGHT, A DIAGONALLY. "
      "E ELEPHANT MOVES TWO DIAGONALLY, CANNOT CROSS THE RIVER, AND IS BLOCKED BY A PIECE IN BETWEEN.",
      "R CHARIOT SLIDES LIKE A ROOK. H HORSE MOVES ONE STRAIGHT THEN ONE DIAGONAL, BLOCKED BY A PIECE "
      "BESIDE IT. C CANNON SLIDES, BUT TAKES ONLY BY JUMPING ONE PIECE. P SOLDIER STEPS FORWARD, AND SIDEWAYS ONCE "
      "ACROSS THE RIVER.",
      "CHECKMATE THE GENERAL. HAVING NO LEGAL MOVE LOSES. THE TWO GENERALS MAY NEVER FACE EACH OTHER "
      "ON AN OPEN FILE. THREE REPEATS IS A DRAW.",
  };
  fui::TextStyle cap;
  cap.font = toybox::kUiFont;
  cap.align = fui::TextAlign::Center;
  cap.maxLines = 10;
  const int16_t capTop = static_cast<int16_t>(body.y + 76);
  screen.target().text(fui::makeRect(body.x, capTop, body.width, static_cast<int16_t>(dotY - 50 - capTop)),
                       kLines[page], cap);
}

}  // namespace

void buildHowTo(toybox::Screen& screen, const HowToModel& model) { doBuildHowTo(screen, model); }

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  fui::HeaderProps header;
  header.title = "XIANGQI";
  header.borderEdges = fui::EdgesNone;
  toybox::headerBand(screen, header);

  const fui::DeviceContext device = screen.device();

  fui::ButtonProps status;
  status.label = model.status;
  status.action = fui::NO_ACTION;
  status.borderEdges = fui::EdgesNone;
  const fui::Rect capsule = fui::makeRect(boardLeft(device), capsuleTop(), kBoardW, toybox::kPillHeight);
  screen.button(
      status, model.opponentName != nullptr ? linkui::withOpponentFace(screen, capsule, model.opponentName) : capsule);

  const fui::Rect frame = fui::makeRect(static_cast<int16_t>(boardLeft(device) - toybox::kBoardFrame),
                                        static_cast<int16_t>(boardTop() - toybox::kBoardFrame),
                                        static_cast<int16_t>(kBoardW + toybox::kBoardFrame * 2),
                                        static_cast<int16_t>(kBoardH + toybox::kBoardFrame * 2));
  screen.target().stroke(frame, fui::Paint::solid(fui::Color::Black), toybox::kBoardFrame);
  drawLines(screen, device);

  for (int rank = 0; rank < xq::kRanks; ++rank) {
    for (int file = 0; file < xq::kFiles; ++file) {
      const uint8_t cell = model.board.cell[xq::indexOf(file, rank)];
      if (cell == 0) continue;
      const fui::Rect box = pointRect(device, file, rank, model.seat);
      drawPiece(screen, static_cast<int16_t>(box.x + kCell / 2), static_cast<int16_t>(box.y + kCell / 2), kPieceRadius,
                xq::typeOf(cell), xq::ownerOf(cell) == xq::kBlack, toybox::kUiCut, toybox::kUiFont);
    }
  }

  const uint8_t top = xq::other(model.seat);
  lostRow(screen, device, model.board, top, stripTop(), "THEM");
  lostRow(screen, device, model.board, model.seat, static_cast<int16_t>(stripTop() + kStripRow), "YOU");

  if (model.lastTo < xq::kPoints) {
    toybox::bracket(screen, pointRect(device, xq::fileOf(model.lastTo), xq::rankOf(model.lastTo), model.seat), 10, 3);
  }

  if (model.picked >= 0) {
    const fui::Rect box = pointRect(device, xq::fileOf(model.picked), xq::rankOf(model.picked), model.seat);
    screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), toybox::kFrame);
  }

  for (int i = 0; i < model.targets.count; ++i) {
    const int point = model.targets.to[i];
    const fui::Rect box = pointRect(device, xq::fileOf(point), xq::rankOf(point), model.seat);
    if (model.targets.capture[i]) {
      toybox::bracket(screen, box, 14, 4);
    } else {
      // Knocked out of the line first, so the dot reads as a mark rather than a
      // thick joint in the grid.
      toybox::disc(screen, static_cast<int16_t>(box.x + kCell / 2), static_cast<int16_t>(box.y + kCell / 2), 10,
                   fui::Color::White);
      toybox::disc(screen, static_cast<int16_t>(box.x + kCell / 2), static_cast<int16_t>(box.y + kCell / 2), 7,
                   fui::Color::Black);
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

}  // namespace xiangqiui
