#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <cassert>
#include <iostream>
#include <map>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include "../src/native_smart_search_session.h"
static NativeSmartSearchSession g_nativeSmartSearchSession;
struct NativeAppActivePanelModel {
  struct Row {
    bool block = false;
    std::string id = "region-7", name = "Musica";
    int order = 12, sourceNumber = 7;
    double start = 120.0, end = 180.0;
  };
  bool regionsPage = false, mixerPage = false;
  std::vector<Row> rows{Row{}};
};
static NativeAppActivePanelModel g_nativeAppActivePanelModel;
static bool g_nativeMainSearchFocused = false, g_nativeMainSearchApplyPending = false;
static bool g_nativeMainSmartSearchSelectFirstPending = false;
static bool g_nativeUiRowsSourceSignatureValid = false;
static bool g_nativeMainSmartSearchExpectedRegionsPage = false;
static bool g_nativeUiTextMouseSelecting = false, g_nativeUiMusicSelectionVisualActive = false;
static bool g_nativeUiMainSelectionAuthoritative = false;
static std::map<std::string, bool> g_nativeUiSelectedRows;
static std::vector<std::string> g_nativeUiSelectionOrder;
static std::string g_nativeUiExclusiveSelectedSongIdentity;
static void nativeUiClearMainRowSelection();
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
static std::chrono::steady_clock::time_point g_nativeUiLastModelRefreshAt;
static std::string nativeLower(const std::string& value) { return value; }
static std::string nativeJsonString(const std::string& value) { return "\"" + value + "\""; }
static std::string nativeJsonExtractString(const std::string& json, const std::string& key) {
  const std::string field = "\"" + key + "\":";
  size_t start = json.find(field);
  if (start == std::string::npos) return "";
  start += field.size();
  const bool quoted = json[start] == '"';
  if (quoted) ++start;
  const auto end = quoted ? json.find('"', start) : json.find_first_of(",}", start);
  return json.substr(start, end - start);
}
static void nativeTimecodeLanRecordCommand(const std::string&) {}
static void nativeRefreshAppActivePanelModel() {}
static std::vector<size_t> nativeUiBuildVisibleRowIndices() { return {0}; }
static void nativeUiSmartSearchQueryChanged() { g_nativeMainSearchApplyPending = true; }
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
static void nativeApplySelectionCommand(const std::string& command) {
  lastCommand = command;
  if (command.find("set_page") != std::string::npos) {
    nativeUiSmartSearchSetPage(nativeJsonExtractString(command, "page") == "regions");
  }
  // The production command clears both the functional AND visual selection.
  if (command.find("clear_selection") != std::string::npos) nativeUiClearMainRowSelection();
}
static void nativeUiSmartSearchRememberScrollTarget(const NativeAppActivePanelModel::Row&, bool, bool = false) {}
static void nativeUiOpenSearchResultFamily(const NativeAppActivePanelModel::Row&) {}
#include "search-under-test.h"
static void assertSelectionSurvivesSearchClose(bool regionsPage) {
  assert(!g_nativeMainSearchFocused);
  assert(g_nativeUiMainSelectionAuthoritative);
  assert(g_nativeUiSelectedRows.size() == 1);
  assert(g_nativeUiSelectedRows.begin()->first.rfind(regionsPage ? "regions|" : "playlist|", 0) == 0);
  assert(nativeUiMainRowIsSelected(g_nativeAppActivePanelModel.rows[0], regionsPage));
  // Closing search removes virtual children of unrelated closed drawers;
  // the same result can now have a different display order.
  auto rebuiltRow = g_nativeAppActivePanelModel.rows[0];
  rebuiltRow.order = 3;
  assert(nativeUiMainRowIsSelected(rebuiltRow, regionsPage));
  rebuiltRow.id = "another-song";
  assert(!nativeUiMainRowIsSelected(rebuiltRow, regionsPage));
}
static void query(const std::string& text) {
  g_nativeMainSearchText = text;
  g_nativeMainSearchApplyPending = true;
  nativeUiCommitPendingSmartSearchQuery(true);
}
static void testRemoteSearchProtocol() {
  g_nativeSmartSearchSession = {};
  playing = false;
  localMatch = true;
  const auto command = [](const std::string& type, int serial, int sequence,
      const std::string& result = "region-7") {
    return "{\"type\":\"smart_search_" + type + "\",\"searchClient\":\"tablet\",\"searchSerial\":" +
      std::to_string(serial) + ",\"searchSequence\":" + std::to_string(sequence) +
      ",\"page\":\"regions\",\"query\":\"Musica\",\"resultId\":\"" + result + "\",\"resultStart\":120}";
  };
  nativeApplySmartSearchCommand(command("open", 1, 1));
  assert(g_nativeMainSearchFocused && g_nativeAppActivePanelModel.regionsPage);
  nativeApplySmartSearchCommand(command("query", 1, 3));
  nativeApplySmartSearchCommand(command("query", 1, 2));
  assert(g_nativeSmartSearchSession.sequence == 3); // Out-of-order query ignored.
  nativeApplySmartSearchCommand(command("activate", 1, 4));
  assert(g_nativeSmartSearchSession.activationOk);
  assert(g_nativeSmartSearchSession.activationSequence == 4);
  assert(!g_nativeMainSearchFocused);
  assertSelectionSurvivesSearchClose(true);
  nativeApplySmartSearchCommand(command("query", 1, 5));
  assert(!g_nativeMainSearchFocused); // Delayed query cannot reopen a closed search.
  nativeApplySmartSearchCommand(command("open", 2, 1));
  nativeApplySmartSearchCommand(command("close", 1, 6));
  assert(g_nativeMainSearchFocused); // Old close cannot close a new search.
  nativeApplySmartSearchCommand(command("open", 1, 1));
  assert(g_nativeSmartSearchSession.serial == 2);
  nativeApplySmartSearchCommand(command("activate", 2, 2, "missing-song"));
  assert(g_nativeMainSearchFocused && !g_nativeSmartSearchSession.activationOk);
  assert(!g_nativeSmartSearchSession.activationError.empty());
  playing = true;
  nativeApplySmartSearchCommand(command("activate", 2, 3));
  assert(g_nativeSmartSearchSession.activationOk && lastCommand == "queue_region_song");
  assert(!g_nativeMainSearchFocused);
  nativeApplySmartSearchCommand(command("activate", 2, 3));
  assert(g_nativeSmartSearchSession.activationSequence == 3);
  nativeApplySmartSearchCommand(command("open", 3, 1));
  assert(!g_nativeSmartSearchSession.accept("phone", 3, 2));
  nativeApplySmartSearchCommand(command("close", 3, 2));
  assert(!g_nativeMainSearchFocused);
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
      if (!running) assertSelectionSurvivesSearchClose(true);
      else assert(g_nativeUiSelectedRows.empty()); // Queue keeps its normal highlight.
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
  assertSelectionSurvivesSearchClose(false);
  playing = true;
  nativeUiToggleSmartSearch();
  query("Local durante play");
  nativeUiActivateSmartSearchResult(g_nativeAppActivePanelModel.rows[0], false);
  assert(lastCommand == "queue_playlist_song");
  assert(g_nativeUiSelectedRows.empty());
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
  testRemoteSearchProtocol();
  std::cout << "SMART_SEARCH_ORIGIN_OK: local/remote selection, queue, failure acknowledgement, session isolation and stale requests\n";
}
