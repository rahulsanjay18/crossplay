#include "CalcGraph.h"

#include <climits>
#include <cmath>

namespace calc {
namespace graph {

namespace {
constexpr int kX = 'X' - 'A';
constexpr double kClampRows = 1e6;
}  // namespace

bool isValid(const Window& window) {
  return std::isfinite(window.xMin) && std::isfinite(window.xMax) && std::isfinite(window.yMin) &&
         std::isfinite(window.yMax) && window.xMax > window.xMin && window.yMax > window.yMin;
}

int32_t rowFor(const Window& window, const double y, const int height) {
  if (!std::isfinite(y) || height <= 1) return kOffscreen;
  double row = (window.yMax - y) / (window.yMax - window.yMin) * (height - 1);
  if (row > kClampRows) row = kClampRows;
  if (row < -kClampRows) row = -kClampRows;
  return static_cast<int32_t>(std::lround(row));
}

int32_t columnFor(const Window& window, const double x, const int width) {
  if (!std::isfinite(x) || width <= 1) return kOffscreen;
  double column = (x - window.xMin) / (window.xMax - window.xMin) * (width - 1);
  if (column > kClampRows) column = kClampRows;
  if (column < -kClampRows) column = -kClampRows;
  return static_cast<int32_t>(std::lround(column));
}

double xAt(const Window& window, const int column, const int width) {
  if (width <= 1) return window.xMin;
  return window.xMin + (window.xMax - window.xMin) * column / (width - 1);
}

expr::Result valueAt(const std::string& function, expr::Context context, const double x) {
  context.variables[kX] = x;
  return expr::evaluate(function, context);
}

std::vector<int32_t> plot(const std::string& function, expr::Context context, const Window& window, const int width,
                          const int height) {
  std::vector<int32_t> rows;
  if (width <= 0) return rows;
  rows.reserve(static_cast<size_t>(width));
  for (int column = 0; column < width; column++) {
    const expr::Result result = valueAt(function, context, xAt(window, column, width));
    rows.push_back(result.error == expr::Error::None ? rowFor(window, result.value, height) : kOffscreen);
  }
  return rows;
}

bool shouldJoin(const int32_t from, const int32_t to, const int height) {
  if (from == kOffscreen || to == kOffscreen) return false;
  const int64_t jump = static_cast<int64_t>(to) - from;
  return (jump < 0 ? -jump : jump) <= height;
}

Window standard() { return Window{}; }

Window zoomed(const Window& window, const double factor, const double x, const double y) {
  Window next = window;
  const double halfWidth = (window.xMax - window.xMin) * factor / 2;
  const double halfHeight = (window.yMax - window.yMin) * factor / 2;
  next.xMin = x - halfWidth;
  next.xMax = x + halfWidth;
  next.yMin = y - halfHeight;
  next.yMax = y + halfHeight;
  return isValid(next) ? next : window;
}

Window zoomFit(const std::string& function, const expr::Context& context, const Window& window, const int width) {
  double low = INFINITY;
  double high = -INFINITY;
  for (int column = 0; column < width; column++) {
    const expr::Result result = valueAt(function, context, xAt(window, column, width));
    if (result.error != expr::Error::None) continue;
    if (result.value < low) low = result.value;
    if (result.value > high) high = result.value;
  }
  if (!std::isfinite(low) || !std::isfinite(high)) return window;
  if (high - low < 1e-9) {
    low -= 1;
    high += 1;
  }
  const double air = (high - low) * 0.05;
  Window next = window;
  next.yMin = low - air;
  next.yMax = high + air;
  return isValid(next) ? next : window;
}

}  // namespace graph
}  // namespace calc
