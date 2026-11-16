#pragma once

// The graphing calculator's arithmetic: typed expressions, variables and
// functions, in the way a TI-84 reads them.
//
// Binary doubles, unlike the four-function pad's decimal engine, because the
// functions a graph needs (trig, logs, powers) have no decimal implementation
// here. format() rounds to ten significant digits, so 0.1 + 0.2 still shows 0.3.
//
// Freestanding C++17 so host-tests/calcgraph can drive every rule.

#include <cstddef>
#include <cstdint>
#include <string>

namespace calc {
namespace expr {

enum class Angle : uint8_t { Radians, Degrees };

enum class Error : uint8_t { None, Syntax, Domain, DivideByZero, Overflow };

// A to Z, with X the graph's variable.
constexpr int kVariableCount = 26;

struct Context {
  double variables[kVariableCount] = {};
  double ans = 0;
  Angle angle = Angle::Radians;
};

struct Result {
  double value = 0;
  Error error = Error::None;
  size_t at = 0;  // where a syntax error was found, as a byte offset
};

// Evaluates `text` with X taken from context.variables.
Result evaluate(const std::string& text, const Context& context);

// One line typed on the home screen. "expression->A" stores into A as well.
// On success the value becomes Ans.
struct Statement {
  Result result;
  char storedIn = 0;  // 'A' to 'Z' when the line stored, otherwise 0
};
Statement run(const std::string& line, Context& context);

// Ten significant digits, trailing zeros dropped, scientific outside 1e-5 to
// 1e10. Writes at most `capacity - 1` characters.
void format(double value, char* out, size_t capacity);

const char* errorText(Error error);

}  // namespace expr
}  // namespace calc
