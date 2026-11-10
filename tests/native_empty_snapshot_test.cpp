#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
struct ReaProject { int revision = 0; uint64_t tempo = 0; };
struct NativeSongWindow {};
static int builds = 0, g_nativeMultiLoopPairCacheRevision = 0;
static ReaProject* g_nativeLightweightMutationProject = nullptr;
static int g_nativeLightweightMutationChangeCount = -1;
static int revisionOf(ReaProject* p) { return p ? p->revision : -1; }
static auto GetProjectStateChangeCount_ptr = &revisionOf;
using NativeTempoMarkerBpmValues = uint64_t;
static uint64_t nativeCollectTempoMarkerBpmValues(ReaProject* p) { return p ? p->tempo : 0; }
static uint64_t nativeTempoMarkerBpmValuesSignature(uint64_t v) { return v; }
static void nativeRefreshMixerTimelineCatalog(std::chrono::steady_clock::time_point) {}
#include "snapshot-under-test.h"
int main() {
  assert(snapshotTick(nullptr)); // Even the first empty/no-project state builds once.
  for (int i = 0; i < 100; ++i) assert(!snapshotTick(nullptr));
  ReaProject project, other;
  assert(snapshotTick(&project));
  for (int i = 0; i < 1000; ++i) assert(!snapshotTick(&project));
  assert(builds == 2);
  ++project.revision; // An edit retains the original settling interval.
  assert(!snapshotTick(&project));
  std::this_thread::sleep_for(std::chrono::milliseconds(320));
  assert(snapshotTick(&project));
  assert(!snapshotTick(&project));
  assert(snapshotTick(&project, true)); // Explicit commands remain immediate.
  ++project.tempo;
  assert(snapshotTick(&project)); // Ruler BPM update without revision.
  assert(snapshotTick(&other)); // Same counters, different project.
  assert(snapshotTick(&project));
  ++project.revision;
  g_nativeLightweightMutationProject = &project;
  g_nativeLightweightMutationChangeCount = project.revision;
  assert(!snapshotTick(&project)); // Runtime fades must not cause a full scan.
  std::this_thread::sleep_for(std::chrono::milliseconds(320));
  assert(!snapshotTick(&project));
  std::cout << "EMPTY_SNAPSHOT_REUSE_AND_EDIT_INVALIDATION_OK\n";
}
