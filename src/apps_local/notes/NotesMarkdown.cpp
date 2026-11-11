#include "NotesMarkdown.h"

#include <cstring>

namespace notes {
namespace md {
namespace {

struct Symbol {
  const char* name;
  const char* text;
};

constexpr Symbol kSymbols[] = {
    {"alpha", "α"},     {"beta", "β"},       {"gamma", "γ"},  {"delta", "δ"},  {"epsilon", "ε"}, {"zeta", "ζ"},
    {"eta", "η"},       {"theta", "θ"},      {"iota", "ι"},   {"kappa", "κ"},  {"lambda", "λ"},  {"mu", "μ"},
    {"nu", "ν"},        {"xi", "ξ"},         {"pi", "π"},     {"rho", "ρ"},    {"sigma", "σ"},   {"tau", "τ"},
    {"upsilon", "υ"},   {"phi", "φ"},        {"chi", "χ"},    {"psi", "ψ"},    {"omega", "ω"},   {"Gamma", "Γ"},
    {"Delta", "Δ"},     {"Theta", "Θ"},      {"Lambda", "Λ"}, {"Xi", "Ξ"},     {"Pi", "Π"},      {"Sigma", "Σ"},
    {"Phi", "Φ"},       {"Psi", "Ψ"},        {"Omega", "Ω"},  {"sum", "∑"},    {"prod", "∏"},    {"int", "∫"},
    {"infty", "∞"},     {"partial", "∂"},    {"nabla", "∇"},  {"pm", "±"},     {"times", "×"},   {"cdot", "·"},
    {"div", "÷"},       {"leq", "≤"},        {"le", "≤"},     {"geq", "≥"},    {"ge", "≥"},      {"neq", "≠"},
    {"ne", "≠"},        {"approx", "≈"},     {"equiv", "≡"},  {"in", "∈"},     {"notin", "∉"},   {"subset", "⊂"},
    {"cup", "∪"},       {"cap", "∩"},        {"forall", "∀"}, {"exists", "∃"}, {"to", "→"},      {"rightarrow", "→"},
    {"leftarrow", "←"}, {"Rightarrow", "⇒"}, {"iff", "⇔"},    {"degree", "°"}, {"ldots", "…"},   {"dots", "…"},
    {"sin", "sin"},     {"cos", "cos"},      {"tan", "tan"},  {"log", "log"},  {"ln", "ln"},     {"exp", "exp"},
    {"lim", "lim"},     {"min", "min"},      {"max", "max"},
};

const char* symbolFor(const std::string& name) {
  for (const Symbol& symbol : kSymbols) {
    if (name == symbol.name) return symbol.text;
  }
  return nullptr;
}

void append(std::vector<Span>& spans, const std::string& text, const uint8_t style) {
  if (text.empty()) return;
  if (!spans.empty() && !spans.back().isFraction && spans.back().style == style) {
    spans.back().text += text;
    return;
  }
  Span span;
  span.text = text;
  span.style = style;
  spans.push_back(std::move(span));
}

// The text of a balanced {group} at `at`, or the single character there. A
// multi-byte UTF-8 character counts as one.
std::string takeArgument(const std::string& tex, size_t& at) {
  while (at < tex.size() && tex[at] == ' ') at++;
  if (at >= tex.size()) return "";
  if (tex[at] == '{') {
    int depth = 1;
    const size_t begin = ++at;
    while (at < tex.size() && depth > 0) {
      if (tex[at] == '\\' && at + 1 < tex.size()) {
        at += 2;
        continue;
      }
      if (tex[at] == '{') depth++;
      if (tex[at] == '}') depth--;
      at++;
    }
    return tex.substr(begin, at - begin - (depth == 0 ? 1 : 0));
  }
  if (tex[at] == '\\') {
    const size_t begin = at++;
    while (at < tex.size() && ((tex[at] >= 'a' && tex[at] <= 'z') || (tex[at] >= 'A' && tex[at] <= 'Z'))) at++;
    if (at == begin + 1 && at < tex.size()) at++;
    return tex.substr(begin, at - begin);
  }
  const size_t begin = at++;
  while (at < tex.size() && (static_cast<unsigned char>(tex[at]) & 0xC0) == 0x80) at++;
  return tex.substr(begin, at - begin);
}

void mathInto(std::vector<Span>& spans, const std::string& tex, uint8_t style);

// A fraction part is one string, so its scripts become script characters.
std::string scriptText(const std::string& text, const bool super) {
  static constexpr const char* kSuperDigits[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
  static constexpr const char* kSubDigits[] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
  std::string out;
  for (const char c : text) {
    if (c >= '0' && c <= '9')
      out += super ? kSuperDigits[c - '0'] : kSubDigits[c - '0'];
    else if (c == '+')
      out += super ? "⁺" : "₊";
    else if (c == '-')
      out += super ? "⁻" : "₋";
    else if (c == 'n' && super)
      out += "ⁿ";
    else
      out += c;
  }
  return out;
}

std::string flatten(const std::string& tex) {
  std::vector<Span> spans;
  mathInto(spans, tex, kMath);
  std::string out;
  for (const Span& span : spans) {
    if (span.isFraction) {
      out += span.text + "/" + span.denominator;
    } else if ((span.style & (kSuper | kSub)) != 0) {
      out += scriptText(span.text, (span.style & kSuper) != 0);
    } else {
      out += span.text;
    }
  }
  return out;
}

void mathInto(std::vector<Span>& spans, const std::string& tex, const uint8_t style) {
  size_t at = 0;
  while (at < tex.size()) {
    const char c = tex[at];
    if (c == '^' || c == '_') {
      at++;
      const std::string argument = takeArgument(tex, at);
      mathInto(spans, argument, static_cast<uint8_t>(style | (c == '^' ? kSuper : kSub)));
      continue;
    }
    if (c == '{') {
      mathInto(spans, takeArgument(tex, at), style);
      continue;
    }
    if (c != '\\' || at + 1 >= tex.size()) {
      const size_t begin = at++;
      while (at < tex.size() && (static_cast<unsigned char>(tex[at]) & 0xC0) == 0x80) at++;
      append(spans, tex.substr(begin, at - begin), style);
      continue;
    }

    size_t end = at + 1;
    while (end < tex.size() && ((tex[end] >= 'a' && tex[end] <= 'z') || (tex[end] >= 'A' && tex[end] <= 'Z'))) end++;
    if (end == at + 1) {
      // A single non-letter: \, \; \! are spacing, and \{ \} \$ are literal.
      const char next = tex[end];
      if (next == ',' || next == ';' || next == ':')
        append(spans, " ", style);
      else if (next != '!')
        append(spans, std::string(1, next), style);
      at = end + 1;
      continue;
    }
    const std::string name = tex.substr(at + 1, end - at - 1);
    at = end;
    if (name == "left" || name == "right") continue;
    if (name == "frac") {
      Span fraction;
      fraction.isFraction = true;
      fraction.style = style;
      fraction.text = flatten(takeArgument(tex, at));
      fraction.denominator = flatten(takeArgument(tex, at));
      spans.push_back(std::move(fraction));
      continue;
    }
    if (name == "sqrt") {
      const std::string inner = flatten(takeArgument(tex, at));
      append(spans, inner.size() <= 1 ? "√" + inner : "√(" + inner + ")", style);
      continue;
    }
    if (name == "text" || name == "mathrm") {
      append(spans, takeArgument(tex, at), style);
      continue;
    }
    const char* symbol = symbolFor(name);
    append(spans, symbol != nullptr ? std::string(symbol) : "\\" + name, style);
  }
}

bool isWordByte(const char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c & 0x80) != 0;
}

// The closing `marker` at or after `from`, or npos. Backslash escapes it.
size_t findClosing(const std::string& text, const char* marker, const size_t from) {
  const size_t length = std::strlen(marker);
  for (size_t i = from; i + length <= text.size(); i++) {
    if (text[i] == '\\') {
      i++;
      continue;
    }
    if (text.compare(i, length, marker) == 0) return i;
  }
  return std::string::npos;
}

void inlineInto(std::vector<Span>& spans, const std::string& text, const uint8_t style) {
  std::string plain;
  const auto flush = [&] {
    append(spans, plain, style);
    plain.clear();
  };
  size_t at = 0;
  while (at < text.size()) {
    const char c = text[at];
    if (c == '\\' && at + 1 < text.size() && std::strchr("\\`*_$[]#>-", text[at + 1]) != nullptr) {
      plain += text[at + 1];
      at += 2;
      continue;
    }
    if (c == '`' || c == '$') {
      const size_t close = findClosing(text, c == '`' ? "`" : "$", at + 1);
      if (close != std::string::npos && close > at + 1) {
        flush();
        const std::string inner = text.substr(at + 1, close - at - 1);
        if (c == '`')
          append(spans, inner, static_cast<uint8_t>(style | kCode));
        else
          mathInto(spans, inner, static_cast<uint8_t>(style | kMath));
        at = close + 1;
        continue;
      }
    }
    if (c == '*' || c == '_') {
      const bool twice = at + 1 < text.size() && text[at + 1] == c;
      const char marker[3] = {c, twice ? c : '\0', '\0'};
      const size_t length = twice ? 2 : 1;
      const bool opensInsideWord = at > 0 && isWordByte(text[at - 1]);
      const bool followedBySpace = at + length >= text.size() || text[at + length] == ' ';
      if (!followedBySpace && !(c == '_' && opensInsideWord)) {
        const size_t close = findClosing(text, marker, at + length);
        if (close != std::string::npos && close > at + length) {
          flush();
          inlineInto(spans, text.substr(at + length, close - at - length),
                     static_cast<uint8_t>(style | (twice ? kBold : kItalic)));
          at = close + length;
          continue;
        }
      }
    }
    plain += c;
    at++;
  }
  flush();
}

std::string trimmed(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) begin++;
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) end--;
  return text.substr(begin, end - begin);
}

bool isRule(const std::string& line) {
  if (line.size() < 3 || (line[0] != '-' && line[0] != '*' && line[0] != '_')) return false;
  for (const char c : line) {
    if (c != line[0]) return false;
  }
  return true;
}

}  // namespace

std::vector<Span> parseInline(const std::string& text) {
  std::vector<Span> spans;
  inlineInto(spans, text, kPlain);
  return spans;
}

std::vector<Span> parseMath(const std::string& tex) {
  std::vector<Span> spans;
  mathInto(spans, tex, kMath);
  return spans;
}

std::vector<Block> parse(const std::vector<std::string>& lines) {
  std::vector<Block> blocks;
  blocks.reserve(lines.size());
  size_t i = 0;
  const auto add = [&blocks](const BlockKind kind, const size_t line) -> Block& {
    Block block;
    block.kind = kind;
    block.line = line;
    blocks.push_back(std::move(block));
    return blocks.back();
  };
  while (i < lines.size()) {
    const std::string line = trimmed(lines[i]);
    if (line.empty()) {
      i++;
      continue;
    }
    if (line.compare(0, 3, "```") == 0) {
      // An unclosed fence is not a code block: the rest of the note would vanish
      // into one for want of three backticks.
      size_t close = i + 1;
      while (close < lines.size() && trimmed(lines[close]).compare(0, 3, "```") != 0) close++;
      if (close < lines.size()) {
        for (size_t c = i + 1; c < close; c++) {
          Block& code = add(BlockKind::Code, c);
          append(code.spans, lines[c].empty() ? " " : lines[c], kCode);
        }
        i = close + 1;
        continue;
      }
    }
    if (line.compare(0, 2, "$$") == 0) {
      std::string tex;
      size_t last = i;
      bool closed = false;
      if (line.size() >= 4 && line.compare(line.size() - 2, 2, "$$") == 0) {
        tex = line.substr(2, line.size() - 4);
        closed = true;
      } else {
        tex = line.substr(2);
        for (size_t c = i + 1; c < lines.size() && !closed; c++) {
          const std::string next = trimmed(lines[c]);
          const size_t end = next.find("$$");
          tex += " " + next.substr(0, end);
          if (end != std::string::npos) {
            closed = true;
            last = c;
          }
        }
      }
      if (closed) {
        add(BlockKind::DisplayMath, i).spans = parseMath(trimmed(tex));
        i = last + 1;
        continue;
      }
    }
    if (isRule(line)) {
      add(BlockKind::Rule, i);
      i++;
      continue;
    }
    size_t hashes = 0;
    while (hashes < line.size() && line[hashes] == '#') hashes++;
    if (hashes >= 1 && hashes <= 6 && hashes < line.size() && line[hashes] == ' ') {
      Block& heading = add(BlockKind::Heading, i);
      heading.level = static_cast<int>(hashes);
      heading.spans = parseInline(trimmed(line.substr(hashes)));
      i++;
      continue;
    }
    if (line.compare(0, 2, "> ") == 0 || line == ">") {
      add(BlockKind::Quote, i).spans = parseInline(line.size() > 2 ? line.substr(2) : "");
      i++;
      continue;
    }
    if (line.size() > 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ') {
      add(BlockKind::Bullet, i).spans = parseInline(trimmed(line.substr(2)));
      i++;
      continue;
    }
    size_t digits = 0;
    while (digits < line.size() && digits < 9 && line[digits] >= '0' && line[digits] <= '9') digits++;
    if (digits > 0 && digits + 1 < line.size() && line[digits] == '.' && line[digits + 1] == ' ') {
      Block& item = add(BlockKind::Numbered, i);
      item.number = std::stoi(line.substr(0, digits));
      item.spans = parseInline(trimmed(line.substr(digits + 2)));
      i++;
      continue;
    }
    add(BlockKind::Paragraph, i).spans = parseInline(line);
    i++;
  }
  return blocks;
}

}  // namespace md
}  // namespace notes
