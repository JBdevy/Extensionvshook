#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <string>

struct ReaProject {};
struct NativeSongWindow { std::string id; double start = 0, end = 0; };
struct NativePendingSelectionCommand {
  bool pending = false, hasBounds = false, exactPosition = true,
    seekToMarker = false, hasExplicitStart = true, selectionOnly = false, remote = true;
  std::string type = "edit_cursor_move", id;
  double startPos = 123.456, endPos = 0, minPos = 0, maxPos = 0;
  int markerNumber = 0, markerEnumIndex = -1;
};
static NativePendingSelectionCommand g_nativePendingSelection;
static std::mutex g_nativeMutex;
static ReaProject project;
static ReaProject* getCurrentProject(char*, int) { return &project; }
static int playState = 0, writes = 0, redraws = 0;
static double cursor = 60, playback = 87;
static int (*GetPlayStateEx_ptr)(ReaProject*) = [](ReaProject*) { return playState; };
static double (*GetCursorPositionEx_ptr)(ReaProject*) = [](ReaProject*) { return cursor; };
static void (*SetEditCurPos2_ptr)(ReaProject*, double, bool, bool) =
  [](ReaProject*, double pos, bool, bool seek) { assert(!seek); cursor = pos; ++writes; };
static void (*SetEditCurPos_ptr)(double, bool, bool) =
  [](double pos, bool, bool seek) { assert(!seek); cursor = pos; ++writes; };
static void (*UpdateArrange_ptr)() = []() { ++redraws; };
static bool nativeResolveProjectMarkerPosition(ReaProject*, int, int, double&) { return false; }
static const NativeSongWindow* nativeFindSongForCommand(const std::string&, double, double) {
  assert(false && "cursor selection must not resolve or select a song"); return nullptr;
}
static std::string g_nativeSelectedId = "keep-song", g_nativeMixerTimelineRequestedSongId,
  g_nativeSelectedTab = "regions";
static double g_nativeSelectedStart = 60, g_nativeSelectedEnd = 270;
static bool g_nativeSelectedNavigationIntent = false;
static std::atomic<bool> g_nativeForceMixerBuild{false}, g_nativeForceStateBuild{false};
static ReaProject* g_nativeTimecodeLanEditCursorProject = nullptr;
static double g_nativeTimecodeLanEditCursorBaseline = 0;
static bool g_nativeTimecodeLanEditCursorBaselineValid = false;
static ReaProject* g_nativePendingTransportEditCursorProject = nullptr;
static double g_nativePendingTransportEditCursorPosition = 0;
static int g_nativePendingTransportEditCursorRestorePasses = 0;
static std::chrono::steady_clock::time_point g_nativeRemoteSemanticSeekSuppressUntil;
#include "cursor-under-test.h"

int main() {
  // Receiver stopped: exact position, no playback seek, no song-selection change.
  g_nativePendingSelection.pending = true;
  nativeProcessPendingSelectionOnMainThread();
  assert(writes == 1 && redraws == 1 && cursor == 123.456 && playback == 87);
  assert(g_nativeSelectedId == "keep-song" && g_nativeSelectedStart == 60);
  assert(g_nativeTimecodeLanEditCursorBaseline == cursor);
  nativeProcessPendingSelectionOnMainThread();
  assert(writes == 1); // consumed once
  // A may be stopped while B is still playing: discard instead of saving for Stop.
  for (int state : {1, 4, 5}) {
    playState = state;
    g_nativePendingSelection = NativePendingSelectionCommand{};
    g_nativePendingSelection.pending = true;
    g_nativePendingSelection.startPos = 200;
    nativeProcessPendingSelectionOnMainThread();
    assert(writes == 1 && cursor == 123.456 && playback == 87);
  }
  playState = 0;
  nativeProcessPendingSelectionOnMainThread();
  assert(writes == 1 && cursor == 123.456);
  // Older hosts use the fallback API with the same no-seek contract.
  SetEditCurPos2_ptr = nullptr;
  g_nativePendingSelection = NativePendingSelectionCommand{};
  g_nativePendingSelection.pending = true;
  g_nativePendingSelection.startPos = 150.25;
  nativeProcessPendingSelectionOnMainThread();
  assert(writes == 2 && cursor == 150.25 && playback == 87);
  std::cout << "PROJECT_SYNC_STOPPED_CURSOR_OK: exact position, no playback seek, no song selection, no delayed replay\n";
}
