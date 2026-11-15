#pragma once

// Chord charts: reading them, transposing them, and the small files the app
// keeps beside them.
//
// Reads the two forms people actually have: chords on the line above the
// lyrics, which is what copying a chart from a website gives, and ChordPro
// ("[G]words [C]more"). Anything not recognised is kept as text, never dropped.
//
// Freestanding C++17, so host-tests/songs can drive every rule.

#include <cstdint>
#include <string>
#include <vector>

namespace songs {

struct Chord {
  int root = 0;         // 0 is C, counting semitones
  std::string quality;  // everything between the root and any slash: "m7", "sus4"
  int bass = -1;        // the note after a slash, or -1
};

bool parseChord(const std::string& token, Chord& out);

// A whole line of chords, such as "G   D/F#  Em  C".
bool isChordLine(const std::string& line);

enum class LineKind : uint8_t { Pair, Text, Section, Blank };

// A chord and the lyric it sits over. Either may be empty.
struct Segment {
  std::string chord;
  std::string lyric;
};

struct SongLine {
  LineKind kind = LineKind::Blank;
  std::string text;               // Text and Section
  std::vector<Segment> segments;  // Pair
};

struct Song {
  std::string title;
  std::string artist;
  int capo = 0;     // the capo the chart was written for
  std::string key;  // as written in the chart, possibly empty
  std::vector<SongLine> lines;
};

// `chordPro` reads [C] inside lyrics and {directives}; otherwise chords are
// read off the line above the lyric.
Song parse(const std::string& text, bool chordPro);
bool looksLikeChordPro(const std::string& path, const std::string& text);

// How the chart is shown: `transpose` moves the music, `capo` is where the
// player's capo is. Shapes shown = written shape + chart capo + transpose - capo.
struct View {
  int transpose = 0;
  int capo = 0;
};

int shapeShift(const Song& song, const View& view);

// The chord moved by `shift` semitones, spelled with flats when `flats`.
// Unchanged, as written, when the shift is zero.
std::string transposed(const std::string& chord, int shift, bool flats);

// Whether chords shown at `shift` read better with flats: decided by the key
// the shapes land in, taken from the chart's key or else its first chord.
bool prefersFlats(const Song& song, int shift);

// "+2", "-1", "0".
std::string signedText(int value);

// Per-song view settings, one line per song: path, transpose, capo.
struct SavedView {
  std::string path;
  View view;
};
std::string formatViews(const std::vector<SavedView>& views);
std::vector<SavedView> parseViews(const std::string& text);

// A setlist file is one song path per line.
std::vector<std::string> parseSetlist(const std::string& text);
std::string formatSetlist(const std::vector<std::string>& paths);

}  // namespace songs
