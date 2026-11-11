// The note format, checked without a panel.
//
// The claim under most scrutiny is that a tick changes ONE BYTE and leaves the
// rest of the file alone. That is not a property to sample; it is the reason
// the format is strict, so the test holds it to account directly: it ticks
// every task in a document full of awkward prose and asserts the whole buffer
// is byte-identical apart from the single mark.

#include <cstdio>
#include <string>
#include <vector>

#include "NotesCore.h"
#include "NotesMarkdown.h"

using namespace notes;

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++checks;                                                     \
    if (!(cond)) {                                                \
      ++failures;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

namespace {

size_t differingBytes(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return static_cast<size_t>(-1);
  size_t n = 0;
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i] != b[i]) n++;
  }
  return n;
}

void testWhatIsATask() {
  const std::string doc =
      "Shopping\n"
      "- [ ] Milk\n"
      "- [x] Eggs\n"
      "- [X] Bread\n"
      "  - [ ] Indented is still a task\n"
      "* [ ] Star bullet\n"
      "+ [ ] Plus bullet\n"
      "- [] No space is prose\n"
      "-[ ] No bullet space is prose\n"
      "- [ ]\n"
      "- [x]tra credit is prose\n"
      "Just a sentence with - [ ] inside it\n";

  const std::vector<Line> lines = parse(doc);
  CHECK(!lines[0].isTask);
  CHECK(lines[1].isTask && !lines[1].checked);
  CHECK(lines[2].isTask && lines[2].checked);
  CHECK(lines[3].isTask && lines[3].checked);  // capital X
  CHECK(lines[4].isTask && !lines[4].checked);
  CHECK(lines[5].isTask);
  CHECK(lines[6].isTask);
  CHECK(!lines[7].isTask);  // "- [] "
  CHECK(!lines[8].isTask);  // "-[ ] "
  CHECK(lines[9].isTask);   // a box and nothing else is an empty task
  CHECK(textOf(doc, lines[9]).empty());
  CHECK(!lines[10].isTask);  // "- [x]tra"
  CHECK(!lines[11].isTask);  // a box mid-sentence is not a task

  CHECK(textOf(doc, lines[1]) == "Milk");
  CHECK(textOf(doc, lines[4]) == "Indented is still a task");

  // Every non-empty line counts, marker or not: the deck's tally is "how much
  // of this list is done", and a line somebody typed on their phone is part of
  // the list whether or not it arrived with a marker on it.
  const Counts c = counts(lines);
  CHECK(c.total == 12);
  CHECK(c.done == 2);
  // Blank lines are not items, and neither is the trailing empty line a file
  // ending in a newline produces.
  CHECK(counts(parse("a\n\nb\n")).total == 2);
  CHECK(counts(parse("")).total == 0);
}

void testATickIsOneByte() {
  const std::string original =
      "# Packing\n"
      "\n"
      "Ferry leaves at 07:40 -- do not forget the tickets [in the drawer].\n"
      "- [ ] Passport\n"
      "- [x] Charger\n"
      "\tTabbed prose with trailing spaces   \n"
      "- [ ] Toothbrush\n";

  std::string doc = original;
  std::vector<Line> lines = parse(doc);

  for (Line& line : lines) {
    if (!line.isTask) continue;
    const std::string before = doc;
    const bool wasChecked = line.checked;
    CHECK(toggle(doc, line));
    CHECK(line.checked != wasChecked);
    CHECK(differingBytes(before, doc) == 1);
  }

  // Ticking everything twice must return the file to exactly what it was,
  // including the tab, the double spaces and the square brackets in the prose.
  for (Line& line : lines) {
    if (line.isTask) toggle(doc, line);
  }
  CHECK(doc == original);

  // Prose cannot be ticked, and a failed toggle changes nothing.
  std::string untouched = doc;
  Line prose = parse(doc)[2];
  CHECK(!toggle(doc, prose));
  CHECK(doc == untouched);
}

void testCoercing() {
  // What a person types on their phone, with no syntax at all.
  std::string plain = "Milk\nEggs\nBread flour\n";
  CHECK(coerceToList(plain));
  CHECK(plain == "- [ ] Milk\n- [ ] Eggs\n- [ ] Bread flour\n");
  // And every one of them is now tickable, which is the whole point.
  CHECK(counts(parse(plain)).total == 3);

  // A file that is already a list is left BYTE FOR BYTE alone, ticks included:
  // a save from the phone must not disturb what was ticked on the device.
  std::string already = "- [x] Milk\n- [ ] Eggs\n";
  CHECK(!coerceToList(already));
  CHECK(already == "- [x] Milk\n- [ ] Eggs\n");

  // Mixed, which is what an edit of an existing list looks like.
  std::string mixed = "- [x] Milk\nLemons\n";
  CHECK(coerceToList(mixed));
  CHECK(mixed == "- [x] Milk\n- [ ] Lemons\n");

  // A blank line stays blank. Somebody who pressed return twice meant a gap,
  // not a thing to do, and an empty tick box is a hole in the list.
  std::string gap = "Milk\n\nEggs\n";
  CHECK(coerceToList(gap));
  CHECK(gap == "- [ ] Milk\n\n- [ ] Eggs\n");

  // CRLF survives: the phone is not the only thing that writes this file.
  std::string crlf = "Milk\r\nEggs\r\n";
  CHECK(coerceToList(crlf));
  CHECK(crlf == "- [ ] Milk\r\n- [ ] Eggs\r\n");

  // The near-misses the parser rejects are coerced rather than left as a second
  // kind of line. "- [] milk" is not a marker, so it becomes one.
  std::string nearly = "- [] milk\n- [x]tra\n";
  CHECK(coerceToList(nearly));
  CHECK(counts(parse(nearly)).total == 2);

  std::string empty;
  CHECK(!coerceToList(empty));
  CHECK(empty.empty());
}

void testKind() {
  // One tick box anywhere makes the whole file a list; the kind is never a
  // property of the line.
  CHECK(kindOf(parse("- [ ] Milk\nEggs\n")) == Kind::List);
  CHECK(kindOf(parse("Flour 500g\nWater 375g\n")) == Kind::Page);
  CHECK(kindOf(parse("Notes about the flat\n- [x] Ask about the boiler\n")) == Kind::List);

  // An empty note has no evidence, so the caller's choice at creation decides,
  // and the first line written settles it for good.
  CHECK(kindOf(parse(""), true) == Kind::List);
  CHECK(kindOf(parse(""), false) == Kind::Page);
  CHECK(kindOf(parse("\n\n"), false) == Kind::Page);

  // The tally only belongs on a list, so the count of MARKED lines is what the
  // deck asks for -- not the count of lines, which a page also has.
  const Counts page = counts(parse("Flour\nWater\n"));
  CHECK(page.total == 2);
  CHECK(page.marked == 0);
  const Counts list = counts(parse("- [x] Milk\n- [ ] Eggs\n"));
  CHECK(list.marked == 2);
  CHECK(list.done == 1);
}

void testSwitchingKind() {
  // A note becomes a list and back, and the round trip keeps the words. This is
  // the escape hatch for a file whose kind was inferred wrong -- a shopping
  // list typed as plain lines on a computer opens as a note, and one tap fixes
  // it without anybody editing syntax.
  std::string doc = "Milk\nEggs\n";
  CHECK(coerceToList(doc));
  CHECK(kindOf(parse(doc)) == Kind::List);
  CHECK(stripMarkers(doc));
  CHECK(doc == "Milk\nEggs\n");
  CHECK(kindOf(parse(doc)) == Kind::Page);

  // Ticks are lost on the way to a note, which is the honest outcome: a note
  // has nothing to be done with.
  std::string ticked = "- [x] Milk\n- [ ] Eggs\n";
  CHECK(stripMarkers(ticked));
  CHECK(ticked == "Milk\nEggs\n");

  // Nothing to strip is not a change, and must not rewrite the file.
  std::string plain = "Milk\r\nEggs\r\n";
  CHECK(!stripMarkers(plain));
  CHECK(plain == "Milk\r\nEggs\r\n");
}

void testClearing() {
  std::string doc =
      "Shopping\n"
      "- [x] Milk\n"
      "- [ ] Bread flour\n"
      "- [x] Eggs\n"
      "Remember the deposit\n";

  const std::vector<std::string> removed = clearChecked(doc);
  CHECK(removed.size() == 2);
  CHECK(removed[0] == "Milk");
  CHECK(removed[1] == "Eggs");
  CHECK(doc ==
        "Shopping\n"
        "- [ ] Bread flour\n"
        "Remember the deposit\n");

  // Prose is never removed, whatever it says.
  std::string prose = "- [x] done\nnot a task\n";
  clearChecked(prose);
  CHECK(prose == "not a task\n");

  // Clearing a note with nothing ticked is a no-op, not a reformat.
  std::string same = "- [ ] a\r\nplain\r\n";
  const std::vector<std::string> none = clearChecked(same);
  CHECK(none.empty());
  CHECK(same == "- [ ] a\r\nplain\r\n");

  // CRLF survives a clear. Files arrive from desktop editors.
  std::string crlf = "- [x] gone\r\n- [ ] stays\r\n";
  clearChecked(crlf);
  CHECK(crlf == "- [ ] stays\r\n");
}

// The rows a note draws, shared by the open note and the sleep screen. A blank
// line and a marker with nothing after it are spacing, not items.
void testDrawnLines() {
  const std::string doc = "- [ ] milk\n\n- [x] \n   \n- [x] eggs\nbread\n";
  const std::vector<Line> lines = parse(doc);
  const std::vector<size_t> drawn = drawnLines(doc, lines);
  CHECK(drawn.size() == 3);
  if (drawn.size() == 3) {
    CHECK(textOf(doc, lines[drawn[0]]) == "milk");
    CHECK(textOf(doc, lines[drawn[1]]) == "eggs");
    CHECK(lines[drawn[1]].checked);
    CHECK(textOf(doc, lines[drawn[2]]) == "bread");
  }
  CHECK(drawnLines("", parse("")).empty());
}

// The sleep screen's choice: the note's name and the mode it replaced, so
// turning it off puts back the person's own screen.
void testAsleepChoice() {
  AsleepChoice in;
  in.name = "Groceries";
  in.previousMode = 2;
  AsleepChoice out;
  CHECK(parseAsleep(formatAsleep(in), out));
  CHECK(out.name == "Groceries");
  CHECK(out.previousMode == 2);

  // Nothing recorded to put back survives the round trip as "nothing".
  in.previousMode = -1;
  CHECK(parseAsleep(formatAsleep(in), out));
  CHECK(out.previousMode == -1);

  // A name alone, a Windows line ending, and trailing spaces all read.
  CHECK(parseAsleep("Packing list\r\n", out) && out.name == "Packing list" && out.previousMode == -1);
  CHECK(parseAsleep("Packing list", out) && out.name == "Packing list");

  // No name is no choice, whatever follows; a mode that is not a mode is
  // forgotten rather than trusted.
  CHECK(!parseAsleep("", out));
  CHECK(!parseAsleep("\n3\n", out));
  CHECK(parseAsleep("Todo\n9999\n", out) && out.name == "Todo" && out.previousMode == -1);
  CHECK(parseAsleep("Todo\nabc\n", out) && out.previousMode == -1);

  // Quick Resume on Timeout, which putting a note up turns off: recorded so
  // taking it down turns it back on. Unknown is -1, "leave it alone".
  in.name = "Groceries";
  in.previousMode = 2;
  in.previousQuickResume = 1;
  CHECK(parseAsleep(formatAsleep(in), out));
  CHECK(out.previousMode == 2 && out.previousQuickResume == 1);
  in.previousQuickResume = 0;
  CHECK(parseAsleep(formatAsleep(in), out) && out.previousQuickResume == 0);
  // A file written before the field existed, or a value that is not 0 or 1.
  CHECK(parseAsleep("Todo\n3\n", out) && out.previousMode == 3 && out.previousQuickResume == -1);
  CHECK(parseAsleep("Todo\n3\n7\n", out) && out.previousQuickResume == -1);
  CHECK(parseAsleep("Todo\r\n3\r\n1\r\n", out) && out.previousMode == 3 && out.previousQuickResume == 1);
  // A bad mode does not lose a good quick-resume value beside it.
  CHECK(parseAsleep("Todo\nabc\n1\n", out) && out.previousMode == -1 && out.previousQuickResume == 1);
}

}  // namespace

namespace md = notes::md;

std::string spansText(const std::vector<md::Span>& spans) {
  std::string out;
  for (const md::Span& span : spans)
    out += span.isFraction ? "[" + span.text + "|" + span.denominator + "]" : span.text;
  return out;
}

void testInlineMarkdown() {
  auto spans = md::parseInline("a **b** *c* `d`");
  CHECK(spans.size() == 6);
  CHECK(spans[1].text == "b" && spans[1].style == md::kBold);
  CHECK(spans[3].text == "c" && spans[3].style == md::kItalic);
  CHECK(spans[5].text == "d" && spans[5].style == md::kCode);

  // Nothing unmatched disappears.
  CHECK(spansText(md::parseInline("2 * 3 * 4")) == "2 * 3 * 4");
  CHECK(spansText(md::parseInline("**open")) == "**open");
  CHECK(spansText(md::parseInline("`open")) == "`open");
  CHECK(spansText(md::parseInline("costs $5 today")) == "costs $5 today");
  CHECK(md::parseInline("snake_case_name").size() == 1);
  CHECK(spansText(md::parseInline("\\*not\\*")) == "*not*");
  spans = md::parseInline("***x***");
  CHECK(spansText(spans).find('x') != std::string::npos);
  CHECK(md::parseInline("`**x**`")[0].style == md::kCode);
  CHECK(md::parseInline("**a *b* c**")[1].style == (md::kBold | md::kItalic));
}

void testMath() {
  CHECK(spansText(md::parseMath("\\alpha + \\beta")) == "α + β");
  CHECK(spansText(md::parseMath("a \\leq b")) == "a ≤ b");

  auto spans = md::parseMath("x^2");
  CHECK(spans.size() == 2);
  CHECK(spans[1].text == "2" && (spans[1].style & md::kSuper) != 0);
  spans = md::parseMath("a_{ij}");
  CHECK(spans[1].text == "ij" && (spans[1].style & md::kSub) != 0);
  spans = md::parseMath("x^{\\alpha}");
  CHECK(spans[1].text == "α" && (spans[1].style & md::kSuper) != 0);

  spans = md::parseMath("\\frac{a+b}{\\pi}");
  CHECK(spans.size() == 1 && spans[0].isFraction);
  CHECK(spans[0].text == "a+b" && spans[0].denominator == "π");
  spans = md::parseMath("\\frac{\\pi^2}{x_1}");
  CHECK(spans[0].text == "π²" && spans[0].denominator == "x₁");

  CHECK(spansText(md::parseMath("\\sqrt{x+1}")) == "√(x+1)");
  CHECK(spansText(md::parseMath("\\sqrt{x}")) == "√x");
  CHECK(spansText(md::parseMath("\\left(x\\right)")) == "(x)");
  CHECK(spansText(md::parseMath("\\text{if } x")) == "if  x");
  CHECK(spansText(md::parseMath("\\nosuchcommand")) == "\\nosuchcommand");
  CHECK(spansText(md::parseMath("\\frac{a")) == "[a|]");
  CHECK(spansText(md::parseMath("x^")) == "x");
  CHECK(spansText(md::parseMath("\\")) == "\\");
  CHECK(spansText(md::parseInline("is $x^2$ ok")) == "is x2 ok");
}

void testBlockMarkdown() {
  const std::vector<std::string> lines = {"# Title", "",        "para", "- item",  "3. third", "> quote", "---",
                                          "```",     "**raw**", "```",  "$$x^2$$", "$$",       "\\alpha", "$$"};
  const auto blocks = md::parse(lines);
  CHECK(blocks.size() == 9);
  CHECK(blocks[0].kind == md::BlockKind::Heading && blocks[0].level == 1 && spansText(blocks[0].spans) == "Title");
  CHECK(blocks[1].kind == md::BlockKind::Paragraph && blocks[1].line == 2);
  CHECK(blocks[2].kind == md::BlockKind::Bullet);
  CHECK(blocks[3].kind == md::BlockKind::Numbered && blocks[3].number == 3);
  CHECK(blocks[4].kind == md::BlockKind::Quote);
  CHECK(blocks[5].kind == md::BlockKind::Rule);
  CHECK(blocks[6].kind == md::BlockKind::Code && spansText(blocks[6].spans) == "**raw**");
  CHECK(blocks[7].kind == md::BlockKind::DisplayMath);
  CHECK(blocks[8].kind == md::BlockKind::DisplayMath && spansText(blocks[8].spans) == "α");

  CHECK(md::parse({"#nope"})[0].kind == md::BlockKind::Paragraph);
  CHECK(md::parse({"####### seven"})[0].kind == md::BlockKind::Paragraph);
  CHECK(md::parse({"--"})[0].kind == md::BlockKind::Paragraph);
  CHECK(md::parse({"1.no"})[0].kind == md::BlockKind::Paragraph);

  // An unclosed fence or $$ must not swallow the rest of the note.
  const auto open = md::parse({"```", "after"});
  CHECK(open.size() == 2 && open[1].kind == md::BlockKind::Paragraph);
  CHECK(md::parse({"$$ x", "after"}).size() == 2);
  CHECK(md::parse({}).empty());
}

int main() {
  testInlineMarkdown();
  testMath();
  testBlockMarkdown();
  testDrawnLines();
  testAsleepChoice();
  testWhatIsATask();
  testATickIsOneByte();
  testKind();
  testCoercing();
  testSwitchingKind();
  testClearing();

  std::printf("%s  notes: %d checks, %d failed\n", failures ? "FAIL" : "ok  ", checks, failures);
  return failures == 0 ? 0 : 1;
}
