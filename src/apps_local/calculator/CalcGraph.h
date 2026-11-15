#pragma once

// Graphing: the window, plotting Y= functions to pixels, trace, zoom and the
// table. Freestanding, like CalcExpr.

#include <cstdint>
#include <string>
#include <vector>

#include "CalcExpr.h"

namespace calc {
namespace graph {

struct Window {
  double xMin = -10;
  double xMax = 10;
  double yMin = -10;
  double yMax = 10;
  double xScale = 1;
  double yScale = 1;
};

bool isValid(const Window& window);

// The pixel row of y, or kOffscreen when y is not finite. Rows count down from
// the top, so yMax is row 0. Values far outside are clamped so a steep line
// still reaches the edge.
constexpr int32_t kOffscreen = INT32_MIN;
int32_t rowFor(const Window& window, double y, int height);
int32_t columnFor(const Window& window, double x, int width);
double xAt(const Window& window, int column, int width);

// y for each of `width` columns, kOffscreen where the function is undefined.
std::vector<int32_t> plot(const std::string& function, expr::Context context, const Window& window, int width,
                          int height);

// Whether two neighbouring samples should be joined. Not across an undefined
// point, and not across a jump of more than a screen, which is what an
// asymptote looks like when sampled.
bool shouldJoin(int32_t from, int32_t to, int height);

// The value of `function` at x, for trace and the table.
expr::Result valueAt(const std::string& function, expr::Context context, double x);

Window standard();
Window zoomed(const Window& window, double factor, double x, double y);
// Same x range, y fitted to the function's values on it, with a little air.
Window zoomFit(const std::string& function, const expr::Context& context, const Window& window, int width);

}  // namespace graph
}  // namespace calc
