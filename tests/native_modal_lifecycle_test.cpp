#include <algorithm>
#include <cassert>
#include <chrono>
#include <cctype>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
struct ReaProject {};
struct RECT { int left, top, right, bottom; };
#include "modal-types.h"
static NativeMainModalKind g_nativeMainModalKind = NativeMainModalKind::None;
// Other editors' row payloads are immaterial here: closing only clears them.
static auto g_nativeMainModalCancelRect = RECT{0, 0, 0, 0};
static auto g_nativeMainModalCloseRect = RECT{0, 0, 0, 0};
static auto g_nativeMainModalInputRect = RECT{0, 0, 0, 0};
static auto g_nativeUiTimerCaretPositionsValid = false;
static std::vector<int> g_nativeUiPlaylistRowHits;
static auto g_nativeUiPlaylistInputFocused = false;
static auto g_nativeUiPlaylistInputCursor = 0;
static auto g_nativeUiCreatePlaylistInputFocused = false;
static auto g_nativeUiCreatePlaylistInputCursor = 0;
static auto g_nativeUiTextSelectionKind = NativeUiTextInputKind::None;
static auto g_nativeUiTextSelectionAnchor = 0;
static auto g_nativeUiTextMouseSelecting = false;
static std::vector<int> g_nativeUiTargetPlaylistRowHits;
static std::string g_nativeUiRenameId;
static std::string g_nativeUiRenameOriginalName;
static std::string g_nativeUiRenameInput;
static auto g_nativeUiRenameInputCursor = 0;
static auto g_nativeMainRowInlineRenameOpen = false;
static auto g_nativeMainRowInlineRenameInputRect = RECT{0, 0, 0, 0};
static auto g_nativeHookControllerRowInlineRenameOwner = false;
static auto g_nativeHookControllerRowInlineRenameInputRect = RECT{0, 0, 0, 0};
static auto g_nativeUiRenameBlockCustomName = false;
static auto g_nativeUiRenameBlockCentered = false;
static auto g_nativeUiRenameFamilyParent = false;
static std::string g_nativeUiHelpTopic;
static auto g_nativeUiHelpScroll = 0;
static std::string g_nativeUiConfigAccessInfo;
static std::vector<int> g_nativeUiConverterPartsEntries;
static std::vector<int> g_nativeUiConverterPartsRowHits;
static std::vector<int> g_nativeUiConverterPartsSelection;
static auto g_nativeUiConverterPartsAnchor = -1;
static auto g_nativeUiConverterPartsScroll = 0;
static std::vector<int> g_nativeUiPremixRows;
static std::vector<int> g_nativeUiPremixSliderHits;
static std::string g_nativeUiPremixSongName;
static auto g_nativeUiPremixFamilyMode = false;
static auto g_nativeUiPremixScroll = 0;
static auto g_nativeUiPremixSliderDragging = false;
static auto g_nativeUiPremixActiveSliderRow = -1;
static auto g_nativeUiPremixActiveSliderRect = RECT{0, 0, 0, 0};
static auto g_nativeUiBlockMidiNumber = 0;
static std::string g_nativeUiBlockMidiInfo;
static std::string g_nativeUiBlockColorKey;
static std::vector<int> g_nativeUiMultiLoopTracks;
static auto g_nativeUiMultiLoopSlot = 0;
static auto g_nativeUiMultiLoopTrackScroll = 0;
static std::string g_nativeUiMultiLoopLimitGuid;
static std::string g_nativeUiMultiLoopLimitTrackName;
static auto g_nativeUiMultiLoopLimitBaseDb = 0.0;
static auto g_nativeUiMultiLoopLimitValueDb = -90.0;
static auto g_nativeUiMultiLoopLimitSliderRect = RECT{0, 0, 0, 0};
static auto g_nativeUiMultiLoopLimitDragging = false;
static std::vector<int> g_nativeUiManualStopTracks;
static std::vector<int> g_nativeUiManualStopTrackHits;
static auto g_nativeUiManualStopTrackScroll = 0;
static std::string g_nativeUiMixerTrackId;
static std::string g_nativeUiMixerTrackName;
static std::string g_nativeUiMixerAction;
static std::string g_nativeUiMixerInfo;
static std::string g_nativeUiMixerBulkAction;
static std::vector<int> g_nativeUiMixerBulkTracks;
static std::vector<int> g_nativeUiMixerBulkTrackRowHits;
static auto g_nativeUiMixerBulkScroll = 0;
static auto g_nativeUiSelectPlaylistMode = NativeUiSelectPlaylistMode::AddSelection;
static std::vector<int> g_nativeUiExportRegionNumbers;
static std::string g_nativeUiExportFormat;
static std::string g_nativeUiExportInfo;
static auto g_nativeUiExportReturnsToPlaylist = false;
static auto g_nativeUiCaptureStartedAt = std::chrono::steady_clock::time_point();
static std::string g_nativeUiCreatePlaylistInput;
static ReaProject project;
static bool failSave = false;
static int saves = 0, selectionClears = 0, multiloopCloses = 0, premixCloses = 0;
static std::vector<NativeUiPlaylistRecord> stored, g_nativeUiPlaylists;
static int g_nativeUiTargetPlaylistIndex = -1;
struct NativeSongWindow { std::string id; bool isHashParent = false; };
static std::mutex g_nativeMutex;
static std::vector<NativeSongWindow> g_nativeSongWindows, selectedSongs;
static const char* kPlaylistExtSection = "playlist";
static std::string lastPlaylist;
static int setExtState(ReaProject*, const char*, const char*, const char* name) { lastPlaylist = name; return 1; }
static auto SetProjExtState_ptr = &setExtState;
static ReaProject* getCurrentProject(char*, int) { return &project; }
static std::string nativeTrim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? std::string{} : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
static std::string nativeUpperNamePtBr(std::string value) {
  for (auto& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return value; // Names are ASCII here; the production normalizer is unchanged.
}
static std::string nativeJsonString(const std::string& value) { return "\"" + value + "\""; }
static std::vector<NativeUiPlaylistRecord> nativeUiReadPlaylistRecords(ReaProject*) { return stored; }
static bool nativeUiSavePlaylistRecords(ReaProject*, const std::vector<NativeUiPlaylistRecord>& records) {
  if (failSave) return false;
  stored = records; ++saves; return true;
}
static std::vector<NativeSongWindow> nativeUiPlaylistSongsFromRegionSelection() { return selectedSongs; }
static std::string nativeUiPlaylistItemLineForSong(const NativeSongWindow& song) { return song.id; }
static bool nativeUiPlaylistContainsSong(const NativeUiPlaylistRecord& record, const NativeSongWindow& song) {
  return std::find(record.itemLines.begin(), record.itemLines.end(), song.id) != record.itemLines.end();
}
static void nativeUiRemoveDuplicatePlaylistSongs(NativeUiPlaylistRecord&) {}
static void nativeUiRemoveLooseFamilyChildren(NativeUiPlaylistRecord&, const NativeSongWindow&, const std::vector<NativeSongWindow>&) {}
static void nativeUiClearMainRowSelection() { selectedSongs.clear(); ++selectionClears; }
static bool nativeApplyPlaylistCommand(const std::string&) { return true; }
static bool nativeApplySelectionCommand(const std::string&) { return true; }
static bool nativeApplyMultiLoopCommand(const std::string& body) { assert(body == "{\"type\":\"multiloop_close\"}"); ++multiloopCloses; return true; }
static bool nativeApplyPremixCommand(const std::string& body) { assert(body == "{\"type\":\"premix_close\"}"); ++premixCloses; return true; }
#include "modal-under-test.h"
int main() {
  // Cancel/Esc use this same closing routine. Every modal must release it.
  for (int kind = 0; kind <= static_cast<int>(NativeMainModalKind::Message); ++kind) {
    g_nativeMainModalKind = static_cast<NativeMainModalKind>(kind);
    g_nativeUiCreatePlaylistInputFocused = true;
    g_nativeUiTextSelectionKind = NativeUiTextInputKind::CreatePlaylist;
    g_nativeUiTextMouseSelecting = true;
    nativeUiCloseMainModal();
    assert(g_nativeMainModalKind == NativeMainModalKind::None);
    assert(!g_nativeUiCreatePlaylistInputFocused && !g_nativeUiTextMouseSelecting);
    assert(g_nativeUiTextSelectionKind == NativeUiTextInputKind::None);
    nativeUiCloseMainModal(); // Repeated close cannot resend editor commands.
  }
  assert(multiloopCloses == 3 && premixCloses == 1 && saves == 0);
  // A successful create writes once and disappears, rather than reporting its
  // own just-created name as a duplicate in the still-open dialog.
  nativeUiOpenCreatePlaylist();
  g_nativeUiCreatePlaylistInput = "  sexta  "; selectedSongs = {{"song-a"}, {"song-b"}, {"song-a"}};
  assert(nativeUiCreatePlaylistFromSelection());
  assert(g_nativeMainModalKind == NativeMainModalKind::None && saves == 1);
  assert(stored.size() == 1 && stored[0].name == "SEXTA" && stored[0].itemLines.size() == 2);
  // Genuine duplicates remain blocked, but Cancel must always work.
  nativeUiOpenCreatePlaylist(); g_nativeUiCreatePlaylistInput = "sexta";
  assert(!nativeUiCreatePlaylistFromSelection() && saves == 1);
  nativeUiCloseMainModal(); assert(g_nativeMainModalKind == NativeMainModalKind::None);
  nativeUiOpenCreatePlaylist(); g_nativeUiCreatePlaylistInput = "  ";
  assert(!nativeUiCreatePlaylistFromSelection());
  nativeUiCloseMainModal(); assert(g_nativeMainModalKind == NativeMainModalKind::None);
  nativeUiOpenCreatePlaylist(); g_nativeUiCreatePlaylistInput = "sabado"; failSave = true;
  assert(!nativeUiCreatePlaylistFromSelection() && stored.size() == 1);
  nativeUiCloseMainModal(); assert(g_nativeMainModalKind == NativeMainModalKind::None); failSave = false;
  // Add songs to an existing repertoire: success closes; failure stays editable
  // and cancellation leaves the stored songs unchanged.
  g_nativeUiPlaylists = stored; g_nativeUiTargetPlaylistIndex = 0;
  selectedSongs = {{"song-b"}, {"song-c"}};
  g_nativeMainModalKind = NativeMainModalKind::SelectPlaylist;
  assert(nativeUiAddSelectionToPlaylist());
  assert(g_nativeMainModalKind == NativeMainModalKind::None && stored[0].itemLines.size() == 3 && saves == 2);
  g_nativeMainModalKind = NativeMainModalKind::SelectPlaylist; selectedSongs = {{"song-d"}}; failSave = true;
  assert(!nativeUiAddSelectionToPlaylist() && stored[0].itemLines.size() == 3);
  nativeUiCloseMainModal(); assert(g_nativeMainModalKind == NativeMainModalKind::None && saves == 2);
  assert(selectionClears == 2);
  std::cout << "PASS: all modals close; create/add success, duplicate, empty name, save failure and cancellation\n";
}
