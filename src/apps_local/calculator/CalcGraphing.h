#pragma once

// The graphing mode: home screen, Y= editor, graph and table. Drawn straight
// into the renderer and hit-tested by geometry, like the four-function pad,
// because the keypad has more keys than the interaction table holds.

#include <cstdint>
#include <string>
#include <vector>

#include "CalcExpr.h"
#include "CalcGraph.h"
#include "CalcLayout.h"
#include "CalcTiPad.h"

class GfxRenderer;

namespace calc {

class Graphing {
 public:
  enum class Request : uint8_t { None, Redraw, OpenKeyboard, Basic };
  static constexpr int kFunctions = 3;

  void load();
  void draw(const GfxRenderer& renderer, const Rect16& body) const;
  Request tap(int x, int y, const Rect16& body);
  // A side button: trace on the graph, scroll on the table. True when used.
  bool step(int direction);
  // Back. False when already on the home screen, which leaves the mode.
  bool back();

  const std::string& keyboardText() const { return editing().text; }
  void keyboardDone(const std::string& text);

 private:
  enum class Screen : uint8_t { Home, YEquals, Graph, Table };
  struct Line {
    std::string input;
    std::string output;
  };

  ti::Entry& editing() { return screen_ == Screen::YEquals ? functions_[selected_] : entry_; }
  const ti::Entry& editing() const { return screen_ == Screen::YEquals ? functions_[selected_] : entry_; }
  Request press(const ti::Key& key);
  void enter();
  void save() const;
  int firstDefined() const;

  void drawHome(const GfxRenderer& renderer, const Rect16& display) const;
  void drawFunctions(const GfxRenderer& renderer, const Rect16& display) const;
  void drawGraph(const GfxRenderer& renderer, const Rect16& plot) const;
  void drawTable(const GfxRenderer& renderer, const Rect16& area) const;
  void drawPad(const GfxRenderer& renderer, const Rect16& grid) const;
  void drawControls(const GfxRenderer& renderer, const Rect16& strip) const;

  Screen screen_ = Screen::Home;
  expr::Context context_{};
  ti::Entry entry_;
  ti::Entry functions_[kFunctions];
  int selected_ = 0;
  std::vector<Line> history_;
  graph::Window window_{};
  int traceFunction_ = 0;
  int traceColumn_ = -1;
  double tableStart_ = 0;
  double tableStep_ = 1;
};

}  // namespace calc
