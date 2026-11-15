#include "SongsCore.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace songs {
namespace {

constexpr const char* kSharpNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
constexpr const char* kFlatNames[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

int letterValue(const char letter) {
  switch (letter) {
    case 'C':
      return 0;
    case 'D':
      return 2;
    case 'E':
      return 4;
    case 'F':
      return 5;
    case 'G':
      return 7;
    case 'A':
      return 9;
    case 'B':
      return 11;
    default:
      return -1;
  }
}

int wrap(const int semitones) { return ((semitones % 12) + 12) % 12; }

// A note at `at`: a letter, then any one of # or b. Returns its length, or 0.
size_t readNote(const std::string& text, const size_t at, int& value) {
  if (at >= text.size()) return 0;
  const int base = letterValue(text[at]);
  if (base < 0) return 0;
  if (at + 1 < text.size() && (text[at + 1] == '#' || text[at + 1] == 'b')) {
    value = wrap(base + (text[at + 1] == '#' ? 1 : -1));
    return 2;
  }
  value = base;
  return 1;
}

// What may follow a root: m, maj7, sus4, add9, dim, aug, 7b9, (add11), +, -.
bool isQuality(const std::string& quality) {
  static constexpr const char* kWords[] = {"maj", "min", "dim", "aug", "sus", "add", "M", "m"};
  size_t at = 0;
  while (at < quality.size()) {
    const char c = quality[at];
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '#' || c == 'b' || c == '+' || c == '-' || c == '(' ||
        c == ')' || c == '^') {
      at++;
      continue;
    }
    bool matched = false;
    for (const char* word : kWords) {
      const size_t length = std::strlen(word);
      if (quality.compare(at, length, word) == 0) {
        at += length;
        matched = true;
        break;
      }
    }
    if (!matched) return false;
  }
  return true;
}

std::string trimmed(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) begin++;
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) end--;
  return text.substr(begin, end - begin);
}

std::string lower(std::string text) {
  for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

// Tokens a chord line may carry that are not chords.
bool isChordLineFiller(const std::string& token) {
  const std::string t = lower(token);
  return t == "|" || t == "||" || t == "-" || t == "/" || t == "n.c." || t == "nc" || t == "x2" || t == "x3" ||
         t == "x4" || t == "(x2)" || t == "(x3)" || t == "(x4)" || t == "%";
}

std::vector<std::string> rawLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t begin = 0;
  while (begin < text.size()) {
    size_t end = text.find('\n', begin);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(begin, end - begin);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
    begin = end + 1;
  }
  return lines;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t begin = 0;
  while (begin <= text.size()) {
    size_t end = text.find('\n', begin);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(begin, end - begin);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // Tabs become spaces so that columns line up the way they were typed.
    std::string expanded;
    for (const char c : line) {
      if (c == '\t') {
        do expanded += ' ';
        while (expanded.size() % 4 != 0);
      } else {
        expanded += c;
      }
    }
    lines.push_back(expanded);
    if (end == text.size()) break;
    begin = end + 1;
  }
  return lines;
}

// Website markup copied with the text: [ch]G[/ch] becomes G, and [tab]
// wrappers go.
std::string stripMarkup(std::string line) {
  for (const char* tag : {"[ch]", "[/ch]", "[tab]", "[/tab]"}) {
    size_t at;
    while ((at = line.find(tag)) != std::string::npos) line.erase(at, std::strlen(tag));
  }
  return line;
}

// The column each code point starts at, so a chord over a multi-byte lyric
// still lands on the right letter.
size_t byteForColumn(const std::string& text, const size_t column) {
  size_t seen = 0;
  for (size_t i = 0; i < text.size(); i++) {
    if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue;
    if (seen == column) return i;
    seen++;
  }
  return text.size();
}

SongLine pairFrom(const std::string& chords, const std::string& lyric) {
  SongLine line;
  line.kind = LineKind::Pair;
  std::vector<std::pair<size_t, std::string>> placed;
  size_t at = 0;
  while (at < chords.size()) {
    while (at < chords.size() && chords[at] == ' ') at++;
    const size_t begin = at;
    while (at < chords.size() && chords[at] != ' ') at++;
    if (at > begin) placed.emplace_back(begin, chords.substr(begin, at - begin));
  }
  if (placed.empty()) {
    line.segments.push_back(Segment{"", lyric});
    return line;
  }
  if (placed.front().first > 0) {
    line.segments.push_back(Segment{"", lyric.substr(0, byteForColumn(lyric, placed.front().first))});
  }
  for (size_t i = 0; i < placed.size(); i++) {
    const size_t from = byteForColumn(lyric, placed[i].first);
    const size_t to = i + 1 < placed.size() ? byteForColumn(lyric, placed[i + 1].first) : lyric.size();
    line.segments.push_back(Segment{placed[i].second, from < to ? lyric.substr(from, to - from) : ""});
  }
  return line;
}

// "Capo: 2nd fret", "Capo 3", "capo: 1".
bool readCapo(const std::string& line, int& capo) {
  const std::string t = lower(trimmed(line));
  if (t.compare(0, 4, "capo") != 0) return false;
  size_t at = 4;
  while (at < t.size() && (t[at] == ':' || t[at] == ' ')) at++;
  if (at >= t.size() || !std::isdigit(static_cast<unsigned char>(t[at]))) {
    if (t.find("no capo") != std::string::npos || t.compare(at, 4, "none") == 0) {
      capo = 0;
      return true;
    }
    return false;
  }
  capo = std::atoi(t.c_str() + at);
  return capo >= 0 && capo <= 12;
}

bool readKey(const std::string& line, std::string& key) {
  const std::string t = trimmed(line);
  if (lower(t).compare(0, 4, "key:") != 0) return false;
  key = trimmed(t.substr(4));
  return !key.empty();
}

bool isSectionLabel(const std::string& line) {
  const std::string t = trimmed(line);
  if (t.size() < 3 || t.front() != '[' || t.back() != ']') return false;
  Chord chord;
  return !parseChord(t.substr(1, t.size() - 2), chord);
}

SongLine textLine(const std::string& text) {
  SongLine line;
  line.kind = trimmed(text).empty() ? LineKind::Blank : LineKind::Text;
  line.text = text;
  return line;
}

SongLine section(const std::string& label) {
  SongLine line;
  line.kind = LineKind::Section;
  line.text = label;
  return line;
}

void parsePlain(Song& song, const std::vector<std::string>& lines) {
  for (size_t i = 0; i < lines.size(); i++) {
    const std::string line = stripMarkup(lines[i]);
    int capo = 0;
    std::string key;
    if (readCapo(line, capo)) {
      song.capo = capo;
      continue;
    }
    if (readKey(line, key)) {
      song.key = key;
      continue;
    }
    if (isSectionLabel(line)) {
      const std::string t = trimmed(line);
      song.lines.push_back(section(t.substr(1, t.size() - 2)));
      continue;
    }
    if (isChordLine(line)) {
      const std::string next = i + 1 < lines.size() ? stripMarkup(lines[i + 1]) : "";
      if (!trimmed(next).empty() && !isChordLine(next) && !isSectionLabel(next)) {
        song.lines.push_back(pairFrom(line, next));
        i++;
      } else {
        song.lines.push_back(pairFrom(line, ""));
      }
      continue;
    }
    song.lines.push_back(textLine(line));
  }
}

void parseChordPro(Song& song, const std::vector<std::string>& lines) {
  for (const std::string& raw : lines) {
    const std::string line = trimmed(raw);
    if (!line.empty() && line[0] == '#') continue;
    if (line.size() >= 2 && line.front() == '{' && line.back() == '}') {
      const std::string body = line.substr(1, line.size() - 2);
      const size_t colon = body.find(':');
      const std::string name = lower(trimmed(body.substr(0, colon)));
      const std::string value = colon == std::string::npos ? "" : trimmed(body.substr(colon + 1));
      if (name == "title" || name == "t") {
        song.title = value;
      } else if (name == "artist" || name == "subtitle" || name == "st") {
        song.artist = value;
      } else if (name == "capo") {
        song.capo = std::atoi(value.c_str());
      } else if (name == "key") {
        song.key = value;
      } else if (name == "comment" || name == "c" || name == "ci" || name == "comment_italic") {
        song.lines.push_back(section(value));
      } else if (name == "start_of_chorus" || name == "soc") {
        song.lines.push_back(section(value.empty() ? "Chorus" : value));
      } else if (name == "start_of_verse" || name == "sov") {
        song.lines.push_back(section(value.empty() ? "Verse" : value));
      } else if (name == "start_of_bridge" || name == "sob") {
        song.lines.push_back(section(value.empty() ? "Bridge" : value));
      }
      continue;
    }
    if (line.find('[') == std::string::npos) {
      song.lines.push_back(textLine(raw));
      continue;
    }
    SongLine pair;
    pair.kind = LineKind::Pair;
    size_t at = 0;
    std::string chord;
    std::string lyric;
    bool any = false;
    while (at < raw.size()) {
      if (raw[at] == '[') {
        const size_t close = raw.find(']', at);
        Chord parsed;
        if (close != std::string::npos && parseChord(raw.substr(at + 1, close - at - 1), parsed)) {
          if (any || !lyric.empty()) pair.segments.push_back(Segment{chord, lyric});
          chord = raw.substr(at + 1, close - at - 1);
          lyric.clear();
          any = true;
          at = close + 1;
          continue;
        }
      }
      lyric += raw[at];
      at++;
    }
    pair.segments.push_back(Segment{chord, lyric});
    song.lines.push_back(any ? pair : textLine(raw));
  }
}

}  // namespace

bool parseChord(const std::string& token, Chord& out) {
  int root = 0;
  const size_t rootLength = readNote(token, 0, root);
  if (rootLength == 0) return false;
  const size_t slash = token.find('/', rootLength);
  const std::string quality =
      token.substr(rootLength, slash == std::string::npos ? std::string::npos : slash - rootLength);
  if (!isQuality(quality)) return false;
  int bass = -1;
  if (slash != std::string::npos) {
    int value = 0;
    const size_t bassLength = readNote(token, slash + 1, value);
    if (bassLength == 0 || slash + 1 + bassLength != token.size()) return false;
    bass = value;
  }
  out.root = root;
  out.quality = quality;
  out.bass = bass;
  return true;
}

bool isChordLine(const std::string& raw) {
  const std::string line = stripMarkup(raw);
  int chords = 0;
  size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && line[at] == ' ') at++;
    const size_t begin = at;
    while (at < line.size() && line[at] != ' ') at++;
    if (at == begin) break;
    const std::string token = line.substr(begin, at - begin);
    Chord chord;
    if (parseChord(token, chord)) {
      chords++;
    } else if (!isChordLineFiller(token)) {
      return false;
    }
  }
  return chords > 0;
}

bool looksLikeChordPro(const std::string& path, const std::string& text) {
  const std::string name = lower(path);
  for (const char* extension : {".cho", ".pro", ".chopro", ".chordpro", ".crd"}) {
    const size_t length = std::strlen(extension);
    if (name.size() >= length && name.compare(name.size() - length, length, extension) == 0) return true;
  }
  for (const std::string& line : splitLines(text)) {
    const std::string t = trimmed(line);
    if (t.size() > 2 && t.front() == '{' && t.back() == '}' && t.find(':') != std::string::npos) return true;
  }
  return false;
}

Song parse(const std::string& text, const bool chordPro) {
  Song song;
  const std::vector<std::string> lines = splitLines(text);
  if (chordPro) {
    parseChordPro(song, lines);
  } else {
    parsePlain(song, lines);
  }
  // Leading and trailing blank lines carry nothing.
  while (!song.lines.empty() && song.lines.back().kind == LineKind::Blank) song.lines.pop_back();
  size_t first = 0;
  while (first < song.lines.size() && song.lines[first].kind == LineKind::Blank) first++;
  song.lines.erase(song.lines.begin(), song.lines.begin() + static_cast<long>(first));
  return song;
}

int shapeShift(const Song& song, const View& view) { return song.capo + view.transpose - view.capo; }

std::string transposed(const std::string& text, const int shift, const bool flats) {
  if (wrap(shift) == 0) return text;
  Chord chord;
  if (!parseChord(text, chord)) return text;
  const char* const* names = flats ? kFlatNames : kSharpNames;
  std::string out = names[wrap(chord.root + shift)];
  out += chord.quality;
  if (chord.bass >= 0) {
    out += "/";
    out += names[wrap(chord.bass + shift)];
  }
  return out;
}

bool prefersFlats(const Song& song, const int shift) {
  Chord key;
  bool found = parseChord(song.key, key);
  for (size_t i = 0; !found && i < song.lines.size(); i++) {
    for (const Segment& segment : song.lines[i].segments) {
      if (!segment.chord.empty() && parseChord(segment.chord, key)) {
        found = true;
        break;
      }
    }
  }
  if (!found) return false;
  const bool minor = key.quality.compare(0, 1, "m") == 0 && key.quality.compare(0, 3, "maj") != 0;
  const int root = wrap(key.root + shift);
  // Major keys written with flats: F Bb Eb Ab Db Gb. Their relative minors: D G C F Bb Eb.
  static constexpr bool kFlatMajor[12] = {false, true, false, true, false, true, true, false, true, false, true, false};
  static constexpr bool kFlatMinor[12] = {true, false, true, true, false, true, false, true, false, false, true, false};
  return minor ? kFlatMinor[root] : kFlatMajor[root];
}

std::string signedText(const int value) {
  if (value > 0) return "+" + std::to_string(value);
  return std::to_string(value);
}

std::string formatViews(const std::vector<SavedView>& views) {
  std::string out;
  for (const SavedView& saved : views) {
    out += saved.path + "\t" + std::to_string(saved.view.transpose) + "\t" + std::to_string(saved.view.capo) + "\n";
  }
  return out;
}

std::vector<SavedView> parseViews(const std::string& text) {
  std::vector<SavedView> views;
  for (const std::string& line : rawLines(text)) {
    const size_t first = line.find('\t');
    const size_t second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (second == std::string::npos) continue;
    SavedView saved;
    saved.path = line.substr(0, first);
    saved.view.transpose = std::atoi(line.c_str() + first + 1);
    saved.view.capo = std::atoi(line.c_str() + second + 1);
    if (saved.path.empty() || saved.view.capo < 0 || saved.view.capo > 12) continue;
    views.push_back(saved);
  }
  return views;
}

std::vector<std::string> parseSetlist(const std::string& text) {
  std::vector<std::string> paths;
  for (const std::string& line : splitLines(text)) {
    const std::string path = trimmed(line);
    if (!path.empty() && path[0] != '#') paths.push_back(path);
  }
  return paths;
}

std::string formatSetlist(const std::vector<std::string>& paths) {
  std::string out;
  for (const std::string& path : paths) out += path + "\n";
  return out;
}

}  // namespace songs
