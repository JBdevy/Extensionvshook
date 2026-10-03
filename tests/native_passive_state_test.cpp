#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

struct ReaProject {};
static ReaProject project;
static ReaProject* getCurrentProject(char*, int) { return &project; }
static int getPlayState(ReaProject*) { return 0; }
static auto GetPlayStateEx_ptr = &getPlayState;
static bool panel = false, director = false;
static std::string mode, projectSignature = "project-one", standbyDiscoveryProjectSignature;
static std::string getTimecodeMode() { return mode; }
static std::string getParallelTimecodeMode() { return {}; }
static bool nativeAppActivePanelIsOpen() { return panel; }
static bool nativeHookControllerWindowIsOpen() { return false; }
static bool nativeTelepromptWindowIsOpen(int) { return false; }
static bool nativeVideoWindowIsOpen() { return false; }
static bool nativeBigClockWindowIsOpen(int) { return false; }
static bool nativeIsDirectorControlActive() { return director; }
static std::atomic<bool> g_nativeExtensionBypassActive{false};
static std::atomic<bool> g_nativeForceStateBuild{false}, g_nativeForceSnapshotBuild{false};
static bool g_nativeRuntimeWasActive = false, standbyDiscoveryPublished = false;
static bool g_nativeTimecodeLanPeerConnected = false, g_nativeTimerRunning = false;
static std::mutex g_nativeMutex, g_nativeTimecodeLanMutex, g_nativeManualStopFadeoutMutex;
static struct { bool active = false, restorePending = false; } g_nativeManualStopFadeoutRuntime;
static struct { int startupTimerTicks = 0, projectStableTicks = 2; } g_state;
static constexpr int kNativeBridgeLiveIntervalMs = 220;
static std::chrono::steady_clock::time_point g_nativeLastLiveBuild;
static int g_nativeUiHeldNavigationKey = 0;
static int activeBuilds = 0, passiveBuilds = 0, controlWork = 0, releases = 0, standbyBuilds = 0;
static bool lastForced = false, publishedEmpty = true;
static void nativeRebuildState(bool force, bool allowRuntimeControl = true) {
  if (allowRuntimeControl) ++activeBuilds; else ++passiveBuilds;
  lastForced = force;
  publishedEmpty = false;
}
static void nativePublishStandbyDiscoveryState() { ++standbyBuilds; publishedEmpty = true; }
static void nativeRefreshStopPauseModeFromExtState() {}
static void nativeProcessGlobalStopBreakOnMainThread() { ++controlWork; }
static void nativeProcessGlobalStopPauseOnMainThread() { ++controlWork; }
static void nativeUpdateManualStopFadeoutOnMainThread() { ++controlWork; }
static void nativeProcessPendingSelectionOnMainThread() { ++controlWork; }
static void nativeUiPollBatteryState() {}
static void nativeProcessMultiLoopsOnMainThread() { ++controlWork; }
static void nativePrepareQueuedTunerNearRegionEndOnMainThread() { ++controlWork; }
static void nativeRefreshAppActivePanelModel() {}
static void processPcResumeRequestFromAppActiveOnMainThread() {}
static void processAppActiveScreenRequestsOnMainThread() {}
static void nativeReleaseRuntimeControlOnMainThread() { ++releases; }
static void nativeLoadFamilyDrawersForCurrentProject(const std::string&) {}

#include "passive-state-under-test.h"

// Execute the real guarded playback sections of nativeRebuildState against
// mutation probes. Returning songs must not execute any pending automation or
// consume the transition a later Director session needs.
struct NativeSongWindow { bool isBlock = false, isHashParent = false; std::string id = "song"; };
static bool g_nativeCurrentTransportPlaying = true;
static std::string g_nativeCurrentPlayingId = "previous";
static double g_nativeCurrentSongStart = -1, g_nativeCurrentSongEnd = -1, g_nativeCurrentPlayPosition = -1;
static std::atomic<bool> g_nativeUiPanelModelSourceDirty{false};
static std::chrono::steady_clock::time_point g_nativeSuppressStoppedTransitionPrepareUntil;
static std::string g_nativeQueuedSongId = "queued", g_nativeAutoBlocoTargetSongId;
static double g_nativeQueuedStart = 0, g_nativeQueuedEnd = 20;
static double g_nativeAutoBlocoTargetStart = 0, g_nativeAutoBlocoTargetEnd = 0;
static int mutationCalls = 0, stopPrepares = 0;
#define MUTATION_PROBE(name) template<class... Args> static bool name(Args&&...) { ++mutationCalls; return false; }
MUTATION_PROBE(nativeProcessTunerCommands)
MUTATION_PROBE(nativeUpdateLiveMarkTracker)
MUTATION_PROBE(nativeApplyPremixOnSongStart)
MUTATION_PROBE(nativeApplyTunerValue)
MUTATION_PROBE(nativeScheduleTunerRetry)
MUTATION_PROBE(nativeProcessTunerRetry)
MUTATION_PROBE(nativeMaintainQueueAutomation)
MUTATION_PROBE(nativeMaintainAutoStop)
MUTATION_PROBE(nativeReleaseLoopAfterPartsArrival)
MUTATION_PROBE(nativeTimerObservePlaybackLocked)
#undef MUTATION_PROBE
static void nativePrepareStopSelectionFromQueueOrCurrent(ReaProject*, bool) { ++mutationCalls; ++stopPrepares; }
static int nativeTunerValueForSong(ReaProject*, const NativeSongWindow&) { return 0; }
static void snapshotPlaybackEffects(bool allowRuntimeControl, bool playing) {
  auto* activeProject = &project;
  std::vector<NativeSongWindow> songs(1), cachedSongs(1), activePlaylistItems(1);
  std::string playingId = playing ? "song" : "";
  const double playPos = 10, songStart = 0, songEnd = 20, partsArrivalTarget = 5;
  const bool paused = false, partsArrived = true;
#include "passive-effects-under-test.h"
}
static void testPassiveEffects() {
  g_nativeForceStateBuild = false;
  snapshotPlaybackEffects(false, true);
  snapshotPlaybackEffects(false, false);
  assert(mutationCalls == 0 && !g_nativeForceStateBuild);
  assert(g_nativeCurrentTransportPlaying && g_nativeCurrentPlayingId == "previous");
  assert(g_nativeCurrentSongStart == -1 && g_nativeCurrentSongEnd == -1 && g_nativeCurrentPlayPosition == -1);
  assert(!g_nativeUiPanelModelSourceDirty);
  snapshotPlaybackEffects(true, true);
  assert(mutationCalls == 10 && g_nativeForceStateBuild);
  assert(g_nativeCurrentPlayingId == "song" && g_nativeUiPanelModelSourceDirty);
  snapshotPlaybackEffects(true, false);
  assert(stopPrepares == 1);
  std::cout << "PASSIVE_SNAPSHOT_HAS_ZERO_TRANSPORT_TUNER_PREMIX_LOOP_TIMER_EFFECTS_OK\n";
}

static void due() {
  g_nativeLastLiveBuild = std::chrono::steady_clock::now() - std::chrono::milliseconds(1100);
}
static void expireReader() {
  g_nativeLastStateClientReadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count() - 13000;
}

int main() {
  for (int i = 0; i < 100; ++i) { due(); runtimeTimerBody(); }
  assert(activeBuilds == 0 && passiveBuilds == 0 && controlWork == 0 && standbyBuilds == 1);
  stateRequest("GET /state HTTP/1.1\r\n\r\n");
  assert(nativeBridgeStateClientActive() && !nativeIsRuntimeControlActive());
  runtimeTimerBody();
  assert(passiveBuilds == 1 && activeBuilds == 0 && controlWork == 0 && !publishedEmpty);
  // Frequent polling coalesces; it must not force one rebuild per HTTP request.
  for (int i = 0; i < 100; ++i) {
    stateRequest("GET /state HTTP/1.1\r\n\r\n"); runtimeTimerBody();
  }
  assert(passiveBuilds == 1);
  due(); runtimeTimerBody();
  assert(passiveBuilds == 2 && controlWork == 0);
  std::cout << "PASSIVE_READER_PUBLISHES_WITHOUT_RUNTIME_CONTROL_OK\n";

  // Project changes publish a new non-empty snapshot without a standby frame
  // overwriting it in the same timer cycle.
  projectSignature = "project-two"; due(); runtimeTimerBody();
  assert(passiveBuilds == 3 && !publishedEmpty);
  director = true; runtimeTimerBody();
  assert(activeBuilds == 1 && controlWork > 0 && lastForced && g_nativeRuntimeWasActive);
  const int previousControlWork = controlWork;
  director = false; due(); runtimeTimerBody();
  assert(releases == 1 && passiveBuilds == 4 && !g_nativeRuntimeWasActive);
  assert(controlWork == previousControlWork && !publishedEmpty);
  expireReader(); due(); runtimeTimerBody();
  assert(publishedEmpty && standbyBuilds == 2);
  const int previousPassiveBuilds = passiveBuilds;
  for (int i = 0; i < 100; ++i) { due(); runtimeTimerBody(); }
  assert(passiveBuilds == previousPassiveBuilds && activeBuilds == 1);
  assert(standbyBuilds == 2 && releases == 1);
  std::cout << "DIRECTOR_ENTER_EXIT_AND_READER_EXPIRATION_OK\n";

  // Hook Center discovers REAPER in the background, including with no phone.
  g_nativeLastStateClientReadMs = 0;
  g_nativeForceStateBuild = false;
  for (int i = 0; i < 100; ++i) {
    stateRequest("GET /state?discovery=1 HTTP/1.1\r\n\r\n"); due(); runtimeTimerBody();
  }
  assert(!nativeBridgeStateClientActive() && !g_nativeForceStateBuild);
  assert(passiveBuilds == previousPassiveBuilds && activeBuilds == 1 && standbyBuilds == 2);
  stateRequest("GET /state?cacheBust=1&discovery=%31 HTTP/1.1\r\n\r\n");
  assert(!nativeBridgeStateClientActive());
  stateRequest("GET /state?discovery=0 HTTP/1.1\r\n\r\n");
  runtimeTimerBody();
  assert(passiveBuilds == previousPassiveBuilds + 1 && !publishedEmpty);
  std::cout << "DISCOVERY_HAS_ZERO_SNAPSHOT_WORK_AND_RECONNECT_WAKES_OK\n";

  // Existing local and Project Sync runtime ownership remain unchanged.
  expireReader(); mode = "project_sync"; runtimeTimerBody();
  assert(activeBuilds == 2 && g_nativeRuntimeWasActive);
  mode.clear(); panel = true; due(); runtimeTimerBody();
  assert(activeBuilds == 3);
  std::cout << "EXISTING_PANEL_AND_PROJECT_SYNC_RUNTIME_PRESERVED_OK\n";
  testPassiveEffects();
}
