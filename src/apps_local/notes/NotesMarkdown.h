#pragma once

// The Markdown a page note draws, and the math inside it.
//
// Freestanding C++17, so host-tests/notes can drive every rule. It only reads:
// the file on the card stays the text the person wrote, and anything this does
// not recognise is shown as that text rather than dropped.

#include <cstdint>
#include <string>
#include <vector>

namespace notes {
namespace md {

enum Style : uint8_t {
  kPlain = 0,
  kBold = 1,
  kItalic = 2,
  kCode = 4,
  kMath = 8,
  kSuper = 16,
  kSub = 32,
};

struct Span {
  std::string text;  // UTF-8; the numerator when isFraction
  std::string denominator;
  uint8_t style = kPlain;
  bool isFraction = false;
};

enum class BlockKind : uint8_t { Paragraph, Heading, Bullet, Numbered, Quote, Code, Rule, DisplayMath };

struct Block {
  BlockKind kind = BlockKind::Paragraph;
  int level = 0;    // heading level, 1 to 6
  int number = 0;   // the Numbered list's own number
  size_t line = 0;  // index into the lines given to parse()
  std::vector<Span> spans;
};

std::vector<Span> parseInline(const std::string& text);
std::vector<Span> parseMath(const std::string& tex);
std::vector<Block> parse(const std::vector<std::string>& lines);

}  // namespace md
}  // namespace notes
