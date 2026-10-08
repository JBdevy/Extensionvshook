#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
struct ReaProject {};
#include "stop-types.h"
static std::mutex g_nativeMutex;
static NativePendingPlayTarget g_nativePendingPlayTarget;
static uint64_t g_nativePendingPlayTargetRevision;
static ReaProject* g_nativeAutoStopLastPlayingProject;
static std::string g_nativeAutoStopLastStoppedSignature, g_nativeAutoStopLastPlayingId;
static double g_nativeAutoStopLastPlayingStart, g_nativeAutoStopLastPlayingEnd;
static std::string g_nativeArmedMarkerId, g_nativeAutoBlocoTargetSongId, g_nativeQueuedSongId;
static std::string g_nativeSelectedId, g_nativeSelectedTab;
static double g_nativeAutoBlocoTargetStart, g_nativeAutoBlocoTargetEnd;
static double g_nativeQueuedStart, g_nativeQueuedEnd, g_nativeSelectedStart, g_nativeSelectedEnd;
static bool g_nativeAutoStopEnabled = true, g_nativeQueuedManual, g_nativeSelectedNavigationIntent;
static bool g_nativeUiMusicSelectionVisualActive, g_nativeUiMainSelectionAuthoritative, g_transportCommandBypass;
struct { bool regionsPage = false; } g_nativeAppActivePanelModel;
struct UiRow { bool block = false; std::string id; double start = 0, end = 0; };
static UiRow uiRow;
static const UiRow* nativeUiCurrentMainSelectedSongRow() { return &uiRow; }
static void nativeLoadAutomationSettingsOnceLocked() {}
static bool repeatEnabled;
static bool nativeIsRepeatEnabled(ReaProject*) { return repeatEnabled; }
static bool nativeSongIsPlayable(const NativeSongWindow& song) { return !song.isBlock && song.end > song.start + 0.0005; }
static std::string nativeNumber(double value, int digits) { std::ostringstream out; out << std::fixed << std::setprecision(digits) << value; return out.str(); }
static void command(int, int) {}
static auto Main_OnCommand_ptr = command;
static int stops;
static std::string finalSelection;
static bool nativeStopTransportAndPrepareExplicitSelection(ReaProject*, const std::string& id, const std::string&, double, double, bool, bool = false) {
  ++stops; finalSelection = id; return true;
}
static const NativeSongWindow* nativeResolveAutoStopTarget(const std::vector<NativeSongWindow>&, const std::string&, double, double) { return nullptr; }
#include "stop-resolver.h"
#include "stop-runtime.h"
static NativeSongWindow song(const char* id, double start, double end) {
  NativeSongWindow result; result.id = result.name = id; result.start = start; result.end = end; return result;
}
static void reset() {
  nativeClearPendingPlayTarget(); g_nativeAutoStopLastPlayingProject = nullptr;
  g_nativeAutoStopLastPlayingId.clear(); g_nativeAutoStopLastStoppedSignature.clear();
  g_nativeAutoStopLastPlayingStart = g_nativeAutoStopLastPlayingEnd = 0;
  g_nativeArmedMarkerId.clear(); g_nativeAutoBlocoTargetSongId.clear(); g_nativeQueuedSongId.clear();
  g_nativeSelectedId.clear(); g_nativeSelectedNavigationIntent = false;
  g_nativeAutoStopEnabled = true; repeatEnabled = g_nativeQueuedManual = false;
  g_nativeUiMusicSelectionVisualActive = g_nativeUiMainSelectionAuthoritative = false;
  stops = 0; finalSelection.clear();
}
static void tick(ReaProject* project, const std::vector<NativeSongWindow>& songs, double position, bool playing = true) {
  std::string name; double start = 0, end = 0;
  const std::string id = nativeFindPlayingIdWithPendingTarget(project, songs, playing, position, name, start, end);
  nativeMaintainAutoStop(project, playing, id, position, start, end, {}, songs);
}
int main() {
  ReaProject project, otherProject;
  const std::vector<NativeSongWindow> songs{song("a", 10, 20), song("b", 40, 60)};
  reset(); tick(&project, songs, 19.95); nativeArmPendingPlayTarget(&project, songs[1]);
  tick(&project, songs, 20.02);
  if (stops) { std::cerr << "REGRESSION: starting B in a gap stopped at A's stale end position\n"; return 1; }
  assert(g_nativePendingPlayTarget.active);
  tick(&project, songs, 40.01); assert(!g_nativePendingPlayTarget.active && stops == 0);
  tick(&project, songs, 59.99); assert(stops == 0);
  tick(&project, songs, 60.01); assert(stops == 1);
  tick(&project, songs, 60.02); assert(stops == 1);
  // Stale reads may still be inside A for more than one timer tick.
  reset(); tick(&project, songs, 19.90); nativeArmPendingPlayTarget(&project, songs[1]);
  tick(&project, songs, 19.98); tick(&project, songs, 20.10); assert(stops == 0);
  tick(&project, songs, 40.02); assert(stops == 0 && !g_nativePendingPlayTarget.active);
  // Backward launches have the same latency and must preserve the new target.
  reset(); tick(&project, songs, 59.95); nativeArmPendingPlayTarget(&project, songs[0]);
  tick(&project, songs, 60.01); assert(stops == 0);
  tick(&project, songs, 10.02); assert(stops == 0);
  // A stopped playback and another project cannot donate a stale end boundary.
  reset(); tick(&project, songs, 19.9); tick(&project, songs, 19.9, false);
  tick(&project, songs, 20.02); assert(stops == 0);
  reset(); tick(&project, songs, 19.9); tick(&otherProject, {}, 20.02); assert(stops == 0);
  // Protection has a bounded timeout; it does not permanently disable Stop.
  reset(); nativeArmPendingPlayTarget(&project, songs[1]);
  g_nativePendingPlayTarget.requestedAt -= std::chrono::milliseconds(600);
  tick(&project, songs, 19.9); assert(!g_nativePendingPlayTarget.active);
  tick(&project, songs, 20.01); assert(stops == 1);
  reset(); nativeArmPendingPlayTarget(&project, songs[1]);
  tick(&project, {songs[0]}, 19.9); assert(!g_nativePendingPlayTarget.active);
  tick(&project, {songs[0]}, 20.01); assert(stops == 1);
  reset(); nativeArmPendingPlayTarget(&project, songs[1]);
  tick(&otherProject, songs, 19.9); assert(!g_nativePendingPlayTarget.active);
  tick(&otherProject, songs, 20.01); assert(stops == 1);
  reset(); nativeArmPendingPlayTarget(&project, songs[1]);
  nativeClearPendingPlayTarget();
  assert(!g_nativePendingPlayTarget.active);
  tick(&project, songs, 59.9); tick(&project, songs, 60); assert(stops == 1);
  // Adjacent songs remain continuous and their shared point belongs to B.
  const std::vector<NativeSongWindow> adjacent{song("a", 10, 20), song("b", 20, 40)};
  reset(); tick(&project, adjacent, 19.99); nativeArmPendingPlayTarget(&project, adjacent[1]);
  tick(&project, adjacent, 19.999); assert(stops == 0);
  tick(&project, adjacent, 20); assert(stops == 0 && !g_nativePendingPlayTarget.active);
  tick(&project, adjacent, 39.9); tick(&project, adjacent, 40); assert(stops == 1);
  // Children use their enclosing song/block end, never a child boundary.
  std::vector<NativeSongWindow> family{song("parent", 10, 60), song("c1", 10, 30), song("c2", 30, 60)};
  family[0].isHashParent = true;
  for (size_t index = 1; index < family.size(); ++index) {
    family[index].isHashChild = true; family[index].parentId = "parent";
  }
  reset(); tick(&project, family, 29.99); tick(&project, family, 30);
  assert(stops == 0);
  tick(&project, family, 59.99); tick(&project, family, 60.01); assert(stops == 1);
  // Repeat/Parts/manual queue retain precedence over AutoStop.
  for (int mode = 0; mode < 4; ++mode) {
    reset(); tick(&project, songs, 19.9);
    if (mode == 0) repeatEnabled = true;
    if (mode == 1) g_nativeArmedMarkerId = "part";
    if (mode == 2) { g_nativeQueuedManual = true; g_nativeQueuedSongId = "b"; g_nativeQueuedStart = 40; g_nativeQueuedEnd = 60; }
    if (mode == 3) g_nativeAutoStopEnabled = false;
    tick(&project, songs, 20.01); assert(stops == 0);
  }
  reset(); tick(&project, songs, 19.9);
  g_nativeSelectedNavigationIntent = true; g_nativeSelectedId = "b"; g_nativeSelectedTab = "playlist";
  g_nativeSelectedStart = 40; g_nativeSelectedEnd = 60;
  tick(&project, songs, 20.01); assert(stops == 1 && finalSelection == "b");
  reset(); tick(&project, songs, 19.9); g_nativeAutoStopEnabled = false;
  g_nativeAutoBlocoTargetSongId = "b"; g_nativeAutoBlocoTargetStart = 40; g_nativeAutoBlocoTargetEnd = 60;
  tick(&project, songs, 20.01); assert(stops == 1); // AT/BL owns its stop even when global Stop is off.
  std::cout << "AUTO_STOP_START_OK: delayed forward/backward starts, real ends, stop/restart, project changes, timeout, adjacent songs, repeat, Parts, queue and selection\n";
}
