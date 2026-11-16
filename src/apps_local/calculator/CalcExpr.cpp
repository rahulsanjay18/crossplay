#include "CalcExpr.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace calc {
namespace expr {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Function {
  const char* name;
  int id;
};

enum FunctionId { Sin, Cos, Tan, Asin, Acos, Atan, Ln, Log, Sqrt, Abs, Exp, Floor, Round };

// Longest names first, so "asin" is not read as "a" times "sin".
constexpr Function kFunctions[] = {{"asin", Asin}, {"acos", Acos}, {"atan", Atan},   {"sqrt", Sqrt},
                                   {"sin", Sin},   {"cos", Cos},   {"tan", Tan},     {"abs", Abs},
                                   {"exp", Exp},   {"log", Log},   {"floor", Floor}, {"round", Round},
                                   {"ln", Ln}};

class Parser {
 public:
  Parser(const std::string& text, const Context& context) : text_(text), context_(context) {}

  Result parse() {
    const double value = sum();
    skipSpaces();
    if (error_ == Error::None && at_ < text_.size()) fail(Error::Syntax);
    Result result;
    result.error = error_;
    result.at = errorAt_;
    if (error_ == Error::None && !std::isfinite(value)) result.error = Error::Overflow;
    result.value = result.error == Error::None ? value : 0;
    return result;
  }

 private:
  void fail(const Error error) {
    if (error_ != Error::None) return;
    error_ = error;
    errorAt_ = at_;
  }

  void skipSpaces() {
    while (at_ < text_.size() && text_[at_] == ' ') at_++;
  }

  bool accept(const char c) {
    skipSpaces();
    if (at_ < text_.size() && text_[at_] == c) {
      at_++;
      return true;
    }
    return false;
  }

  // Something that can begin a factor, for implicit multiplication: 2x, 2(3),
  // (a)(b), 3pi, 2sin(x).
  bool startsFactor() {
    skipSpaces();
    if (at_ >= text_.size()) return false;
    const char c = text_[at_];
    return c == '(' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.';
  }

  double sum() {
    double value = product();
    while (error_ == Error::None) {
      if (accept('+')) {
        value += product();
      } else if (accept('-')) {
        value -= product();
      } else {
        break;
      }
    }
    return value;
  }

  double product() {
    double value = unary();
    while (error_ == Error::None) {
      if (accept('*')) {
        value *= unary();
      } else if (accept('/')) {
        const double divisor = unary();
        if (divisor == 0) {
          fail(Error::DivideByZero);
          return 0;
        }
        value /= divisor;
      } else if (startsFactor()) {
        value *= power();
      } else {
        break;
      }
    }
    return value;
  }

  // Negation binds looser than ^, as on a TI-84: -2^2 is -4.
  double unary() {
    if (accept('-')) return -unary();
    if (accept('+')) return unary();
    return power();
  }

  double power() {
    const double base = postfix();
    if (accept('^')) {
      const double exponent = unary();
      if (error_ != Error::None) return 0;
      if (base < 0 && exponent != std::floor(exponent)) {
        fail(Error::Domain);
        return 0;
      }
      if (base == 0 && exponent < 0) {
        fail(Error::DivideByZero);
        return 0;
      }
      return std::pow(base, exponent);
    }
    return base;
  }

  double postfix() {
    double value = primary();
    while (error_ == Error::None && accept('!')) {
      if (value < 0 || value != std::floor(value) || value > 170) {
        fail(Error::Domain);
        return 0;
      }
      double product = 1;
      for (int i = 2; i <= static_cast<int>(value); i++) product *= i;
      value = product;
    }
    return value;
  }

  double primary() {
    skipSpaces();
    if (at_ >= text_.size()) {
      fail(Error::Syntax);
      return 0;
    }
    const char c = text_[at_];
    if (c == '(') {
      at_++;
      const double value = sum();
      if (!accept(')') && at_ < text_.size()) fail(Error::Syntax);  // a missing final ')' is allowed
      return value;
    }
    if ((c >= '0' && c <= '9') || c == '.') return number();
    for (const Function& function : kFunctions) {
      const size_t length = std::strlen(function.name);
      if (text_.compare(at_, length, function.name) == 0) {
        at_ += length;
        if (!accept('(')) {
          fail(Error::Syntax);
          return 0;
        }
        const double argument = sum();
        if (!accept(')') && at_ < text_.size()) fail(Error::Syntax);
        return apply(function.id, argument);
      }
    }
    if (text_.compare(at_, 2, "pi") == 0) {
      at_ += 2;
      return kPi;
    }
    if (text_.compare(at_, 3, "Ans") == 0) {
      at_ += 3;
      return context_.ans;
    }
    if (c == 'e') {
      at_++;
      return std::exp(1.0);
    }
    if (c >= 'A' && c <= 'Z') {
      at_++;
      return context_.variables[c - 'A'];
    }
    fail(Error::Syntax);
    return 0;
  }

  double number() {
    const size_t begin = at_;
    bool dot = false;
    while (at_ < text_.size() && ((text_[at_] >= '0' && text_[at_] <= '9') || (text_[at_] == '.' && !dot))) {
      if (text_[at_] == '.') dot = true;
      at_++;
    }
    // TI's exponent, written E as the calculator prints it.
    if (at_ < text_.size() && text_[at_] == 'E') {
      size_t end = at_ + 1;
      if (end < text_.size() && (text_[end] == '-' || text_[end] == '+')) end++;
      if (end < text_.size() && text_[end] >= '0' && text_[end] <= '9') {
        at_ = end;
        while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') at_++;
      }
    }
    const std::string digits = text_.substr(begin, at_ - begin);
    if (digits == ".") {
      fail(Error::Syntax);
      return 0;
    }
    return std::strtod(digits.c_str(), nullptr);
  }

  double toRadians(const double value) const {
    return context_.angle == Angle::Degrees ? value * kPi / 180.0 : value;
  }

  double fromRadians(const double value) const {
    return context_.angle == Angle::Degrees ? value * 180.0 / kPi : value;
  }

  double apply(const int id, const double x) {
    if (error_ != Error::None) return 0;
    switch (id) {
      case Sin:
        return std::sin(toRadians(x));
      case Cos:
        return std::cos(toRadians(x));
      case Tan: {
        const double radians = toRadians(x);
        if (std::fabs(std::cos(radians)) < 1e-12) {
          fail(Error::Domain);
          return 0;
        }
        return std::tan(radians);
      }
      case Asin:
      case Acos:
        if (x < -1 || x > 1) {
          fail(Error::Domain);
          return 0;
        }
        return fromRadians(id == Asin ? std::asin(x) : std::acos(x));
      case Atan:
        return fromRadians(std::atan(x));
      case Ln:
      case Log:
        if (x <= 0) {
          fail(Error::Domain);
          return 0;
        }
        return id == Ln ? std::log(x) : std::log10(x);
      case Sqrt:
        if (x < 0) {
          fail(Error::Domain);
          return 0;
        }
        return std::sqrt(x);
      case Abs:
        return std::fabs(x);
      case Exp:
        return std::exp(x);
      case Floor:
        return std::floor(x);
      case Round:
        return std::round(x);
      default:
        fail(Error::Syntax);
        return 0;
    }
  }

  const std::string& text_;
  const Context& context_;
  size_t at_ = 0;
  Error error_ = Error::None;
  size_t errorAt_ = 0;
};

}  // namespace

Result evaluate(const std::string& text, const Context& context) { return Parser(text, context).parse(); }

Statement run(const std::string& line, Context& context) {
  Statement statement;
  std::string expression = line;
  const size_t arrow = line.rfind("->");
  if (arrow != std::string::npos) {
    size_t at = arrow + 2;
    while (at < line.size() && line[at] == ' ') at++;
    size_t end = line.size();
    while (end > at && line[end - 1] == ' ') end--;
    if (end - at != 1 || line[at] < 'A' || line[at] > 'Z') {
      statement.result.error = Error::Syntax;
      statement.result.at = arrow;
      return statement;
    }
    statement.storedIn = line[at];
    expression = line.substr(0, arrow);
  }
  statement.result = evaluate(expression, context);
  if (statement.result.error != Error::None) {
    statement.storedIn = 0;
    return statement;
  }
  context.ans = statement.result.value;
  if (statement.storedIn != 0) context.variables[statement.storedIn - 'A'] = statement.result.value;
  return statement;
}

void format(double value, char* out, const size_t capacity) {
  if (capacity == 0) return;
  if (value == 0) value = 0;  // no "-0"
  const double magnitude = std::fabs(value);
  if (magnitude != 0 && (magnitude < 1e-5 || magnitude >= 1e10)) {
    char mantissa[32];
    std::snprintf(mantissa, sizeof(mantissa), "%.9e", value);
    // "1.230000000e+12" becomes "1.23E12".
    char* e = std::strchr(mantissa, 'e');
    const int exponent = e != nullptr ? std::atoi(e + 1) : 0;
    if (e != nullptr) *e = '\0';
    size_t length = std::strlen(mantissa);
    while (length > 0 && mantissa[length - 1] == '0') mantissa[--length] = '\0';
    if (length > 0 && mantissa[length - 1] == '.') mantissa[--length] = '\0';
    std::snprintf(out, capacity, "%sE%d", mantissa, exponent);
    return;
  }
  std::snprintf(out, capacity, "%.10g", value);
  // %.10g can still print an exponent near the edges; keep plain digits.
  if (std::strchr(out, 'e') != nullptr) std::snprintf(out, capacity, "%.10f", value);
  if (std::strchr(out, '.') != nullptr) {
    size_t length = std::strlen(out);
    while (length > 0 && out[length - 1] == '0') out[--length] = '\0';
    if (length > 0 && out[length - 1] == '.') out[--length] = '\0';
  }
  if (std::strcmp(out, "-0") == 0) std::snprintf(out, capacity, "0");
}

const char* errorText(const Error error) {
  switch (error) {
    case Error::None:
      return "";
    case Error::Syntax:
      return "ERR: SYNTAX";
    case Error::Domain:
      return "ERR: DOMAIN";
    case Error::DivideByZero:
      return "ERR: DIVIDE BY 0";
    case Error::Overflow:
      return "ERR: OVERFLOW";
  }
  return "";
}

}  // namespace expr
}  // namespace calc
