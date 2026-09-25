#include <atomic>
#include <cassert>
#include <iostream>

struct ReaProject {};
struct NativeMultiLoopPair { bool valid = false; double start = 0; double end = 0; };
static NativeMultiLoopPair g_nativeMultiLoopActivePair;
static std::atomic<bool> g_nativeForceStateBuild{false};
static bool repeatEnabled;
static double rangeStart, rangeEnd;
static int restored;
static bool nativeIsRepeatEnabled(ReaProject*) { return repeatEnabled; }
static bool nativeGetLoopTimeRange(ReaProject*, double& start, double& end) {
  start = rangeStart; end = rangeEnd; return end > start;
}
static void nativeSetRepeatEnabled(ReaProject*, bool enabled) { repeatEnabled = enabled; }
static void nativeClearLoopTimeRange(ReaProject*) { rangeStart = rangeEnd = 0; }
static void nativeRestoreMultiLoopTracks() { ++restored; }
#include "parts-loop-exit-under-test.h"

static void reset(double start = 10, double end = 20) {
  repeatEnabled = true; rangeStart = start; rangeEnd = end;
  restored = 0; g_nativeMultiLoopActivePair = {}; g_nativeForceStateBuild = false;
}
int main() {
  ReaProject project;
  reset(); nativeReleaseLoopAfterPartsArrival(&project, 30, 30.1);
  assert(!repeatEnabled && rangeStart == 0 && rangeEnd == 0 && g_nativeForceStateBuild);
  reset(); nativeReleaseLoopAfterPartsArrival(&project, 2, 2.3);
  assert(!repeatEnabled);
  reset(); nativeReleaseLoopAfterPartsArrival(&project, 15, 15.1);
  assert(repeatEnabled && rangeStart == 10 && !g_nativeForceStateBuild);
  reset(); nativeReleaseLoopAfterPartsArrival(&project, 20, 20.1);
  assert(!repeatEnabled); // Loop end is exclusive.
  reset(30, 40); g_nativeMultiLoopActivePair = {true, 30, 40};
  nativeReleaseLoopAfterPartsArrival(&project, 30, 30.2);
  assert(repeatEnabled && g_nativeMultiLoopActivePair.valid && restored == 0);
  reset(); g_nativeMultiLoopActivePair = {true, 10, 20};
  nativeReleaseLoopAfterPartsArrival(&project, 30, 30.2);
  assert(!repeatEnabled && !g_nativeMultiLoopActivePair.valid && restored == 1);
  reset(); repeatEnabled = false;
  nativeReleaseLoopAfterPartsArrival(&project, 30, 30.2);
  assert(!g_nativeForceStateBuild && rangeStart == 10);
  reset(0, 0); nativeReleaseLoopAfterPartsArrival(&project, 30, 30.2);
  assert(!repeatEnabled);
  reset(); nativeReleaseLoopAfterPartsArrival(nullptr, 30, 30.2);
  assert(repeatEnabled);
  std::cout << "PARTS_LOOP_EXIT_OK: forward/backward exit, same-loop seek, target multiloop, restoration and inactive repeat\n";
}
