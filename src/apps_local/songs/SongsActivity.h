#pragma once

// Songs: chord Charts from the SD card, with transpose, capo and Setlists.
//
// Drawn straight into the renderer and hit-tested by geometry, like the
// calculator, so a long library page is not limited by the interaction table.

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "SongsCore.h"
#include "SongsLayout.h"

class SongsActivity final : public Activity {
 public:
  SongsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Songs", renderer, mappedInput) {}
  ~SongsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { Library, Setlists, Setlist, PickSetlist, Chart, Menu };

  // A row on a list screen.
  struct Item {
    std::string label;
    std::string path;
  };

  void scanLibrary();
  void scanSetlists();
  void openSetlist(const std::string& path);
  void saveSetlist() const;
  void openChart(const std::string& path, int setlistIndex);
  void relayout();
  void loadSettings();
  void saveSettings() const;
  void askNewSetlist(bool addCurrentChart);
  void addCurrentChartTo(const std::string& setlistPath);
  bool nextPage();
  bool previousPage();

  void tapList(int x, int y);
  void tapChart(int x, int y);
  void tapMenu(int x, int y);

  void drawHeader(const char* title, const char* action) const;
  void drawList() const;
  void drawChart() const;
  void drawMenu() const;
  void drawFooter(const char* left, const char* right) const;

  const std::vector<Item>& listItems() const;
  int rowsPerPage() const;
  int chordFont() const;
  int lyricFont() const;

  Screen screen_ = Screen::Library;
  std::vector<Item> library_;
  std::vector<Item> setlists_;
  std::vector<Item> setlistItems_;
  std::string setlistPath_;
  std::string setlistName_;
  bool editingSetlist_ = false;
  int listTop_ = 0;

  std::string chartPath_;
  songs::Song song_;
  songs::View view_;
  std::vector<songs::Row> rows_;
  std::vector<int> pageStarts_;
  int page_ = 0;
  int setlistIndex_ = -1;  // where the open Chart is in the open Setlist, or -1
  int size_ = 1;           // 0, 1 or 2

  std::vector<songs::SavedView> savedViews_;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
