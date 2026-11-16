#include "CalcGraphing.h"

#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../ui/ToyboxFonts.h"
#include "CalcFonts.h"
#include "CalcStyle.h"

namespace calc {
namespace {

// Lines of history and entry above the keypad: two calculations and the line
// being typed.
constexpr int kDisplayLines = 5;

constexpr const char* kSavePath = "/.crosspoint/calc-graph.txt";
constexpr int kHistoryKept = 24;
constexpr int16_t kPadGap = 6;
constexpr int16_t kControlHeight = 64;
constexpr int kX = 'X' - 'A';

enum Control : uint8_t { ZoomIn, ZoomOut, Standard, Fit, NextFunction, Home, TableUp, TableDown, TableGraph };

struct ControlKey {
  const char* label;
  Control control;
};

constexpr ControlKey kGraphControls[] = {{"IN", ZoomIn}, {"OUT", ZoomOut},     {"STD", Standard},
                                         {"FIT", Fit},   {"Yn", NextFunction}, {"HOME", Home}};
constexpr ControlKey kTableControls[] = {{"UP", TableUp}, {"DOWN", TableDown}, {"GRAPH", TableGraph}, {"HOME", Home}};

int16_t lineHeight() { return kSmallCut.lineHeight; }

Rect16 displayOf(const Rect16& body) {
  return Rect16{body.x, body.y, body.w, static_cast<int16_t>(kDisplayLines * lineHeight() + kRule + 8)};
}

Rect16 gridOf(const Rect16& body) {
  const Rect16 display = displayOf(body);
  const int16_t top = static_cast<int16_t>(display.bottom() + kPadGap);
  return Rect16{body.x, top, body.w, static_cast<int16_t>(body.bottom() - top)};
}

Rect16 stripOf(const Rect16& body) {
  return Rect16{body.x, static_cast<int16_t>(body.bottom() - kControlHeight), body.w, kControlHeight};
}

Rect16 controlRect(const Rect16& strip, const int count, const int index) {
  const int16_t width = static_cast<int16_t>((strip.w - (count - 1) * kPadGap) / count);
  return Rect16{static_cast<int16_t>(strip.x + index * (width + kPadGap)), strip.y, width, strip.h};
}

int controlAt(const Rect16& strip, const int count, const int x, const int y) {
  for (int i = 0; i < count; i++) {
    const Rect16 r = controlRect(strip, count, i);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) return i;
  }
  return -1;
}

// The area above the strip, less one line for the readout.
Rect16 plotOf(const Rect16& body) {
  const int16_t readout = static_cast<int16_t>(lineHeight() + 6);
  return Rect16{body.x, static_cast<int16_t>(body.y + readout), body.w,
                static_cast<int16_t>(stripOf(body).y - kPadGap - body.y - readout)};
}

void drawLeft(const GfxRenderer& renderer, const int x, const int y, const char* text, const bool black = true) {
  renderer.drawText(kSmallFontId, x, y, text, black);
}

void drawRight(const GfxRenderer& renderer, const int right, const int y, const char* text) {
  renderer.drawText(kSmallFontId, right - renderer.getTextWidth(kSmallFontId, text), y, text, true);
}

// Draws `text` right-aligned and cut from the LEFT when too wide: on a
// calculator the end of a line is what was just typed.
void drawTail(const GfxRenderer& renderer, const int x, const int width, const int y, const std::string& text) {
  size_t from = 0;
  while (from < text.size() && renderer.getTextWidth(kSmallFontId, text.c_str() + from) > width) from++;
  drawRight(renderer, x + width, y, text.c_str() + from);
}

void formatted(const double value, char* out, const size_t capacity) { expr::format(value, out, capacity); }

// Six significant digits, for the trace readout and table columns, which hold
// several numbers to a line.
void brief(const double value, char* out, const size_t capacity) {
  std::snprintf(out, capacity, "%.6g", value == 0 ? 0.0 : value);
}

constexpr int kLabelFontCount = 4;

// Characters, not bytes: the operator keys are one character in two bytes.
size_t characters(const char* label) {
  size_t count = 0;
  for (const char* c = label; *c != '\0'; c++) {
    if ((static_cast<unsigned char>(*c) & 0xC0) != 0x80) count++;
  }
  return count;
}

bool isAscii(const char* label) {
  for (const char* c = label; *c != '\0'; c++) {
    if ((static_cast<unsigned char>(*c) & 0x80) != 0) return false;
  }
  return true;
}

// The last two rungs are Toybox cuts, which carry ASCII only; the operator
// glyphs stay on the calculator's own cuts.
int labelFont(int rung, const char* label) {
  if (!isAscii(label) && rung > 1) rung = 1;
  const int fonts[kLabelFontCount] = {labelFontFor(label), kSmallFontId, toybox::kButtonFontId, toybox::kTileFontId};
  return fonts[rung];
}

// The first rung at which `label` fits a key `width` wide.
int labelRung(const GfxRenderer& renderer, const char* label, const int width) {
  for (int rung = 0; rung < kLabelFontCount; rung++) {
    if (renderer.getTextWidth(labelFont(rung, label), label) <= width - 8) return rung;
  }
  return kLabelFontCount - 1;
}

void drawButton(const GfxRenderer& renderer, const Rect16& r, const char* label, const bool filled, const int rung) {
  if (filled) {
    renderer.fillRect(r.x, r.y, r.w, r.h, true);
  } else {
    renderer.drawRect(r.x, r.y, r.w, r.h, kDigitStroke, true);
  }
  drawCapsCenteredIn(renderer, labelFont(rung, label), r.x, r.w, r.y, r.h, label, !filled);
}

// Clips the segment between two neighbouring columns to rows [0, height) and
// draws what is left.
void drawSegment(const GfxRenderer& renderer, const Rect16& plot, const int column, int32_t from, int32_t to) {
  double x0 = column;
  double y0 = from;
  double x1 = column + 1;
  double y1 = to;
  const double low = 0;
  const double high = plot.h - 1;
  if ((y0 < low && y1 < low) || (y0 > high && y1 > high)) return;
  const auto clip = [&](double& x, double& y, const double otherX, const double otherY) {
    if (y < low || y > high) {
      const double edge = y < low ? low : high;
      x = x + (otherX - x) * (edge - y) / (otherY - y);
      y = edge;
    }
  };
  clip(x0, y0, x1, y1);
  clip(x1, y1, x0, y0);
  renderer.drawLine(plot.x + static_cast<int>(std::lround(x0)), plot.y + static_cast<int>(std::lround(y0)),
                    plot.x + static_cast<int>(std::lround(x1)), plot.y + static_cast<int>(std::lround(y1)), 2, true);
}

}  // namespace

void Graphing::load() {
  char buffer[512] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer)) == 0) return;
  char* line = std::strtok(buffer, "\n");
  int index = 0;
  while (line != nullptr) {
    if (index == 0) {
      context_.angle = line[0] == 'D' ? expr::Angle::Degrees : expr::Angle::Radians;
    } else if (index == 1) {
      graph::Window window;
      if (std::sscanf(line, "%lf %lf %lf %lf %lf %lf", &window.xMin, &window.xMax, &window.yMin, &window.yMax,
                      &window.xScale, &window.yScale) == 6 &&
          graph::isValid(window)) {
        window_ = window;
      }
    } else if (index <= 1 + kFunctions && line[0] == 'Y') {
      // "Y1=" and the function.
      const char* equals = std::strchr(line, '=');
      if (equals != nullptr) functions_[index - 2].set(equals + 1);
    }
    index++;
    line = std::strtok(nullptr, "\n");
  }
}

void Graphing::save() const {
  std::string text = context_.angle == expr::Angle::Degrees ? "DEG\n" : "RAD\n";
  char window[160];
  std::snprintf(window, sizeof(window), "%.12g %.12g %.12g %.12g %.12g %.12g\n", window_.xMin, window_.xMax,
                window_.yMin, window_.yMax, window_.xScale, window_.yScale);
  text += window;
  for (int i = 0; i < kFunctions; i++) {
    text += "Y" + std::to_string(i + 1) + "=" + functions_[i].text + "\n";
  }
  Storage.writeFile(kSavePath, String(text.c_str()));
}

int Graphing::firstDefined() const {
  for (int i = 0; i < kFunctions; i++) {
    if (!functions_[i].text.empty()) return i;
  }
  return -1;
}

void Graphing::enter() {
  if (screen_ == Screen::YEquals) {
    selected_ = (selected_ + 1) % kFunctions;
    save();
    return;
  }
  if (entry_.text.empty()) return;
  const expr::Statement statement = expr::run(entry_.text, context_);
  Line line;
  line.input = entry_.text;
  char value[40];
  if (statement.result.error == expr::Error::None) {
    formatted(statement.result.value, value, sizeof(value));
    line.output = value;
  } else {
    line.output = expr::errorText(statement.result.error);
  }
  if (history_.size() >= kHistoryKept) history_.erase(history_.begin());
  history_.push_back(std::move(line));
  entry_.clear();
}

Graphing::Request Graphing::press(const ti::Key& key) {
  switch (key.action) {
    case ti::Action::Insert:
      editing().insert(key.insert);
      return Request::Redraw;
    case ti::Action::Store:
      editing().insert("->");
      return Request::Redraw;
    case ti::Action::Enter:
      enter();
      return Request::Redraw;
    case ti::Action::Delete:
      editing().backspace();
      return Request::Redraw;
    case ti::Action::Clear:
      if (screen_ == Screen::Home && entry_.text.empty()) history_.clear();
      editing().clear();
      if (screen_ == Screen::YEquals) save();
      return Request::Redraw;
    case ti::Action::Left:
      editing().left();
      return Request::Redraw;
    case ti::Action::Right:
      editing().right();
      return Request::Redraw;
    case ti::Action::YEquals:
      screen_ = Screen::YEquals;
      return Request::Redraw;
    case ti::Action::Graph:
      save();
      screen_ = Screen::Graph;
      traceColumn_ = -1;
      traceFunction_ = firstDefined() < 0 ? 0 : firstDefined();
      return Request::Redraw;
    case ti::Action::Table:
      save();
      screen_ = Screen::Table;
      return Request::Redraw;
    case ti::Action::Angle:
      context_.angle = context_.angle == expr::Angle::Degrees ? expr::Angle::Radians : expr::Angle::Degrees;
      save();
      return Request::Redraw;
    case ti::Action::Keyboard:
      return Request::OpenKeyboard;
    case ti::Action::Basic:
      return Request::Basic;
  }
  return Request::None;
}

Graphing::Request Graphing::tap(const int x, const int y, const Rect16& body) {
  if (screen_ == Screen::Graph) {
    const Rect16 strip = stripOf(body);
    const int count = static_cast<int>(sizeof(kGraphControls) / sizeof(kGraphControls[0]));
    const int index = controlAt(strip, count, x, y);
    if (index < 0) {
      const Rect16 plot = plotOf(body);
      if (x >= plot.x && x < plot.right() && y >= plot.y && y < plot.bottom()) {
        traceColumn_ = x - plot.x;
        return Request::Redraw;
      }
      return Request::None;
    }
    const double centreX = (window_.xMin + window_.xMax) / 2;
    const double centreY = (window_.yMin + window_.yMax) / 2;
    switch (kGraphControls[index].control) {
      case ZoomIn:
        window_ = graph::zoomed(window_, 0.5, centreX, centreY);
        break;
      case ZoomOut:
        window_ = graph::zoomed(window_, 2, centreX, centreY);
        break;
      case Standard:
        window_ = graph::standard();
        break;
      case Fit:
        if (firstDefined() >= 0) {
          window_ = graph::zoomFit(functions_[traceFunction_].text.empty() ? functions_[firstDefined()].text
                                                                           : functions_[traceFunction_].text,
                                   context_, window_, plotOf(body).w);
        }
        break;
      case NextFunction:
        for (int i = 1; i <= kFunctions; i++) {
          const int next = (traceFunction_ + i) % kFunctions;
          if (!functions_[next].text.empty()) {
            traceFunction_ = next;
            break;
          }
        }
        break;
      case Home:
        screen_ = Screen::Home;
        break;
      default:
        break;
    }
    save();
    return Request::Redraw;
  }

  if (screen_ == Screen::Table) {
    const Rect16 strip = stripOf(body);
    const int count = static_cast<int>(sizeof(kTableControls) / sizeof(kTableControls[0]));
    const int index = controlAt(strip, count, x, y);
    if (index < 0) return Request::None;
    switch (kTableControls[index].control) {
      case TableUp:
        step(-1);
        break;
      case TableDown:
        step(1);
        break;
      case TableGraph:
        screen_ = Screen::Graph;
        break;
      case Home:
        screen_ = Screen::Home;
        break;
      default:
        break;
    }
    return Request::Redraw;
  }

  const Rect16 display = displayOf(body);
  if (screen_ == Screen::YEquals && y >= display.y && y < display.bottom()) {
    const int row = (y - display.y) / lineHeight();
    if (row >= 0 && row < kFunctions) {
      selected_ = row;
      return Request::Redraw;
    }
  }
  const int index = ti::keyAt(gridOf(body), kPadGap, x, y);
  if (index < 0) return Request::None;
  return press(ti::kKeys[index]);
}

bool Graphing::step(const int direction) {
  if (screen_ == Screen::Graph) {
    traceColumn_ = traceColumn_ < 0 ? 0 : traceColumn_ + direction;
    if (traceColumn_ < 0) traceColumn_ = 0;
    return true;
  }
  if (screen_ == Screen::Table) {
    tableStart_ += direction * tableStep_ * 8;
    return true;
  }
  return false;
}

bool Graphing::back() {
  if (screen_ == Screen::Home) return false;
  screen_ = Screen::Home;
  return true;
}

void Graphing::keyboardDone(const std::string& text) {
  editing().set(text);
  if (screen_ == Screen::YEquals) save();
}

void Graphing::drawHome(const GfxRenderer& renderer, const Rect16& display) const {
  const int16_t height = lineHeight();
  const int lines = kDisplayLines - 1;
  // Each history entry takes two lines: what was typed, then its answer.
  int y = display.y + (lines - 1) * height;
  for (int i = static_cast<int>(history_.size()) - 1; i >= 0 && y >= display.y; i--) {
    drawTail(renderer, display.x, display.w, y, history_[static_cast<size_t>(i)].output);
    y -= height;
    if (y < display.y) break;
    drawLeft(renderer, display.x, y, history_[static_cast<size_t>(i)].input.c_str());
    y -= height;
  }
  const int entryY = display.y + lines * height;
  std::string shown = entry_.text;
  shown.insert(entry_.cursor, "|");
  drawLeft(renderer, display.x, entryY, shown.c_str());
  const char* mode = context_.angle == expr::Angle::Degrees ? "DEG" : "RAD";
  drawRight(renderer, display.right(), display.y, mode);
}

void Graphing::drawFunctions(const GfxRenderer& renderer, const Rect16& display) const {
  for (int i = 0; i < kFunctions; i++) {
    const int y = display.y + i * lineHeight();
    std::string shown = "Y" + std::to_string(i + 1) + "=" + functions_[i].text;
    if (i == selected_) {
      shown.insert(3 + functions_[i].cursor, "|");
      renderer.fillRect(display.x, y + lineHeight() - 3, 10, 3, true);
    }
    drawLeft(renderer, display.x + 14, y, shown.c_str());
  }
}

void Graphing::drawGraph(const GfxRenderer& renderer, const Rect16& plot) const {
  renderer.drawRect(plot.x, plot.y, plot.w, plot.h, 2, true);
  const int32_t axisRow = graph::rowFor(window_, 0, plot.h);
  const int32_t axisColumn = graph::columnFor(window_, 0, plot.w);
  if (axisRow >= 0 && axisRow < plot.h) renderer.drawLine(plot.x, plot.y + axisRow, plot.right() - 1, plot.y + axisRow);
  if (axisColumn >= 0 && axisColumn < plot.w) {
    renderer.drawLine(plot.x + axisColumn, plot.y, plot.x + axisColumn, plot.bottom() - 1);
  }
  // Tick marks, unless there would be so many they make a solid bar.
  if (window_.xScale > 0 && (window_.xMax - window_.xMin) / window_.xScale <= 60 && axisRow >= 0 && axisRow < plot.h) {
    for (double x = std::ceil(window_.xMin / window_.xScale) * window_.xScale; x <= window_.xMax; x += window_.xScale) {
      const int32_t column = graph::columnFor(window_, x, plot.w);
      renderer.drawLine(plot.x + column, plot.y + axisRow - 3, plot.x + column, plot.y + axisRow + 3);
    }
  }
  if (window_.yScale > 0 && (window_.yMax - window_.yMin) / window_.yScale <= 60 && axisColumn >= 0 &&
      axisColumn < plot.w) {
    for (double y = std::ceil(window_.yMin / window_.yScale) * window_.yScale; y <= window_.yMax; y += window_.yScale) {
      const int32_t row = graph::rowFor(window_, y, plot.h);
      renderer.drawLine(plot.x + axisColumn - 3, plot.y + row, plot.x + axisColumn + 3, plot.y + row);
    }
  }
  for (int f = 0; f < kFunctions; f++) {
    if (functions_[f].text.empty()) continue;
    const std::vector<int32_t> rows = graph::plot(functions_[f].text, context_, window_, plot.w, plot.h);
    for (int column = 0; column + 1 < static_cast<int>(rows.size()); column++) {
      if (graph::shouldJoin(rows[column], rows[column + 1], plot.h)) {
        drawSegment(renderer, plot, column, rows[column], rows[column + 1]);
      }
    }
  }

  // The readout: which function, and where the trace is.
  char readout[96];
  const int column = traceColumn_ < 0 ? plot.w / 2 : (traceColumn_ >= plot.w ? plot.w - 1 : traceColumn_);
  const std::string& traced = functions_[traceFunction_].text;
  if (traced.empty()) {
    std::snprintf(readout, sizeof(readout), "Enter a function with Y=");
  } else {
    const double x = graph::xAt(window_, column, plot.w);
    const expr::Result result = graph::valueAt(traced, context_, x);
    char xText[32];
    char yText[32];
    brief(x, xText, sizeof(xText));
    if (result.error == expr::Error::None) {
      brief(result.value, yText, sizeof(yText));
    } else {
      std::snprintf(yText, sizeof(yText), "undefined");
    }
    std::snprintf(readout, sizeof(readout), "Y%d  X=%s  Y=%s", traceFunction_ + 1, xText, yText);
    if (traceColumn_ >= 0 && result.error == expr::Error::None) {
      const int32_t row = graph::rowFor(window_, result.value, plot.h);
      if (row >= 0 && row < plot.h) {
        renderer.drawRect(plot.x + column - 4, plot.y + row - 4, 9, 9, 2, true);
      }
    }
  }
  drawLeft(renderer, plot.x, plot.y - lineHeight() - 4, readout);
}

void Graphing::drawTable(const GfxRenderer& renderer, const Rect16& area) const {
  // Several numbers to a line, so the smaller cut.
  const int font = toybox::kButtonFontId;
  const int16_t rowHeight = static_cast<int16_t>(renderer.getLineHeight(font) + 4);
  int functions = 0;
  for (int i = 0; i < kFunctions; i++) {
    if (!functions_[i].text.empty()) functions++;
  }
  // X takes a narrower column than the functions: its values are short.
  const int unit = area.w * 5 / (3 + 5 * functions);
  const int xWidth = area.w - unit * functions;
  const auto right = [&](const int edge, const int y, const char* text) {
    renderer.drawText(font, edge - renderer.getTextWidth(font, text), y, text, true);
  };
  // Fewer digits until the number fits its column, so columns never meet.
  const auto fitted = [&](const double value, char* out, const size_t capacity, const int width) {
    for (int digits = 6; digits >= 2; digits--) {
      std::snprintf(out, capacity, "%.*g", digits, value == 0 ? 0.0 : value);
      if (renderer.getTextWidth(font, out) <= width) return;
    }
  };
  int y = area.y;
  renderer.drawText(font, area.x, y, "X", true);
  int edge = area.x + xWidth;
  for (int i = 0; i < kFunctions; i++) {
    if (functions_[i].text.empty()) continue;
    edge += unit;
    char head[16];
    std::snprintf(head, sizeof(head), "Y%d", i + 1);
    right(edge, y, head);
  }
  y += rowHeight;
  renderer.fillRect(area.x, y, area.w, kRule, true);
  y += kRule + 4;
  for (int row = 0; y + rowHeight <= area.bottom(); row++) {
    const double x = tableStart_ + row * tableStep_;
    char text[32];
    fitted(x, text, sizeof(text), xWidth - 8);
    renderer.drawText(font, area.x, y, text, true);
    edge = area.x + xWidth;
    for (int i = 0; i < kFunctions; i++) {
      if (functions_[i].text.empty()) continue;
      edge += unit;
      const expr::Result result = graph::valueAt(functions_[i].text, context_, x);
      if (result.error == expr::Error::None) {
        fitted(result.value, text, sizeof(text), unit - 10);
      } else {
        std::snprintf(text, sizeof(text), "ERR");
      }
      right(edge, y, text);
    }
    y += rowHeight;
  }
}

void Graphing::drawPad(const GfxRenderer& renderer, const Rect16& grid) const {
  const auto labelOf = [this](const ti::Key& key) {
    if (key.action != ti::Action::Angle) return key.label;
    return context_.angle == expr::Angle::Degrees ? "RAD" : "DEG";
  };
  // One size per row, so the keys of a row read as a set: the rung is the
  // smallest any key in the row needs.
  int rowRung[ti::kRows] = {};
  for (int i = 0; i < ti::kKeyCount; i++) {
    const Rect16 r = ti::keyRect(grid, kPadGap, i);
    const int row = ti::keyStart(i) / ti::kColumns;
    // A one-character key fits at full size anywhere, so it neither sets nor
    // takes its row's size.
    if (characters(labelOf(ti::kKeys[i])) <= 1) continue;
    const int rung = labelRung(renderer, labelOf(ti::kKeys[i]), r.w);
    if (rung > rowRung[row]) rowRung[row] = rung;
  }
  for (int i = 0; i < ti::kKeyCount; i++) {
    const ti::Key& key = ti::kKeys[i];
    const bool screenKey = (key.action == ti::Action::YEquals && screen_ == Screen::YEquals);
    const int rung = characters(labelOf(key)) <= 1 ? 0 : rowRung[ti::keyStart(i) / ti::kColumns];
    drawButton(renderer, ti::keyRect(grid, kPadGap, i), labelOf(key), key.action == ti::Action::Enter || screenKey,
               rung);
  }
}

void Graphing::drawControls(const GfxRenderer& renderer, const Rect16& strip) const {
  const bool graphScreen = screen_ == Screen::Graph;
  const ControlKey* keys = graphScreen ? kGraphControls : kTableControls;
  const int count = graphScreen ? static_cast<int>(sizeof(kGraphControls) / sizeof(kGraphControls[0]))
                                : static_cast<int>(sizeof(kTableControls) / sizeof(kTableControls[0]));
  int rung = 0;
  for (int i = 0; i < count; i++) {
    const int needed = labelRung(renderer, keys[i].label, controlRect(strip, count, i).w);
    if (needed > rung) rung = needed;
  }
  for (int i = 0; i < count; i++) drawButton(renderer, controlRect(strip, count, i), keys[i].label, false, rung);
}

void Graphing::draw(const GfxRenderer& renderer, const Rect16& body) const {
  if (screen_ == Screen::Graph) {
    drawGraph(renderer, plotOf(body));
    drawControls(renderer, stripOf(body));
    return;
  }
  if (screen_ == Screen::Table) {
    drawTable(renderer, Rect16{body.x, body.y, body.w, static_cast<int16_t>(stripOf(body).y - kPadGap - body.y)});
    drawControls(renderer, stripOf(body));
    return;
  }
  const Rect16 display = displayOf(body);
  if (screen_ == Screen::YEquals) {
    drawFunctions(renderer, display);
  } else {
    drawHome(renderer, display);
  }
  renderer.fillRect(display.x, display.bottom() - kRule, display.w, kRule, true);
  drawPad(renderer, gridOf(body));
}

}  // namespace calc
