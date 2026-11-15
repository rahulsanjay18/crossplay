#include "SongsLayout.h"

namespace songs {
namespace {

int width(const Measure& measure, const bool chord, const std::string& text) {
  return text.empty() ? 0 : measure.width(measure.context, chord, text);
}

// Words of `text`, each keeping the space before it, so pieces join back exactly.
std::vector<std::string> words(const std::string& text) {
  std::vector<std::string> out;
  size_t begin = 0;
  while (begin < text.size()) {
    size_t end = text.find(' ', begin + 1);
    while (end != std::string::npos && end + 1 < text.size() && text[end + 1] == ' ') end++;
    if (end == std::string::npos) end = text.size();
    out.push_back(text.substr(begin, end - begin));
    begin = end;
  }
  return out;
}

Row newRow(const RowKind kind) {
  Row row;
  row.kind = kind;
  return row;
}

void finishPair(std::vector<Row>& rows, Row& row, const Measure& measure) {
  if (row.items.empty()) return;
  bool lyrics = false;
  for (const Placed& item : row.items) {
    if (item.chord) row.hasChords = true;
    if (!item.chord && !item.text.empty()) lyrics = true;
  }
  row.height = static_cast<int16_t>((row.hasChords ? measure.chordHeight : 0) + (lyrics ? measure.lyricHeight : 0));
  if (row.height == 0) row.height = measure.lyricHeight;
  rows.push_back(std::move(row));
  row = newRow(RowKind::Pair);
}

void layoutPair(std::vector<Row>& rows, const SongLine& line, const int shift, const bool flats, const int limit,
                const Measure& measure) {
  const int gap = width(measure, true, " ");
  Row row = newRow(RowKind::Pair);
  int x = 0;
  for (const Segment& segment : line.segments) {
    const std::string chord = segment.chord.empty() ? "" : transposed(segment.chord, shift, flats);
    const int chordWidth = chord.empty() ? 0 : width(measure, true, chord) + gap;
    // The chord travels with the first word of its lyric; the rest may wrap on.
    std::vector<std::string> pieces = words(segment.lyric);
    if (pieces.empty()) pieces.emplace_back();
    for (size_t p = 0; p < pieces.size(); p++) {
      std::string piece = pieces[p];
      const bool carriesChord = p == 0 && !chord.empty();
      if (x == 0 && !piece.empty() && piece[0] == ' ') piece.erase(0, piece.find_first_not_of(' '));
      const int lyricWidth = width(measure, false, piece);
      const int pieceWidth = carriesChord && chordWidth > lyricWidth ? chordWidth : lyricWidth;
      if (x > 0 && x + pieceWidth > limit) {
        finishPair(rows, row, measure);
        x = 0;
        if (!piece.empty() && piece[0] == ' ') piece.erase(0, piece.find_first_not_of(' '));
      }
      if (carriesChord) row.items.push_back(Placed{chord, static_cast<int16_t>(x), true});
      if (!piece.empty()) row.items.push_back(Placed{piece, static_cast<int16_t>(x), false});
      const int placed = carriesChord
                             ? (chordWidth > width(measure, false, piece) ? chordWidth : width(measure, false, piece))
                             : width(measure, false, piece);
      x += placed;
    }
  }
  finishPair(rows, row, measure);
}

void layoutText(std::vector<Row>& rows, const std::string& text, const RowKind kind, const int limit,
                const Measure& measure) {
  Row row = newRow(kind);
  row.height = measure.lyricHeight;
  std::string current;
  for (std::string piece : words(text)) {
    if (!current.empty() && width(measure, false, current + piece) > limit) {
      row.items.push_back(Placed{current, 0, false});
      rows.push_back(row);
      row.items.clear();
      current.clear();
      piece.erase(0, piece.find_first_not_of(' '));
    }
    current += piece;
  }
  row.items.push_back(Placed{current, 0, false});
  rows.push_back(row);
}

}  // namespace

std::vector<Row> layout(const Song& song, const View& view, const int width, const Measure& measure) {
  std::vector<Row> rows;
  const int shift = shapeShift(song, view);
  const bool flats = prefersFlats(song, shift);
  for (const SongLine& line : song.lines) {
    switch (line.kind) {
      case LineKind::Pair:
        layoutPair(rows, line, shift, flats, width, measure);
        break;
      case LineKind::Text:
        layoutText(rows, line.text, RowKind::Text, width, measure);
        break;
      case LineKind::Section:
        layoutText(rows, line.text, RowKind::Section, width, measure);
        break;
      case LineKind::Blank: {
        Row blank = newRow(RowKind::Blank);
        blank.height = static_cast<int16_t>(measure.lyricHeight / 2);
        rows.push_back(blank);
        break;
      }
    }
  }
  return rows;
}

std::vector<int> pageStarts(const std::vector<Row>& rows, const int height) {
  std::vector<int> starts{0};
  int y = 0;
  for (int i = 0; i < static_cast<int>(rows.size()); i++) {
    const Row& row = rows[static_cast<size_t>(i)];
    // A blank at the top of a page is only a gap.
    if (y == 0 && row.kind == RowKind::Blank && i > 0) continue;
    if (y > 0 && y + row.height > height) {
      int at = i;
      if (at - 1 > starts.back() && rows[static_cast<size_t>(at - 1)].kind == RowKind::Section) at--;
      starts.push_back(at);
      y = 0;
      i = at - 1;
      continue;
    }
    y += row.height;
  }
  return starts;
}

}  // namespace songs
