#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using HWND = int;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
static constexpr int FALSE = 0;
static constexpr int VK_UP = 38, VK_DOWN = 40, WM_KEYDOWN = 256;
static constexpr unsigned kNativeUiFrameFastIntervalMs = 16;
static constexpr int kNativeUiNavigationRepeatDelayMs = 220;
static constexpr int kNativeUiNavigationRepeatIntervalMs = 40;
static bool g_nativeUiSyntheticNavigationRepeat = false;
static int g_nativeUiHeldNavigationKey = 0;
static std::chrono::steady_clock::time_point g_nativeUiNextNavigationRepeatAt;
static struct { bool directorInterfaceBlocked = false; } g_state;
static bool g_nativeUiPlaylistStepKeyHeld[2] = {};
static bool macArrowHeld[2] = {};
static bool VSHookMacIsArrowKeyPressed(int direction) {
  return macArrowHeld[direction > 0 ? 1 : 0];
}
struct NativeUiSmoothScrollState {
  int target = -1;
  int lastApplied = -1;
};
static NativeUiSmoothScrollState g_nativeMainSmoothScroll;
static std::chrono::steady_clock::time_point g_nativeUiLastSmoothScrollAdvanceAt;
struct NativeAppActivePanelModel {
  struct Row {
    std::string id;
    double start = 0, end = 1;
    bool block = false;
  };
  bool mixerPage = false, regionsPage = false, queueActive = false, playing = false;
  std::string playingId, queuedId;
  double playingStart = 0, playingEnd = 1, queuedStart = 0, queuedEnd = 1;
  std::vector<Row> rows;
};
static NativeAppActivePanelModel g_nativeAppActivePanelModel;
static std::vector<int> g_nativeMainVisibleRowIndices, g_nativeMainRowOffsets, g_nativeMainRowHeights;
static int selectedIndex = -1, scheduledIndex = -1, scheduleCount = 0;
static bool scheduledRegions = false;
static bool g_nativeUiMusicSelectionVisualActive = false;
static int g_nativeUiSelectionAnchorVisibleIndex = -1, g_nativeUiSelectionFocusVisibleIndex = -1;
static std::string g_nativeMainNavigationFocus;
static int g_nativeAppActiveListScrollPixels = 0;
static int g_nativeMainListViewportPixels = 100, g_nativeMainListMaxScrollPixels = 0;
struct NativeUiListLayout { int singleRowHeight = 20; };
static int nativeUiVisualPrefsForPaint() { return 0; }
static NativeUiListLayout nativeUiCurrentListLayout(int) { return {}; }
static const NativeAppActivePanelModel::Row* nativeUiMainVisibleRowAt(int index) {
  if (index < 0 || index >= static_cast<int>(g_nativeMainVisibleRowIndices.size())) return nullptr;
  return &g_nativeAppActivePanelModel.rows[g_nativeMainVisibleRowIndices[index]];
}
static const NativeAppActivePanelModel::Row* nativeUiCurrentMainSelectedSongRow() {
  return nativeUiMainVisibleRowAt(selectedIndex);
}
static bool nativeAppActiveRowMatches(const NativeAppActivePanelModel::Row& row,
    const std::string& id, double start, double end) {
  return !id.empty() && row.id == id && row.start == start && row.end == end;
}
static void nativeUiSelectOnlyMainRow(const NativeAppActivePanelModel::Row& row, bool) {
  selectedIndex = static_cast<int>(&row - g_nativeAppActivePanelModel.rows.data());
}
static void nativeUiScheduleMusicNavigation(const NativeAppActivePanelModel::Row& row, bool regions) {
  scheduledIndex = static_cast<int>(&row - g_nativeAppActivePanelModel.rows.data());
  scheduledRegions = regions;
  ++scheduleCount;
}
static int invalidations = 0, paints = 0, scrollAdvances = 0, timerUpdates = 0;
static bool IsWindow(HWND hwnd) { return hwnd == 1; }
static void nativeUiSetFrameTimerInterval(HWND, unsigned interval) {
  assert(interval == kNativeUiFrameFastIntervalMs);
  ++timerUpdates;
}
static void nativeUiInvalidateRect(HWND, const void*, int) { ++invalidations; }
static void UpdateWindow(HWND) { ++paints; }
static bool nativeUiAdvanceSmoothScrolls() { ++scrollAdvances; return true; }
static void nativeUiResetNavigationRepeat(int releasedKey = 0);
static int deliveredRepeats = 0;
static void SendMessage(HWND, int, WPARAM, LPARAM);

// Exercise both production platform branches with host/window calls mocked.
// Standard-library headers above retain the compiler's actual target platform.
#if defined(NAVIGATION_TEST_MAC)
#undef _WIN32
#define __APPLE__ 1
#else
#ifndef _WIN32
#define _WIN32 1
#endif
#endif
#include "navigation-cadence-under-test.h"

static void SendMessage(HWND hwnd, int message, WPARAM key, LPARAM keyData) {
  assert(message == WM_KEYDOWN && g_nativeUiSyntheticNavigationRepeat);
  ++deliveredRepeats;
  assert(!nativeUiPrepareNavigationKeyDown(hwnd, key));
  nativeNavigateMainRows(key == VK_UP ? -1 : 1,
    nativeUiNavigationKeyIsRepeat(keyData));
  nativeUiRefreshNavigationNow(hwnd);
}

static void reset(int count = 12) {
  g_nativeAppActivePanelModel = {};
  g_nativeMainVisibleRowIndices.clear();
  g_nativeMainRowOffsets.clear();
  g_nativeMainRowHeights.clear();
  for (int i = 0; i < count; ++i) {
    g_nativeAppActivePanelModel.rows.push_back({std::to_string(i), 0, 1, false});
    g_nativeMainVisibleRowIndices.push_back(i);
    g_nativeMainRowOffsets.push_back(i * 20);
    g_nativeMainRowHeights.push_back(20);
  }
  g_nativeMainRowOffsets.push_back(count * 20);
  g_nativeAppActiveListScrollPixels = 0;
  g_nativeMainListViewportPixels = 100;
  g_nativeMainListMaxScrollPixels = std::max(0, count * 20 + 60 - 100);
  g_nativeMainSmoothScroll = {};
  g_nativeUiSyntheticNavigationRepeat = false;
  g_nativeUiHeldNavigationKey = 0;
  g_nativeUiNextNavigationRepeatAt = {};
  g_state.directorInterfaceBlocked = false;
  g_nativeUiPlaylistStepKeyHeld[0] = g_nativeUiPlaylistStepKeyHeld[1] = false;
  macArrowHeld[0] = macArrowHeld[1] = false;
  deliveredRepeats = 0;
  selectedIndex = scheduledIndex = -1;
  scheduleCount = 0;
  scheduledRegions = false;
  g_nativeUiMusicSelectionVisualActive = false;
  g_nativeMainNavigationFocus.clear();
}

static void animationFrame() {
  assert(nativeUiAdvanceSmoothScroll(g_nativeAppActiveListScrollPixels,
    g_nativeMainListMaxScrollPixels, g_nativeMainSmoothScroll, 67));
}

static void heldRepeatRetargetsBetweenFrames() {
  reset();
  selectedIndex = 3;
  g_nativeUiSyntheticNavigationRepeat = true;
  assert(nativeNavigateMainRows(1));
  assert(selectedIndex == 4 && scheduledIndex == 4 && scheduleCount == 1);
  assert(g_nativeUiMusicSelectionVisualActive);
  assert(g_nativeUiSelectionAnchorVisibleIndex == 4 &&
    g_nativeUiSelectionFocusVisibleIndex == 4);
  assert(g_nativeMainNavigationFocus == "playlist");
  assert(g_nativeMainSmoothScroll.target == 40);
  // Selection updates immediately, while the viewport moves on animation frames.
  assert(g_nativeAppActiveListScrollPixels == 0);
  animationFrame();
  const int firstFrame = g_nativeAppActiveListScrollPixels;
  assert(firstFrame > 0 && firstFrame < 40);
  assert(nativeNavigateMainRows(1));
  assert(selectedIndex == 5 && scheduledIndex == 5 && scheduleCount == 2);
  assert(g_nativeAppActiveListScrollPixels == firstFrame);
  assert(g_nativeMainSmoothScroll.target == 60);
  animationFrame();
  assert(g_nativeAppActiveListScrollPixels > firstFrame &&
    g_nativeAppActiveListScrollPixels < 60);
  int frames = 0;
  while (g_nativeMainSmoothScroll.target >= 0 && ++frames < 100) animationFrame();
  assert(frames < 100 && g_nativeAppActiveListScrollPixels == 60);
}

static void directionChangeCancelsForwardMotion() {
  reset();
  selectedIndex = 4;
  g_nativeAppActiveListScrollPixels = 45;
  g_nativeMainSmoothScroll = {80, 45};
  g_nativeUiSyntheticNavigationRepeat = true;
  assert(nativeNavigateMainRows(-1));
  assert(selectedIndex == 3);
  assert(g_nativeMainSmoothScroll.target == 40);
  assert(g_nativeAppActiveListScrollPixels == 45);
  animationFrame();
  assert(g_nativeAppActiveListScrollPixels < 45 &&
    g_nativeAppActiveListScrollPixels >= 40);
}

static void holdStopsAtEndsButNewPressWraps() {
  reset();
  g_nativeAppActivePanelModel.rows[0].block = true;
  selectedIndex = 11;
  g_nativeAppActiveListScrollPixels = 140;
  g_nativeUiSyntheticNavigationRepeat = true;
  assert(!nativeNavigateMainRows(1));
  assert(selectedIndex == 11 && scheduleCount == 0);
  assert(g_nativeAppActiveListScrollPixels == 140);
  g_nativeUiSyntheticNavigationRepeat = false;
  assert(nativeNavigateMainRows(1));
  assert(selectedIndex == 1 && scheduledIndex == 1);
  assert(g_nativeAppActiveListScrollPixels == 0 && g_nativeMainSmoothScroll.target < 0);
  g_nativeUiSyntheticNavigationRepeat = true;
  assert(!nativeNavigateMainRows(-1));
  assert(selectedIndex == 1);
  g_nativeUiSyntheticNavigationRepeat = false;
  assert(nativeNavigateMainRows(-1));
  assert(selectedIndex == 11);
  assert(g_nativeAppActiveListScrollPixels == 140 && g_nativeMainSmoothScroll.target < 0);
}

static void skipsBlockedAndPlayingRowsAndRetainsQueueAnchor() {
  reset();
  selectedIndex = 2;
  g_nativeAppActivePanelModel.rows[3].block = true;
  g_nativeAppActivePanelModel.playing = true;
  g_nativeAppActivePanelModel.playingId = "4";
  g_nativeAppActivePanelModel.regionsPage = true;
  g_nativeUiSyntheticNavigationRepeat = true;
  assert(nativeNavigateMainRows(1));
  assert(selectedIndex == 5 && scheduledIndex == 5 && scheduledRegions);
  assert(g_nativeMainNavigationFocus == "regions");
  reset();
  selectedIndex = 2;
  g_nativeAppActivePanelModel.playing = true;
  g_nativeAppActivePanelModel.playingId = "2";
  g_nativeAppActivePanelModel.queueActive = true;
  g_nativeAppActivePanelModel.queuedId = "5";
  assert(nativeNavigateMainRows(1));
  assert(selectedIndex == 6);
}

static void syntheticRefreshDefersPaintToTimer() {
  reset();
  invalidations = paints = scrollAdvances = timerUpdates = 0;
  g_nativeUiSyntheticNavigationRepeat = true;
  for (int i = 0; i < 5; ++i) nativeUiRefreshNavigationNow(1);
  assert(invalidations == 5 && timerUpdates == 5);
  assert(paints == 0 && scrollAdvances == 0);
  // A fresh physical press still has immediate feedback.
  g_nativeUiSyntheticNavigationRepeat = false;
  nativeUiRefreshNavigationNow(1);
  assert(paints == 1 && scrollAdvances == 1);
  nativeUiRefreshNavigationNow(0);
  assert(invalidations == 6 && timerUpdates == 6 && paints == 1);
}

static void platformRepeatCadence() {
  reset(50);
  selectedIndex = 0;
  const LPARAM firstDown = 1;
  const LPARAM heldDown = (static_cast<LPARAM>(1) << 30) | 1;
  assert(!nativeUiPrepareNavigationKeyDown(1, VK_DOWN));
  assert(g_nativeUiHeldNavigationKey == VK_DOWN);
  assert(!nativeUiNavigationKeyIsRepeat(firstDown));
  assert(nativeNavigateMainRows(1, nativeUiNavigationKeyIsRepeat(firstDown)));
  assert(selectedIndex == 1);
#ifdef _WIN32
  assert(g_nativeUiNextNavigationRepeatAt.time_since_epoch().count() == 0);
  // Each OS repeat advances one item immediately, independent of timer delivery.
  // Thirty native events must not be reduced to the custom timer's 25 steps/s.
  for (int repeat = 0; repeat < 30; ++repeat) {
    assert(!nativeUiPrepareNavigationKeyDown(1, VK_DOWN));
    assert(nativeUiNavigationKeyIsRepeat(heldDown));
    assert(nativeNavigateMainRows(1, nativeUiNavigationKeyIsRepeat(heldDown)));
    assert(selectedIndex == repeat + 2);
    g_nativeUiNextNavigationRepeatAt =
      std::chrono::steady_clock::now() - std::chrono::seconds(1);
    assert(!nativeUiAdvanceNavigationRepeat(1));
    assert(deliveredRepeats == 0 && selectedIndex == repeat + 2);
  }
  selectedIndex = 49;
  assert(!nativeNavigateMainRows(1, nativeUiNavigationKeyIsRepeat(heldDown)));
  assert(selectedIndex == 49);
  nativeUiResetNavigationRepeat(VK_UP);
  assert(g_nativeUiHeldNavigationKey == VK_DOWN);
  nativeUiResetNavigationRepeat(VK_DOWN);
  assert(g_nativeUiHeldNavigationKey == 0);
  assert(!nativeUiPrepareNavigationKeyDown(1, VK_DOWN));
  assert(nativeNavigateMainRows(1, nativeUiNavigationKeyIsRepeat(firstDown)));
  assert(selectedIndex == 0 && g_nativeAppActiveListScrollPixels == 0);
#else
  assert(g_nativeUiNextNavigationRepeatAt > std::chrono::steady_clock::now());
  for (int repeat = 0; repeat < 30; ++repeat) {
    assert(nativeUiPrepareNavigationKeyDown(1, VK_DOWN));
    assert(selectedIndex == 1);
  }
  macArrowHeld[1] = true;
  g_nativeUiNextNavigationRepeatAt =
    std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
  const int beforePaints = paints;
  assert(nativeUiAdvanceNavigationRepeat(1));
  assert(deliveredRepeats == 1 && selectedIndex == 2);
  assert(paints == beforePaints);
  assert(!g_nativeUiSyntheticNavigationRepeat);
  assert(!nativeUiAdvanceNavigationRepeat(1));
  macArrowHeld[1] = false;
  assert(!nativeUiAdvanceNavigationRepeat(1));
  assert(g_nativeUiHeldNavigationKey == 0 && deliveredRepeats == 1);
#endif
  nativeUiResetNavigationRepeat();
  g_state.directorInterfaceBlocked = true;
  assert(!nativeUiPrepareNavigationKeyDown(1, VK_DOWN));
  assert(g_nativeUiHeldNavigationKey == 0);
}

int main() {
  heldRepeatRetargetsBetweenFrames();
  directionChangeCancelsForwardMotion();
  holdStopsAtEndsButNewPressWraps();
  skipsBlockedAndPlayingRowsAndRetainsQueueAnchor();
  syntheticRefreshDefersPaintToTimer();
  platformRepeatCadence();
  std::cout << "NAVIGATION_RENDER_CADENCE_OK "
#ifdef _WIN32
    << "Windows native repeat\n";
#else
    << "macOS synthetic repeat\n";
#endif
}
