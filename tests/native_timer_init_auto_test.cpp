#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

struct ReaProject {};
static bool g_nativeTimerRunning = false;
static bool g_nativeTimerInitAutoEnabled = false;
static ReaProject* g_nativeTimerObservedProject = nullptr;
static std::string g_nativeTimerObservedSongId;
static std::string g_nativeTimerMode = "progressive";
static double g_nativeTimerBaseSec = 0.0;
static double g_nativeTimerTargetSec = 0.0;
static bool g_nativeTimerResetToZero = false;
static std::chrono::steady_clock::time_point g_nativeTimerStartedAtSteady;
static std::chrono::system_clock::time_point g_nativeTimerStartedAtSystem;

static double nativeTimerDisplaySecLocked() { return g_nativeTimerBaseSec; }

#include "timer-under-test.h"

int main()
{
  ReaProject first, second;
  nativeTimerObservePlaybackLocked(&first, true, false, "song-a");
  assert(!g_nativeTimerRunning);
  g_nativeTimerInitAutoEnabled = true;
  nativeTimerObservePlaybackLocked(&first, true, false, "song-a");
  assert(!g_nativeTimerRunning); // Enabling during a song waits for a new Play.
  nativeTimerObservePlaybackLocked(&first, false, false, "");
  nativeTimerObservePlaybackLocked(&first, true, false, "song-a");
  assert(g_nativeTimerRunning);
  assert(g_nativeTimerBaseSec == 0.0);
  const auto started = g_nativeTimerStartedAtSteady;
  nativeTimerObservePlaybackLocked(&first, true, false, "song-b");
  assert(g_nativeTimerStartedAtSteady == started); // Never restart a running timer.
  nativeTimerStopLocked();
  nativeTimerObservePlaybackLocked(&first, true, false, "song-b");
  assert(!g_nativeTimerRunning); // Manual stop stays stopped for this song.
  nativeTimerObservePlaybackLocked(&first, false, true, "song-b");
  nativeTimerObservePlaybackLocked(&first, true, false, "song-b");
  assert(!g_nativeTimerRunning); // Pause/resume is not a new song.
  nativeTimerObservePlaybackLocked(&first, true, false, "song-c");
  assert(g_nativeTimerRunning); // Starting another song starts it again.
  nativeTimerResetLocked();
  nativeTimerObservePlaybackLocked(&first, false, false, "");
  g_nativeTimerMode = "countdown";
  g_nativeTimerTargetSec = 900.0;
  nativeTimerObservePlaybackLocked(&first, true, false, "song-d");
  assert(g_nativeTimerRunning);
  assert(g_nativeTimerBaseSec == 900.0); // Configured countdown survives reset.
  nativeTimerStopLocked();
  g_nativeTimerInitAutoEnabled = false;
  nativeTimerObservePlaybackLocked(&first, true, false, "song-e");
  assert(!g_nativeTimerRunning);
  g_nativeTimerInitAutoEnabled = true;
  nativeTimerObservePlaybackLocked(&first, true, false, "song-e");
  assert(!g_nativeTimerRunning);
  nativeTimerObservePlaybackLocked(&second, true, false, "song-e");
  assert(g_nativeTimerRunning); // Equal song IDs in another project still start.
  nativeTimerResetLocked();
  nativeTimerObservePlaybackLocked(&first, false, false, "selected-song");
  nativeTimerObservePlaybackLocked(&first, true, false, "");
  nativeTimerObservePlaybackLocked(nullptr, true, false, "song");
  assert(!g_nativeTimerRunning); // Selection/no song is not playback.
  nativeTimerObservePlaybackLocked(&first, true, false, "song-final");
  assert(g_nativeTimerRunning);
  std::cout << "NATIVE_TIMER_INIT_AUTO_OK\n";
}

