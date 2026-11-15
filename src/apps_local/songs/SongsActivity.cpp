#include "SongsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../fontIds.h"
#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;

namespace {

constexpr const char* kSongsDir = "/songs";
constexpr const char* kSetlistsDir = "/songs/setlists";
constexpr const char* kViewsPath = "/.crosspoint/songs-views.txt";
constexpr const char* kSizePath = "/.crosspoint/songs-size.txt";
constexpr size_t kChartCap = 64 * 1024;

constexpr int kMargin = 16;
// Everything above the content: the band this app draws itself, as the
// calculator does.
constexpr int kChromeHeight = 64;
constexpr int kFooterHeight = 56;
constexpr int kRowHeight = 58;
constexpr int kGap = 8;

constexpr int kLyricFonts[3] = {NOTOSANS_14_FONT_ID, NOTOSANS_16_FONT_ID, NOTOSANS_18_FONT_ID};
constexpr const char* kSizeNames[3] = {"S", "M", "L"};

bool isChartFile(const std::string& name) {
  std::string lower = name;
  for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  for (const char* extension : {".txt", ".cho", ".pro", ".chopro", ".chordpro", ".crd"}) {
    const size_t length = strlen(extension);
    if (lower.size() > length && lower.compare(lower.size() - length, length, extension) == 0) return true;
  }
  return false;
}

std::string stem(const std::string& path) {
  const size_t slash = path.rfind('/');
  std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
  const size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

// Charts in `dir` and one level of folders below it, as "folder/name".
void collect(const std::string& dir, const std::string& prefix, const int depth, std::vector<std::string>& out) {
  HalFile folder = Storage.open(dir.c_str());
  if (!folder || !folder.isDirectory()) return;
  HalFile entry = folder.openNextFile();
  char name[256];
  while (entry) {
    entry.getName(name, sizeof(name));
    const std::string entryName(name);
    const std::string path = dir + "/" + entryName;
    if (entryName.empty() || entryName[0] == '.') {
      // hidden
    } else if (entry.isDirectory()) {
      if (depth == 0 && path != kSetlistsDir) collect(path, prefix + entryName + "/", depth + 1, out);
    } else if (isChartFile(entryName)) {
      out.push_back(path);
    }
    entry = folder.openNextFile();
  }
}

struct MeasureContext {
  const GfxRenderer* renderer;
  int chordFont;
  int lyricFont;
};

int measureText(void* context, const bool chord, const std::string& text) {
  const auto* c = static_cast<const MeasureContext*>(context);
  const int font = chord ? c->chordFont : c->lyricFont;
  const EpdFontFamily::Style style = chord ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  // Spaces at either end measure as nothing in getTextWidth, but they are what
  // separates one chord's lyric from the next.
  size_t first = 0;
  size_t last = text.size();
  while (first < last && text[first] == ' ') first++;
  while (last > first && text[last - 1] == ' ') last--;
  const int spaces = static_cast<int>(first + (text.size() - last));
  const int inner = last > first ? c->renderer->getTextWidth(font, text.substr(first, last - first).c_str(), style) : 0;
  return inner + spaces * c->renderer->getSpaceWidth(font, style);
}

}  // namespace

std::unique_ptr<Activity> SongsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<SongsActivity>(renderer, mappedInput);
}

void SongsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadSettings();
  scanLibrary();
  requestUpdate();
}

int SongsActivity::lyricFont() const { return kLyricFonts[size_]; }
// One size smaller than the lyrics, so the words lead and a verse fits a page.
int SongsActivity::chordFont() const { return kLyricFonts[size_ > 0 ? size_ - 1 : 0]; }

void SongsActivity::loadSettings() {
  std::string text;
  if (Storage.exists(kViewsPath) && Storage.readFileToString("SONGS", std::string(kViewsPath), 16 * 1024, text)) {
    savedViews_ = songs::parseViews(text);
  }
  char size[4] = {};
  if (Storage.readFileToBuffer(kSizePath, size, sizeof(size)) > 0 && size[0] >= '0' && size[0] <= '2') {
    size_ = size[0] - '0';
  }
}

void SongsActivity::saveSettings() const {
  Storage.writeFile(kViewsPath, String(songs::formatViews(savedViews_).c_str()));
  char size[16];
  std::snprintf(size, sizeof(size), "%d\n", size_);
  Storage.writeFile(kSizePath, String(size));
}

void SongsActivity::scanLibrary() {
  std::vector<std::string> paths;
  collect(kSongsDir, "", 0, paths);
  std::sort(paths.begin(), paths.end());
  library_.clear();
  library_.reserve(paths.size());
  for (const std::string& path : paths) {
    const std::string relative = path.substr(strlen(kSongsDir) + 1);
    const size_t dot = relative.rfind('.');
    library_.push_back(Item{dot == std::string::npos ? relative : relative.substr(0, dot), path});
  }
}

void SongsActivity::scanSetlists() {
  setlists_.clear();
  HalFile folder = Storage.open(kSetlistsDir);
  if (folder && folder.isDirectory()) {
    HalFile entry = folder.openNextFile();
    char name[256];
    while (entry) {
      entry.getName(name, sizeof(name));
      const std::string entryName(name);
      if (!entry.isDirectory() && entryName.size() > 4 && entryName.compare(entryName.size() - 4, 4, ".txt") == 0) {
        setlists_.push_back(Item{stem(entryName), std::string(kSetlistsDir) + "/" + entryName});
      }
      entry = folder.openNextFile();
    }
  }
  std::sort(setlists_.begin(), setlists_.end(), [](const Item& a, const Item& b) { return a.label < b.label; });
}

void SongsActivity::openSetlist(const std::string& path) {
  std::string text;
  Storage.readFileToString("SONGS", path, 16 * 1024, text);
  setlistItems_.clear();
  for (const std::string& song : songs::parseSetlist(text)) setlistItems_.push_back(Item{stem(song), song});
  setlistPath_ = path;
  setlistName_ = stem(path);
  editingSetlist_ = false;
  listTop_ = 0;
  screen_ = Screen::Setlist;
}

void SongsActivity::saveSetlist() const {
  std::vector<std::string> paths;
  paths.reserve(setlistItems_.size());
  for (const Item& item : setlistItems_) paths.push_back(item.path);
  Storage.mkdir(kSetlistsDir);
  Storage.writeFile(setlistPath_.c_str(), String(songs::formatSetlist(paths).c_str()));
}

void SongsActivity::openChart(const std::string& path, const int setlistIndex) {
  std::string text;
  if (!Storage.readFileToString("SONGS", path, kChartCap, text)) {
    LOG_ERR("SONGS", "could not read %s", path.c_str());
    return;
  }
  song_ = songs::parse(text, songs::looksLikeChordPro(path, text));
  if (song_.title.empty()) song_.title = stem(path);
  chartPath_ = path;
  setlistIndex_ = setlistIndex;
  view_ = songs::View{};
  view_.capo = song_.capo;  // opens at the capo the chart was written for
  for (const songs::SavedView& saved : savedViews_) {
    if (saved.path == path) view_ = saved.view;
  }
  page_ = 0;
  screen_ = Screen::Chart;
  relayout();
}

void SongsActivity::relayout() {
  MeasureContext context{&renderer, chordFont(), lyricFont()};
  songs::Measure measure;
  measure.context = &context;
  measure.width = &measureText;
  measure.chordHeight = static_cast<int16_t>(renderer.getLineHeight(chordFont()));
  measure.lyricHeight = static_cast<int16_t>(renderer.getLineHeight(lyricFont()) + 4);
  const int width = renderer.getScreenWidth() - 2 * kMargin;
  const int height = renderer.getScreenHeight() - kChromeHeight - kFooterHeight - 2 * kGap;
  rows_ = songs::layout(song_, view_, width, measure);
  pageStarts_ = songs::pageStarts(rows_, height);
  if (page_ >= static_cast<int>(pageStarts_.size())) page_ = static_cast<int>(pageStarts_.size()) - 1;
}

bool SongsActivity::nextPage() {
  if (page_ + 1 < static_cast<int>(pageStarts_.size())) {
    page_++;
    return true;
  }
  if (setlistIndex_ >= 0 && setlistIndex_ + 1 < static_cast<int>(setlistItems_.size())) {
    openChart(setlistItems_[static_cast<size_t>(setlistIndex_ + 1)].path, setlistIndex_ + 1);
    return true;
  }
  return false;
}

bool SongsActivity::previousPage() {
  if (page_ > 0) {
    page_--;
    return true;
  }
  if (setlistIndex_ > 0) {
    openChart(setlistItems_[static_cast<size_t>(setlistIndex_ - 1)].path, setlistIndex_ - 1);
    return true;
  }
  return false;
}

void SongsActivity::askNewSetlist(const bool addCurrentChart) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "SETLIST NAME", "", 40);
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this, addCurrentChart](const ActivityResult& result) {
    if (!result.isCancelled) {
      std::string name = std::get<KeyboardResult>(result.data).text;
      // A filename: no separators.
      for (char& c : name) {
        if (c == '/' || c == '\\' || c == ':') c = '-';
      }
      if (!name.empty()) {
        const std::string path = std::string(kSetlistsDir) + "/" + name + ".txt";
        if (addCurrentChart) {
          addCurrentChartTo(path);
        } else {
          setlistItems_.clear();
          setlistPath_ = path;
          setlistName_ = name;
          saveSetlist();
          openSetlist(path);
        }
      }
    }
    requestUpdate();
  });
}

void SongsActivity::addCurrentChartTo(const std::string& setlistPath) {
  std::string text;
  if (Storage.exists(setlistPath.c_str())) Storage.readFileToString("SONGS", setlistPath, 16 * 1024, text);
  std::vector<std::string> paths = songs::parseSetlist(text);
  paths.push_back(chartPath_);
  Storage.mkdir(kSetlistsDir);
  Storage.writeFile(setlistPath.c_str(), String(songs::formatSetlist(paths).c_str()));
  screen_ = Screen::Chart;
}

const std::vector<SongsActivity::Item>& SongsActivity::listItems() const {
  switch (screen_) {
    case Screen::Setlists:
    case Screen::PickSetlist:
      return setlists_;
    case Screen::Setlist:
      return setlistItems_;
    default:
      return library_;
  }
}

int SongsActivity::rowsPerPage() const {
  return (renderer.getScreenHeight() - kChromeHeight - kFooterHeight - 2 * kGap) / kRowHeight;
}

void SongsActivity::drawHeader(const char* title, const char* action) const {
  const int width = renderer.getScreenWidth();
  renderer.fillRect(0, 0, width, kChromeHeight, true);
  toybox::drawCapsCentered(renderer, toybox::kDisplayFontId, kMargin, 0, kChromeHeight, title, false);
  if (action != nullptr) {
    const int actionWidth = renderer.getTextWidth(toybox::kUiFontId, action);
    toybox::drawCapsCentered(renderer, toybox::kUiFontId, width - kMargin - actionWidth, 0, kChromeHeight, action,
                             false);
  }
}

void SongsActivity::drawFooter(const char* left, const char* right) const {
  const int width = renderer.getScreenWidth();
  const int y = renderer.getScreenHeight() - kFooterHeight;
  const int half = (width - 2 * kMargin - kGap) / 2;
  const auto button = [&](const int x, const int w, const char* label) {
    renderer.drawRect(x, y, w, kFooterHeight - kGap, 2, true);
    // The largest cut the label fits.
    int font = toybox::kTileFontId;
    for (const int candidate : {toybox::kUiFontId, toybox::kButtonFontId}) {
      if (renderer.getTextWidth(candidate, label) <= w - 2 * kGap) {
        font = candidate;
        break;
      }
    }
    const int labelWidth = renderer.getTextWidth(font, label);
    toybox::drawCapsCentered(renderer, font, x + (w - labelWidth) / 2, y, kFooterHeight - kGap, label, true);
  };
  if (left != nullptr && right != nullptr) {
    button(kMargin, half, left);
    button(kMargin + half + kGap, half, right);
  } else if (left != nullptr) {
    button(kMargin, width - 2 * kMargin, left);
  }
}

void SongsActivity::drawList() const {
  const std::vector<Item>& items = listItems();
  switch (screen_) {
    case Screen::Library:
      drawHeader("SONGS", "SETLISTS");
      break;
    case Screen::Setlists:
      drawHeader("SETLISTS", nullptr);
      break;
    case Screen::Setlist:
      drawHeader(setlistName_.c_str(), editingSetlist_ ? "DONE" : "EDIT");
      break;
    default:
      drawHeader("ADD TO SETLIST", nullptr);
      break;
  }
  const int width = renderer.getScreenWidth();
  int y = kChromeHeight + kGap;
  if (items.empty()) {
    const char* empty = screen_ == Screen::Library   ? "Copy chord charts into /songs on the card."
                        : screen_ == Screen::Setlist ? "Add songs from their charts' menu."
                                                     : "No setlists yet.";
    renderer.drawText(lyricFont(), kMargin, y + kGap, empty, true);
  }
  const int end = std::min(static_cast<int>(items.size()), listTop_ + rowsPerPage());
  for (int i = listTop_; i < end; i++) {
    const Item& item = items[static_cast<size_t>(i)];
    std::string label = item.label;
    if (screen_ == Screen::Setlist) label = std::to_string(i + 1) + ". " + label;
    while (label.size() > 1 && renderer.getTextWidth(lyricFont(), label.c_str()) > width - 2 * kMargin - 90)
      label.pop_back();
    renderer.drawText(lyricFont(), kMargin, y + (kRowHeight - renderer.getLineHeight(lyricFont())) / 2, label.c_str(),
                      true);
    if (screen_ == Screen::Setlist && editingSetlist_) {
      const char* remove = "REMOVE";
      toybox::drawCapsCentered(renderer, toybox::kTileFontId,
                               width - kMargin - renderer.getTextWidth(toybox::kTileFontId, remove), y, kRowHeight,
                               remove, true);
    }
    renderer.drawLine(kMargin, y + kRowHeight - 1, width - kMargin, y + kRowHeight - 1, true);
    y += kRowHeight;
  }
  const bool more = static_cast<int>(items.size()) > rowsPerPage();
  switch (screen_) {
    case Screen::Library:
      drawFooter(more ? "PREV" : nullptr, more ? "NEXT" : nullptr);
      break;
    case Screen::Setlists:
    case Screen::PickSetlist:
      drawFooter("+ NEW SETLIST", nullptr);
      break;
    case Screen::Setlist:
      drawFooter(more ? "PREV" : nullptr, more ? "NEXT" : nullptr);
      break;
    default:
      break;
  }
}

void SongsActivity::drawChart() const {
  char title[96];
  std::snprintf(title, sizeof(title), "%s", song_.title.c_str());
  drawHeader(title, "MENU");
  const int start = pageStarts_.empty() ? 0 : pageStarts_[static_cast<size_t>(page_)];
  const int end = page_ + 1 < static_cast<int>(pageStarts_.size()) ? pageStarts_[static_cast<size_t>(page_ + 1)]
                                                                   : static_cast<int>(rows_.size());
  const int chordHeight = renderer.getLineHeight(chordFont());
  int y = kChromeHeight + kGap;
  for (int i = start; i < end; i++) {
    const songs::Row& row = rows_[static_cast<size_t>(i)];
    if (i == start && row.kind == songs::RowKind::Blank && i > 0) continue;
    for (const songs::Placed& item : row.items) {
      const int x = kMargin + item.x;
      if (item.chord) {
        renderer.drawText(chordFont(), x, y, item.text.c_str(), true, EpdFontFamily::BOLD);
      } else {
        const int top = row.hasChords ? y + chordHeight : y;
        const EpdFontFamily::Style style =
            row.kind == songs::RowKind::Section ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
        renderer.drawText(lyricFont(), x, top, item.text.c_str(), true, style);
      }
    }
    y += row.height;
  }

  // The state that changes what the chords say, and where we are.
  char status[64];
  const int shift = songs::shapeShift(song_, view_);
  std::snprintf(status, sizeof(status), "%d / %d    TRANSPOSE %s    CAPO %d", page_ + 1,
                static_cast<int>(pageStarts_.size()), songs::signedText(view_.transpose).c_str(), view_.capo);
  (void)shift;
  const int statusY =
      renderer.getScreenHeight() - kFooterHeight + (kFooterHeight - renderer.getLineHeight(toybox::kTileFontId)) / 2;
  renderer.drawLine(kMargin, renderer.getScreenHeight() - kFooterHeight, renderer.getScreenWidth() - kMargin,
                    renderer.getScreenHeight() - kFooterHeight, true);
  renderer.drawText(toybox::kTileFontId, kMargin, statusY, status, true);
}

void SongsActivity::drawMenu() const {
  drawHeader(song_.title.c_str(), "DONE");
  const int width = renderer.getScreenWidth();
  int y = kChromeHeight + kGap * 2;
  const auto stepper = [&](const char* label, const std::string& value) {
    toybox::drawCapsCentered(renderer, toybox::kUiFontId, kMargin, y, kRowHeight, label, true);
    const int box = kRowHeight;
    const int plusX = width - kMargin - box;
    const int minusX = plusX - box * 2 - kGap * 2;
    renderer.drawRect(minusX, y, box, box, 2, true);
    renderer.drawRect(plusX, y, box, box, 2, true);
    toybox::drawCapsCentered(renderer, toybox::kDisplayFontId, minusX + box / 2 - 6, y, box, "-", true);
    toybox::drawCapsCentered(renderer, toybox::kDisplayFontId, plusX + box / 2 - 8, y, box, "+", true);
    const int valueWidth = renderer.getTextWidth(toybox::kUiFontId, value.c_str());
    toybox::drawCapsCentered(renderer, toybox::kUiFontId, minusX + box + (box + 2 * kGap - valueWidth) / 2, y, box,
                             value.c_str(), true);
    y += kRowHeight + kGap * 2;
  };
  stepper("TRANSPOSE", songs::signedText(view_.transpose));
  stepper("CAPO", std::to_string(view_.capo));

  toybox::drawCapsCentered(renderer, toybox::kUiFontId, kMargin, y, kRowHeight, "SIZE", true);
  for (int i = 0; i < 3; i++) {
    const int box = kRowHeight;
    const int x = width - kMargin - (3 - i) * box - (2 - i) * kGap;
    if (i == size_) {
      renderer.fillRect(x, y, box, box, true);
    } else {
      renderer.drawRect(x, y, box, box, 2, true);
    }
    toybox::drawCapsCentered(renderer, toybox::kUiFontId, x + box / 2 - 6, y, box, kSizeNames[i], i != size_);
  }
  y += kRowHeight + kGap * 3;

  // Shapes and sound, so the two numbers above have a meaning beside them.
  const int shift = songs::shapeShift(song_, view_);
  char note[96];
  std::snprintf(note, sizeof(note), "Written for capo %d.", song_.capo);
  renderer.drawText(NOTOSANS_14_FONT_ID, kMargin, y, note, true);
  y += renderer.getLineHeight(NOTOSANS_14_FONT_ID);
  if (shift == 0) {
    std::snprintf(note, sizeof(note), "Chords shown as written.");
  } else {
    std::snprintf(note, sizeof(note), "Chords shown %s semitones from written.", songs::signedText(shift).c_str());
  }
  renderer.drawText(NOTOSANS_14_FONT_ID, kMargin, y, note, true);
  drawFooter("ADD TO SETLIST", "RESET");
}

void SongsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  if (screen_ == Screen::Chart) {
    drawChart();
  } else if (screen_ == Screen::Menu) {
    drawMenu();
  } else {
    drawList();
  }
  interactionsReady = true;
  noteSurfaceBuilt();
  renderer.displayBuffer();
}

void SongsActivity::tapList(const int x, const int y) {
  const int width = renderer.getScreenWidth();
  const std::vector<Item>& items = listItems();
  if (y < kChromeHeight) {
    if (x < width / 2) return;
    if (screen_ == Screen::Library) {
      scanSetlists();
      listTop_ = 0;
      screen_ = Screen::Setlists;
    } else if (screen_ == Screen::Setlist) {
      editingSetlist_ = !editingSetlist_;
    }
    requestUpdate();
    return;
  }
  const int footerTop = renderer.getScreenHeight() - kFooterHeight;
  if (y >= footerTop) {
    if (screen_ == Screen::Setlists || screen_ == Screen::PickSetlist) {
      askNewSetlist(screen_ == Screen::PickSetlist);
      return;
    }
    const int pageRows = rowsPerPage();
    if (x < width / 2) {
      listTop_ = std::max(0, listTop_ - pageRows);
    } else if (listTop_ + pageRows < static_cast<int>(items.size())) {
      listTop_ += pageRows;
    }
    requestUpdate();
    return;
  }
  const int index = listTop_ + (y - kChromeHeight - kGap) / kRowHeight;
  if (index < listTop_ || index >= static_cast<int>(items.size()) || index >= listTop_ + rowsPerPage()) return;
  const Item item = items[static_cast<size_t>(index)];
  switch (screen_) {
    case Screen::Library:
      openChart(item.path, -1);
      break;
    case Screen::Setlists:
      openSetlist(item.path);
      break;
    case Screen::PickSetlist:
      addCurrentChartTo(item.path);
      break;
    case Screen::Setlist:
      if (editingSetlist_) {
        setlistItems_.erase(setlistItems_.begin() + index);
        saveSetlist();
      } else {
        openChart(item.path, index);
      }
      break;
    default:
      break;
  }
  requestUpdate();
}

void SongsActivity::tapChart(const int x, const int y) {
  if (y < kChromeHeight) {
    if (x > renderer.getScreenWidth() / 2) {
      screen_ = Screen::Menu;
      requestUpdate();
    }
    return;
  }
  // The left third goes back, the rest goes forward: forward is the common move.
  const bool moved = x < renderer.getScreenWidth() / 3 ? previousPage() : nextPage();
  if (moved) requestUpdate();
}

void SongsActivity::tapMenu(const int x, const int y) {
  const int width = renderer.getScreenWidth();
  if (y < kChromeHeight) {
    if (x > width / 2) {
      screen_ = Screen::Chart;
      requestUpdate();
    }
    return;
  }
  const int footerTop = renderer.getScreenHeight() - kFooterHeight;
  if (y >= footerTop) {
    if (x < width / 2) {
      scanSetlists();
      listTop_ = 0;
      screen_ = Screen::PickSetlist;
    } else {
      view_ = songs::View{};
      view_.capo = song_.capo;
      savedViews_.erase(std::remove_if(savedViews_.begin(), savedViews_.end(),
                                       [this](const songs::SavedView& saved) { return saved.path == chartPath_; }),
                        savedViews_.end());
      saveSettings();
      relayout();
    }
    requestUpdate();
    return;
  }
  const int box = kRowHeight;
  const int plusX = width - kMargin - box;
  const int minusX = plusX - box * 2 - kGap * 2;
  const int row = (y - kChromeHeight - kGap * 2) / (kRowHeight + kGap * 2);
  const int step = x >= plusX ? 1 : (x >= minusX && x < minusX + box ? -1 : 0);
  bool changed = false;
  if (row == 0 && step != 0) {
    view_.transpose = std::max(-11, std::min(11, view_.transpose + step));
    changed = true;
  } else if (row == 1 && step != 0) {
    view_.capo = std::max(0, std::min(12, view_.capo + step));
    changed = true;
  } else if (row == 2) {
    for (int i = 0; i < 3; i++) {
      const int bx = width - kMargin - (3 - i) * box - (2 - i) * kGap;
      if (x >= bx && x < bx + box && size_ != i) {
        size_ = i;
        changed = true;
      }
    }
  }
  if (!changed) return;
  bool found = false;
  for (songs::SavedView& saved : savedViews_) {
    if (saved.path == chartPath_) {
      saved.view = view_;
      found = true;
    }
  }
  if (!found) savedViews_.push_back(songs::SavedView{chartPath_, view_});
  saveSettings();
  relayout();
  requestUpdate();
}

void SongsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (screen_) {
      case Screen::Library:
        shelf::leave(renderer, mappedInput);
        return;
      case Screen::Menu:
        screen_ = Screen::Chart;
        break;
      case Screen::PickSetlist:
        screen_ = Screen::Menu;
        break;
      case Screen::Chart:
        screen_ = setlistIndex_ >= 0 ? Screen::Setlist : Screen::Library;
        break;
      case Screen::Setlist:
        scanSetlists();
        screen_ = Screen::Setlists;
        break;
      case Screen::Setlists:
        screen_ = Screen::Library;
        break;
    }
    listTop_ = 0;
    requestUpdate();
    return;
  }
  if (screen_ == Screen::Chart) {
    bool moved = false;
    if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) moved = nextPage();
    if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) moved = previousPage();
    if (moved) {
      requestUpdate();
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady) return;
  switch (screen_) {
    case Screen::Chart:
      tapChart(tapX, tapY);
      break;
    case Screen::Menu:
      tapMenu(tapX, tapY);
      break;
    default:
      tapList(tapX, tapY);
      break;
  }
}
