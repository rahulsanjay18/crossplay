// Every song here is invented for these tests.

#include <cstdio>
#include <string>

#include "SongsCore.h"
#include "SongsLayout.h"

namespace {
int checks = 0;
int failures = 0;

void check(const bool ok, const char* what, const int line) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL line %d: %s\n", line, what);
  }
}
#define CHECK(c) check((c), #c, __LINE__)

std::string chordsOf(const songs::SongLine& line) {
  std::string out;
  for (const songs::Segment& segment : line.segments) out += "[" + segment.chord + "]" + segment.lyric;
  return out;
}

void testChords() {
  songs::Chord chord;
  for (const char* good :
       {"C", "Am", "F#m7", "Bbmaj7", "Dsus4", "G/B", "Cadd9", "E7b9", "Asus2/E", "Ddim", "Faug", "C(add9)"}) {
    CHECK(songs::parseChord(good, chord));
  }
  for (const char* bad : {"", "H", "The", "Am/", "Cx", "Go", "Do", "A/H", "Bridge"}) {
    CHECK(!songs::parseChord(bad, chord));
  }
  CHECK(songs::parseChord("Bb/F", chord) && chord.root == 10 && chord.bass == 5);

  CHECK(songs::isChordLine("G   D/F#   Em   C"));
  CHECK(songs::isChordLine("  Am | F | C | G  x2"));
  CHECK(songs::isChordLine("[ch]G[/ch]     [ch]C[/ch]"));
  CHECK(!songs::isChordLine("A lantern swinging low"));
  CHECK(!songs::isChordLine(""));
  CHECK(!songs::isChordLine("|  x2"));
}

void testPlainChart() {
  const std::string chart =
      "Capo: 2nd fret\n"
      "Key: G\n"
      "\n"
      "[Verse 1]\n"
      "G          D\n"
      "Lanterns on the water\n"
      "Em     C\n"
      "Morning wheels\n"
      "\n"
      "[Chorus]\n"
      "C   G\n";
  const songs::Song song = songs::parse(chart, false);
  CHECK(song.capo == 2);
  CHECK(song.key == "G");
  CHECK(song.lines.size() == 6);
  CHECK(song.lines[0].kind == songs::LineKind::Section && song.lines[0].text == "Verse 1");
  CHECK(chordsOf(song.lines[1]) == "[G]Lanterns on[D] the water");
  CHECK(chordsOf(song.lines[2]) == "[Em]Morning[C] wheels");
  CHECK(song.lines[3].kind == songs::LineKind::Blank);
  CHECK(song.lines[4].kind == songs::LineKind::Section && song.lines[4].text == "Chorus");
  CHECK(chordsOf(song.lines[5]) == "[C][G]");

  // A chord past the end of the lyric still appears.
  const songs::Song late = songs::parse("G            D\nShort\n", false);
  CHECK(chordsOf(late.lines[0]) == "[G]Short[D]");
  // Lyric before the first chord keeps no chord.
  const songs::Song lead = songs::parse("     G\nWell the river\n", false);
  CHECK(chordsOf(lead.lines[0]) == "[]Well [G]the river");
  // Columns count characters, not bytes.
  const songs::Song accents = songs::parse("    D\ncafé du soir\n", false);
  CHECK(chordsOf(accents.lines[0]) == "[]café[D] du soir");
  // Website markup is stripped.
  const songs::Song markup = songs::parse("[ch]Am[/ch]   [ch]F[/ch]\nHollow sky\n", false);
  // Columns count after the markup is removed, as the chart is displayed.
  CHECK(chordsOf(markup.lines[0]) == "[Am]Hollo[F]w sky");
  // A plain line stays text.
  CHECK(songs::parse("Hum softly here\n", false).lines[0].kind == songs::LineKind::Text);
  CHECK(songs::parse("Capo: none\nG\n", false).capo == 0);
}

void testChordPro() {
  const std::string chart =
      "{title: Paper Boats}\n"
      "{artist: The Inventions}\n"
      "{capo: 3}\n"
      "# a comment\n"
      "{soc}\n"
      "[G]Paper boats on [C/E]rainy streets\n"
      "No chords here\n"
      "[Bridge] is a word, [D]not a chord\n";
  CHECK(songs::looksLikeChordPro("x.txt", chart));
  CHECK(songs::looksLikeChordPro("song.cho", "anything"));
  CHECK(!songs::looksLikeChordPro("song.txt", "G\nwords\n"));
  const songs::Song song = songs::parse(chart, true);
  CHECK(song.title == "Paper Boats" && song.artist == "The Inventions" && song.capo == 3);
  CHECK(song.lines.size() == 4);
  CHECK(song.lines[0].kind == songs::LineKind::Section && song.lines[0].text == "Chorus");
  CHECK(chordsOf(song.lines[1]) == "[G]Paper boats on [C/E]rainy streets");
  CHECK(song.lines[2].kind == songs::LineKind::Text);
  CHECK(chordsOf(song.lines[3]) == "[][Bridge] is a word, [D]not a chord");
}

void testTranspose() {
  CHECK(songs::transposed("G", 0, true) == "G");
  CHECK(songs::transposed("A#m", 0, true) == "A#m");
  CHECK(songs::transposed("G", 2, false) == "A");
  CHECK(songs::transposed("A", 1, true) == "Bb");
  CHECK(songs::transposed("A", 1, false) == "A#");
  CHECK(songs::transposed("D/F#", -2, false) == "C/E");
  CHECK(songs::transposed("Bbmaj7", 2, false) == "Cmaj7");
  CHECK(songs::transposed("C", -1, false) == "B");
  CHECK(songs::transposed("C", 12, false) == "C");
  CHECK(songs::transposed("N.C.", 3, false) == "N.C.");

  songs::Song song = songs::parse("Capo: 2\nG  D\nLong road home\n", false);
  songs::View view;
  view.capo = song.capo;
  CHECK(songs::shapeShift(song, view) == 0);  // opens as written
  view.capo = 0;
  CHECK(songs::shapeShift(song, view) == 2);  // capo off: shapes move up to sound the same
  view.transpose = -2;
  CHECK(songs::shapeShift(song, view) == 0);

  // G moved to F reads in flats; G moved to A reads in sharps.
  CHECK(songs::prefersFlats(song, -2));
  CHECK(!songs::prefersFlats(song, 2));
  const songs::Song minor = songs::parse("Key: Am\nAm\nGrey\n", false);
  CHECK(songs::prefersFlats(minor, -2));  // G minor
  CHECK(!songs::prefersFlats(minor, 2));  // B minor
  CHECK(songs::signedText(2) == "+2" && songs::signedText(-1) == "-1" && songs::signedText(0) == "0");
}

void testFiles() {
  std::vector<songs::SavedView> views = {{"/songs/a.txt", {2, 0}}, {"/songs/b.cho", {-1, 3}}};
  const auto read = songs::parseViews(songs::formatViews(views));
  CHECK(read.size() == 2 && read[1].path == "/songs/b.cho" && read[1].view.transpose == -1 && read[1].view.capo == 3);
  CHECK(songs::parseViews("bad line\n/x\t1\t99\n").empty());

  const std::vector<std::string> list = {"/songs/a.txt", "/songs/b.cho"};
  CHECK(songs::parseSetlist(songs::formatSetlist(list)) == list);
  CHECK(songs::parseSetlist("# note\n\n  /songs/c.txt  \n").size() == 1);
}

// Ten pixels a character, chords and lyrics alike.
int fixedWidth(void*, bool, const std::string& text) {
  int n = 0;
  for (const char c : text) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
  return n * 10;
}

void testLayout() {
  songs::Measure measure;
  measure.width = &fixedWidth;
  measure.chordHeight = 20;
  measure.lyricHeight = 30;

  const songs::Song song = songs::parse("[Verse]\nG       D\nLanterns on the water\n\nAm\n", false);
  songs::View view;
  auto rows = songs::layout(song, view, 1000, measure);
  CHECK(rows.size() == 4);
  CHECK(rows[0].kind == songs::RowKind::Section);
  CHECK(rows[1].hasChords && rows[1].height == 50);
  CHECK(rows[1].items.size() >= 4 && rows[1].items[0].text == "G" && rows[1].items[2].text == "D");
  CHECK(rows[1].items[2].x == rows[1].items[3].x);   // D sits over its lyric
  CHECK(rows[3].hasChords && rows[3].height == 20);  // chords alone take no lyric line

  // Narrow enough to wrap: every chord stays at the x of the lyric it sits over,
  // and the lyric reads back unchanged across rows.
  const songs::Song wide = songs::parse("G     C        D        Em\nOne two three four five six seven eight\n", false);
  rows = songs::layout(wide, view, 150, measure);
  CHECK(rows.size() > 1);
  std::string lyric;
  for (const songs::Row& row : rows) {
    for (size_t i = 0; i < row.items.size(); i++) {
      const songs::Placed& item = row.items[i];
      CHECK(item.x + fixedWidth(nullptr, item.chord, item.text) <= 150 || item.x == 0);
      if (item.chord) {
        CHECK(i + 1 < row.items.size() && !row.items[i + 1].chord && row.items[i + 1].x == item.x);
      } else {
        lyric +=
            (lyric.empty() || item.text[0] == ' ' || item.x == 0 ? (item.x == 0 && !lyric.empty() ? " " : "") : "") +
            item.text;
      }
    }
  }
  CHECK(lyric == "One two three four five six seven eight");

  // Transposed chords are what get laid out.
  view.transpose = 2;
  rows = songs::layout(song, view, 1000, measure);
  CHECK(rows[1].items[0].text == "A" && rows[1].items[2].text == "E");

  // Pages: a Section is not left alone at the foot of a page.
  std::vector<songs::Row> page(6);
  for (auto& row : page) row.height = 30;
  page[2].kind = songs::RowKind::Section;
  const auto starts = songs::pageStarts(page, 95);
  CHECK(starts.size() == 3 && starts[1] == 2 && starts[2] == 5);
}
}  // namespace

int main() {
  testChords();
  testPlainChart();
  testChordPro();
  testTranspose();
  testFiles();
  testLayout();
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
