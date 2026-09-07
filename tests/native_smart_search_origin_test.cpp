#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <cassert>
#include <iostream>
struct NativeAppActivePanelModel {
  struct Row { bool block = false; };
  bool regionsPage = false, mixerPage = false;
  std::vector<Row> rows{Row{}};
};
static NativeAppActivePanelModel g_nativeAppActivePanelModel;
static bool g_nativeMainSearchFocused = false, g_nativeMainSearchApplyPending = false;
static bool g_nativeMainSmartSearchSelectFirstPending = false;
static bool g_nativeUiRowsSourceSignatureValid = false;
static bool g_nativeMainSmartSearchExpectedRegionsPage = false;
static bool g_nativeUiTextMouseSelecting = false, g_nativeUiMusicSelectionVisualActive = false;
static std::string g_nativeMainSearchText, g_nativeMainSearchAppliedText;
static std::string g_nativeMainSmartSearchSourcePlaylistName;
static size_t g_nativeMainSearchCursor = 0, g_nativeUiTextSelectionAnchor = 0;
static int g_nativeAppActiveListScrollPixels = 0;
static int g_nativeUiSelectionAnchorVisibleIndex = 0, g_nativeUiSelectionFocusVisibleIndex = 0;
static std::vector<size_t> g_nativeMainVisibleRowIndices{0};
static std::chrono::steady_clock::time_point g_nativeMainSearchApplyAt;
static std::atomic<bool> g_nativeForceStateBuild{false}, g_nativeForceSnapshotBuild{false};
enum class NativeUiTextInputKind { None, SmartSearch };
static NativeUiTextInputKind g_nativeUiTextSelectionKind = NativeUiTextInputKind::None;
struct NativeUiSmartSearchScrollTarget {};
static NativeUiSmartSearchScrollTarget g_nativeMainSmartSearchScrollTarget;
struct NativeSongWindow {};
static const char* kNativeSmartSearchInsertQueuePlaylistKey = "insert";
static const char* kNativeSmartSearchScrollQueuedKey = "queue";
static const char* kNativeSmartSearchScrollSelectedKey = "selected";
static bool playing = false, localMatch = true, insertEnabled = true;
static int playlistReads = 0, localChecks = 0, inserts = 0;
static std::string lastCommand;
static std::string nativeTrim(const std::string& value) { return value; }
static std::string nativeUiSmartSearchCurrentPlaylistName() { ++playlistReads; return "Repertorio salvo"; }
static void nativeUiSmartSearchSetPage(bool regions) {
  g_nativeAppActivePanelModel.regionsPage = regions;
  g_nativeAppActivePanelModel.mixerPage = false;
  g_nativeMainSmartSearchExpectedRegionsPage = regions;
}
static bool nativeUiSmartSearchHasLocalMatch(const std::string&) { ++localChecks; return localMatch; }
static bool nativeUiTransportActive() { return playing; }
static bool nativeUiFindSongForRow(const NativeAppActivePanelModel::Row&, NativeSongWindow&) { return true; }
static bool nativeUiSmartSearchSettingEnabled(const char* key) { return std::string(key) != "insert" || insertEnabled; }
static bool nativeUiSmartSearchInsertBelowPlaying(const NativeSongWindow&) { ++inserts; return true; }
static std::string nativeMainSongCommandJson(const char* type, const NativeAppActivePanelModel::Row&, bool) { return type; }
static void nativeApplyQueueCommand(const std::string& command) { lastCommand = command; }
static void nativeApplySelectionCommand(const std::string& command) { lastCommand = command; }
static void nativeUiSmartSearchRememberScrollTarget(const NativeAppActivePanelModel::Row&, bool, bool = false) {}
static void nativeUiOpenSearchResultFamily(const NativeAppActivePanelModel::Row&) {}
static void nativeUiClearMainRowSelectionForPage(bool) {}
static void nativeUiSetMainRowSelected(const NativeAppActivePanelModel::Row&, bool, bool) {}
#include "search-under-test.h"
static void query(const std::string& text) {
  g_nativeMainSearchText = text;
  g_nativeMainSearchApplyPending = true;
  nativeUiCommitPendingSmartSearchQuery(true);
}
int main() {
  for (bool running : {false, true}) {
    for (bool foundInPlaylist : {false, true}) {
      playing = running;
      localMatch = foundInPlaylist;
      g_nativeAppActivePanelModel.regionsPage = true;
      assert(nativeUiToggleSmartSearch());
      assert(g_nativeMainSmartSearchSourcePlaylistName.empty());
      query("Musica");
      assert(g_nativeAppActivePanelModel.regionsPage);
      assert(nativeUiActivateSmartSearchResult(g_nativeAppActivePanelModel.rows[0], true));
      assert(g_nativeAppActivePanelModel.regionsPage);
      assert(lastCommand == (running ? "queue_region_song" : "select_region"));
      assert(inserts == 0);
      nativeUiToggleSmartSearch();
      query("Outro");
      query("");
      nativeUiCloseSmartSearch(true);
      assert(g_nativeAppActivePanelModel.regionsPage);
    }
  }
  assert(playlistReads == 0);
  // Existing playlist search: local result remains local; missing result
  // searches all songs, can insert while playing, and returns to playlist.
  g_nativeAppActivePanelModel.regionsPage = false;
  localMatch = true;
  playing = false;
  nativeUiToggleSmartSearch();
  query("Local");
  assert(!g_nativeAppActivePanelModel.regionsPage);
  nativeUiActivateSmartSearchResult(g_nativeAppActivePanelModel.rows[0], false);
  assert(lastCommand == "select_playlist_song");
  localMatch = false;
  playing = true;
  nativeUiToggleSmartSearch();
  query("Fora do repertorio");
  assert(g_nativeAppActivePanelModel.regionsPage);
  nativeUiActivateSmartSearchResult(g_nativeAppActivePanelModel.rows[0], true);
  assert(inserts == 1);
  assert(!g_nativeAppActivePanelModel.regionsPage);
  nativeUiToggleSmartSearch();
  query("Fora novamente");
  nativeUiCloseSmartSearch(true);
  assert(!g_nativeAppActivePanelModel.regionsPage);
  std::cout << "SMART_SEARCH_ORIGIN_OK: Musicas stays local stopped/playing; Repertorio fallback, queue insertion and cancel preserved\n";
}
