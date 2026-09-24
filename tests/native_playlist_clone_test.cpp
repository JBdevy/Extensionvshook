#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

struct ReaProject {};
#include "playlist-types.h"

static ReaProject project, otherProject;
static ReaProject* activeProject = &project;
static std::vector<NativeUiPlaylistRecord> stored, g_nativeUiPlaylists;
static std::map<int, bool> g_nativeUiPlaylistSelection;
static int g_nativeUiPlaylistSelectedIndex = -1;
static int g_nativeUiPlaylistLastClickedIndex = -1;
static int g_nativeUiPlaylistScroll = 0, g_nativeUiPlaylistVisibleRows = 3;
static std::string g_nativeUiPlaylistInput;
static size_t g_nativeUiPlaylistInputCursor = 0;
static bool g_nativeUiPlaylistInputFocused = false;
static std::atomic<bool> g_nativeUiPlaylistFilterDirty{false};
static std::vector<int> g_nativeUiPlaylistRowHits;
static bool failSave = false;
static int saves = 0, refreshes = 0;

static ReaProject* getCurrentProject(char*, int) { return activeProject; }
static std::string nativeTrim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
static std::string nativeUpperNamePtBr(std::string value) {
  for (auto& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return value; // Fixture names are ASCII; production uses the PT-BR helper.
}
static std::vector<NativeUiPlaylistRecord> nativeUiReadPlaylistRecords(ReaProject* p) {
  assert(p == &project);
  return stored;
}
static std::vector<NativeUiPlaylistRecord> nativeUiReadPlaylistDropdownRecords(ReaProject* p) {
  auto result = nativeUiReadPlaylistRecords(p);
  for (size_t i = 0; i < result.size(); ++i) {
    result[i].project = p;
    result[i].localIndex = static_cast<int>(i);
  }
  return result;
}
static bool nativeUiSavePlaylistRecords(ReaProject* p, const std::vector<NativeUiPlaylistRecord>& data) {
  assert(p == &project);
  if (failSave) return false;
  stored = data;
  ++saves;
  return true;
}
static bool nativeUiOpenPlaylistDropdown() { ++refreshes; return true; }

#include "playlist-clone-under-test.h"

static void select(int index) {
  g_nativeUiPlaylists = nativeUiReadPlaylistDropdownRecords(&project);
  nativeUiSelectPlaylistIndex(index, false, false);
}
int main() {
  NativeUiPlaylistRecord original;
  original.name = "DOMINGO";
  original.itemLines = {
    "ITEM\tblock\t-1\t0\t0\tBLOCO\tgold\t0\t0\t0\t0\t0\t120\t1\t1",
    "ITEM\tsong\t42\t10\t30\tMúsica com acento\tgold\t0\t1\t0",
    "ITEM\tchild\t43\t12\t14\tFilho\tgold\t0\t0\t1\t43",
    "ITEM\tsong\t42\t10\t30\tMúsica com acento\tgold\t0\t1\t0"
  };
  stored = {original};
  select(0);
  assert(nativeUiCloneSelectedPlaylist());
  assert(stored.size() == 2 && stored[1].name == "DOMINGO - clone 2");
  assert(stored[0].name == "DOMINGO" && stored[0].itemLines == original.itemLines);
  assert(stored[1].itemLines == original.itemLines);
  assert(g_nativeUiPlaylistSelectedIndex == 1 && g_nativeUiPlaylistInput == "DOMINGO - clone 2");
  assert(nativeUiPlaylistSelectedCount() == 1 && !g_nativeUiPlaylistInputFocused);
  assert(activeProject == &project);
  // Repeated click clones the selected copy without accumulating suffixes.
  assert(nativeUiCloneSelectedPlaylist());
  assert(stored.back().name == "DOMINGO - clone 3");
  stored[1].itemLines.clear();
  assert(stored[0].itemLines == original.itemLines && stored[2].itemLines == original.itemLines);
  // Clone the actual selection (not the active/first repertoire), including empty ones.
  select(1);
  assert(nativeUiCloneSelectedPlaylist());
  assert(stored.back().name == "DOMINGO - clone 4" && stored.back().itemLines.empty());
  select(0);
  failSave = true;
  assert(!nativeUiCloneSelectedPlaylist() && stored.size() == 4);
  assert(g_nativeUiPlaylistSelectedIndex == 0);
  failSave = false;
  g_nativeUiPlaylistSelection.clear();
  assert(!nativeUiCloneSelectedPlaylist());
  g_nativeUiPlaylistSelection = {{0, true}, {1, true}};
  assert(!nativeUiCloneSelectedPlaylist());
  select(0);
  g_nativeUiPlaylists[0].project = &otherProject;
  assert(!nativeUiCloneSelectedPlaylist());
  select(0);
  activeProject = nullptr;
  assert(!nativeUiCloneSelectedPlaylist());
  activeProject = &project;
  select(0);
  g_nativeUiPlaylists[0].localIndex = 99;
  assert(!nativeUiCloneSelectedPlaylist() && refreshes == 1);
  select(0);
  stored[0].name = "ALTERADO FORA DO MODAL";
  assert(!nativeUiCloneSelectedPlaylist() && refreshes == 2);
  assert(saves == 3);
  NativeUiPlaylistRecord manualName;
  manualName.name = "  domingo - cLoNe 2  ";
  assert(nativeUiNextPlaylistCloneName("DOMINGO", {manualName}) == "DOMINGO - clone 3");
  assert(nativeUiNextPlaylistCloneName("SABADO", {manualName}) == "SABADO - clone 2");
  assert(nativeUiNextPlaylistCloneName("DOMINGO - CLONE 2", {manualName}) == "DOMINGO - clone 3");
  assert(nativeUiNextPlaylistCloneName("  São João  ", {}) == "São João - clone 2");
  assert(nativeUiNextPlaylistCloneName("São João - clone 12", {}) == "São João - clone 2");
  assert(nativeUiNextPlaylistCloneName("Show - clone especial", {}) == "Show - clone especial - clone 2");
  assert(nativeUiNextPlaylistCloneName("Show - clone 2 extra", {}) == "Show - clone 2 extra - clone 2");
  assert(nativeUiNextPlaylistCloneName("Show - clone 1", {}) == "Show - clone 1 - clone 2");
  assert(nativeUiNextPlaylistCloneName("Show - clone ", {}) == "Show - clone - clone 2");
  std::cout << "PLAYLIST_CLONE_OK: naming, exact entries, selection, failures and project isolation\n";
}
