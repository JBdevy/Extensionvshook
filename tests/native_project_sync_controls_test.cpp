#include <algorithm>
#include <cassert>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <deque>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include "native_project_sync_controls.h"
#include "native_control_mapping.h"

struct NativeTimecodeLanEvent {
  uint64_t sequence = 0;
  int64_t createdAtMs = 0;
  std::string command;
};
struct NativeTimecodeLanTransport {
  uint64_t sequence = 1, controlSequence = 1;
  int playState = 0;
  bool audioHealthy = true;
  double position = 12.5;
  int64_t sampledAtMs = 100;
  double frameRate = 30;
  bool dropFrame = false;
};
struct ReaProject {};
static ReaProject testProject;
static ReaProject* getCurrentProject(char*, int) { return &testProject; }
static int testPlayState = 0;
static double testCursorPosition = 0;
static int (*GetPlayStateEx_ptr)(ReaProject*) = [](ReaProject*) { return testPlayState; };
static double (*GetCursorPositionEx_ptr)(ReaProject*) = [](ReaProject*) { return testCursorPosition; };
static ReaProject* g_nativeTimecodeLanEditCursorProject = nullptr;
static bool g_nativeTimecodeLanEditCursorBaselineValid = false;
static double g_nativeTimecodeLanEditCursorBaseline = 0;
static uint64_t g_nativeTimecodeLanEditCursorSequence = 0;
static std::mutex g_nativeTimecodeLanMutex;
static bool g_nativeApplyingTimecodeLanRemoteCommand = false;
static std::string g_nativeTimecodeLanMode = "project_sync";
static std::string g_nativeTimecodeLanProjectSyncRole = "primary";
static std::string g_nativeParallelTimecodeMode = "transmitter";
static std::string g_nativeTimecodeLanCode = "012345", g_nativeTimecodeLanSelectedPeerId = "peer";
static std::string g_nativeTimecodeLanRejectedPeerId, g_nativeTimecodeLanPeerName = "PC B";
static bool g_nativeTimecodeLanPeerConnected = false;
static uint64_t g_nativeTimecodeLanEventSequence = 0, g_nativeParallelTimecodeLanEventSequence = 0;
static std::deque<NativeTimecodeLanEvent> g_nativeTimecodeLanOutbox, g_nativeParallelTimecodeLanOutbox;
static NativeTimecodeLanTransport g_nativeTimecodeLanTransport;
static struct { std::string state = "idle", requestId, error; int64_t updatedAtMs = 0; } g_nativeProjectSyncRoleHandover;
static int64_t nativeSystemNowMs() { static int64_t now = 100; return ++now; }
static std::string nativeTimecodeLanSessionId() { return "local-session"; }
static std::mutex g_nativeMutex;
static bool g_nativeAutoplayEnabled = false, g_nativeAuto2Enabled = false;
static bool g_nativeAutoStopEnabled = false, g_nativeAutoBlocoEnabled = false;
static bool g_nativeQueuedManual = true;
static std::string g_nativeAutoStopLastStoppedSignature;
static std::atomic<bool> g_nativeForceStateBuild{false};
static unsigned settingsLoads = 0;
static void nativeLoadAutomationSettingsOnceLocked() { ++settingsLoads; }
static void nativeSaveAutoStopEnabledLocked() {}
static void nativeSaveAutoplayEnabledLocked() {}
static void nativeSaveAutoBlocoEnabledLocked() {}
static void nativeClearAutoBlocoTargetLocked() {}
static void nativeClearQueuedSongLocked() {}
static void nativeBumpSharedRevisionLocked(const std::string&) {}
// Actual enrichment takes g_nativeMutex. Assert the publishing handler released
// it first, without hanging the test if the lock order regresses.
static std::string nativeProjectSyncDescribeControl(const std::string& command) {
  assert(g_nativeMutex.try_lock());
  g_nativeMutex.unlock();
  return command;
}
#include "controls-under-test.h"

static void record(const char* type) {
  nativeTimecodeLanRecordCommand(std::string("{\"type\":\"") + type + "\"}");
}
static void checkAbsoluteAutoControls() {
  g_nativeTimecodeLanMode = "project_sync";
  g_nativeTimecodeLanProjectSyncRole = "primary";
  g_nativeParallelTimecodeMode = "transmitter";
  const auto setMode = [](int kind, bool on) {
    g_nativeAutoplayEnabled = (kind < 2) && on;
    g_nativeAuto2Enabled = (kind == 1) && on;
    g_nativeAutoBlocoEnabled = (kind == 2) && on;
    g_nativeAutoStopEnabled = (kind == 3) && on;
  };
  const auto active = [](int kind) {
    if (kind == 0) return g_nativeAutoplayEnabled && !g_nativeAuto2Enabled;
    if (kind == 1) return g_nativeAutoplayEnabled && g_nativeAuto2Enabled;
    if (kind == 2) return g_nativeAutoBlocoEnabled;
    return g_nativeAutoStopEnabled;
  };
  const char* types[] = {"autoplay1_toggle", "autoplay2_toggle", "auto_bloco_toggle", "auto_stop_toggle"};
  for (int kind = 0; kind < 4; ++kind) {
    const std::string gesture = std::string("{\"type\":\"") + types[kind] + "\"}";
    // A is off, B is already on: A's explicit activation must leave both on.
    setMode(kind, false);
    g_nativeTimecodeLanOutbox.clear(); g_nativeParallelTimecodeLanOutbox.clear();
    const unsigned loadedBefore = settingsLoads;
    assert(nativeApplyAutoCommand(gesture));
    assert(settingsLoads == loadedBefore + 1 && active(kind));
    assert(g_nativeTimecodeLanOutbox.size() == 1);
    const std::string on = g_nativeTimecodeLanOutbox.back().command;
    assert(nativeJsonExtractString(on, "desiredState") == "true");
    assert(g_nativeParallelTimecodeLanOutbox.back().command == gesture);
    setMode(kind, true);
    g_nativeApplyingTimecodeLanRemoteCommand = true;
    assert(nativeApplyAutoCommand(on) && active(kind));
    assert(g_nativeTimecodeLanOutbox.size() == 1); // Remote application never echoes.
    g_nativeApplyingTimecodeLanRemoteCommand = false;
    // The next gesture is an absolute disable on both machines.
    setMode(kind, true);
    g_nativeTimecodeLanOutbox.clear();
    assert(nativeApplyAutoCommand(gesture) && !active(kind));
    const std::string off = g_nativeTimecodeLanOutbox.back().command;
    assert(nativeJsonExtractString(off, "desiredState") == "false");
    setMode(kind, true);
    g_nativeApplyingTimecodeLanRemoteCommand = true;
    assert(nativeApplyAutoCommand(off) && !active(kind));
    g_nativeApplyingTimecodeLanRemoteCommand = false;
    if (kind < 2) {
      // Selecting either Auto mode switches off the other, even if B started
      // in the opposite mode; this must not invert the requested mode.
      setMode(1 - kind, true);
      g_nativeApplyingTimecodeLanRemoteCommand = true;
      assert(nativeApplyAutoCommand(on) && active(kind));
      assert(g_nativeAuto2Enabled == (kind == 1));
      g_nativeApplyingTimecodeLanRemoteCommand = false;
    }
  }
  // V1 remains the original command, including relative toggles.
  g_nativeTimecodeLanMode = "transmitter";
  g_nativeTimecodeLanOutbox.clear();
  const std::string legacy = R"({"type":"autoplay1_toggle"})";
  assert(nativeApplyAutoCommand(legacy));
  assert(g_nativeTimecodeLanOutbox.back().command == legacy);
}

int main() {
  for (std::string command : {std::string(R"({"type":"play"})"),
       std::string(R"({"type":"play","__vshookLanRemote":false})"),
       std::string(R"({"type":"play","__vshookLanRemote":true})")}) {
    assert(NativeControlMapping::setField(command, "__vshookLanRemote", "true"));
    assert(nativeJsonExtractString(command, "__vshookLanRemote") == "true");
    const auto field = command.find("\"__vshookLanRemote\"");
    assert(field != std::string::npos);
    assert(command.find("\"__vshookLanRemote\"", field + 1) == std::string::npos);
  }
  const char* forbidden[] = {
    "project_sync_preflight", "project_sync_prepare_bundle", "project_sync_apply_bundle",
    "project_sync_bundle_progress", "project_sync_bundle_consumed", "project_sync_config_snapshot",
    "project_sync_manifest_snapshot", "project_sync_live_resync", "project_sync_live_timeline",
    "project_sync_live_item_state", "project_sync_live_item_upsert", "project_sync_live_item_delete",
    "project_sync_live_track_upsert", "project_sync_live_track_delete", "project_sync_unknown_future",
    "playlist_snapshot_sync", "multiloop_snapshot_set", "teleprompt_create_chord_item",
    "teleprompt_create_cifra_item", "teleprompt_cf", "project_select", "save_project",
    "unknown", "", "play\nproject_sync_apply_bundle"
  };
  for (const auto* type : forbidden) {
    assert(!nativeProjectSyncIsControlCommand(type));
    record(type);
  }
  assert(g_nativeTimecodeLanOutbox.empty());
  assert(g_nativeParallelTimecodeLanOutbox.empty());
  assert(g_nativeTimecodeLanEventSequence == 0);
  for (const auto* type : {"project_sync_role_handover", "project_sync_role_handover_state"}) {
    assert(nativeProjectSyncIsManagementCommand(type));
    assert(!nativeProjectSyncIsRemovedCommand(type));
    assert(!nativeProjectSyncIsControlCommand(type));
  }
  assert(nativeProjectSyncIsRemovedCommand("project_sync_apply_bundle"));
  assert(!nativeProjectSyncIsRemovedCommand("project_sync_track_state"));
  assert(!nativeProjectSyncIsRemovedCommand("project_sync_viewport_v1"));
  // Two transport edges must survive; a discarded media event cannot stall later control.
  record("play"); record("project_sync_live_item_upsert"); record("stop");
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  assert(g_nativeTimecodeLanEventSequence == 2);
  assert(g_nativeParallelTimecodeLanOutbox.size() == 2);
  assert(g_nativeTimecodeLanOutbox.front().command.find("play") != std::string::npos);
  assert(g_nativeTimecodeLanOutbox.back().command.find("stop") != std::string::npos);
  nativeTimecodeLanRecordCommand(R"({"type":"project_sync_track_state","trackId":"A","volume":0.5})");
  nativeTimecodeLanRecordCommand(R"({"type":"project_sync_track_state","trackId":"A","volume":0.9})");
  nativeTimecodeLanRecordCommand(R"({"type":"project_sync_track_state","trackId":"B","volume":0.7})");
  assert(g_nativeTimecodeLanOutbox.size() == 4);
  assert(g_nativeTimecodeLanEventSequence == 5);
  assert(g_nativeTimecodeLanOutbox[2].command.find("0.9") != std::string::npos);
  // Authority and echo prevention are unchanged.
  const auto count = g_nativeTimecodeLanOutbox.size();
  g_nativeApplyingTimecodeLanRemoteCommand = true; record("play_start");
  g_nativeApplyingTimecodeLanRemoteCommand = false;
  g_nativeTimecodeLanProjectSyncRole = "secondary"; record("play_start");
  assert(g_nativeTimecodeLanOutbox.size() == count);
  // No project object/media is supplied by this fixture. Pairing alone is readiness.
  assert(nativeTimecodeLanStatusJson().find("\"projectSyncReady\":false") != std::string::npos);
  g_nativeTimecodeLanPeerConnected = true;
  const auto status = nativeTimecodeLanStatusJson();
  assert(status.find("\"projectSyncReady\":true") != std::string::npos);
  assert(status.find("\"projectSyncProtocolVersion\":3") != std::string::npos);
  assert(status.find("\"projectSyncControlsOnly\":true") != std::string::npos);
  assert(status.find("\"projectSyncPreflight\":false") != std::string::npos);
  assert(status.find("manifest") == std::string::npos);
  assert(status.find("Bundle") == std::string::npos);
  assert(status.find("descriptorPath") == std::string::npos);
  assert(status.find("\"transport\"") == std::string::npos);
  assert(nativeTimecodeLanOutboxJson(0).find("\"transport\"") == std::string::npos);
  g_nativeTimecodeLanProjectSyncRole = "primary";
  g_nativeTimecodeLanOutbox.clear();
  g_nativeParallelTimecodeLanOutbox.clear();
  g_nativeParallelTimecodeMode = "off";
  // A queue gesture and Auto setting travel once. Each machine later consumes
  // its own queue; derived transport/UI callbacks cannot overwrite the peer.
  nativeTimecodeLanRecordCommand(R"({"type":"queue_region_song","targetId":"9","manual":true})");
  nativeTimecodeLanRecordCommand(R"({"type":"auto_set","enabled":true})");
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  {
    NativeProjectSyncAutomaticScope autoTransition;
    record("play_start"); record("stop"); record("clear_queue");
    record("select_region"); record("ui_music_selection_snapshot");
    { NativeProjectSyncAutomaticScope nestedFadeout; record("stop"); }
    assert(nativeProjectSyncAutomaticDepth() == 1);
  }
  assert(nativeProjectSyncAutomaticDepth() == 0);
  nativeTimecodeLanRecordCommand(R"({"type":"queue_region_song","targetId":"3","autoQueue":true})");
  nativeTimecodeLanRecordCommand(R"({"type":"queue_playlist_song","targetId":"4","manual":false})");
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  for (const auto* type : {"timecode_transport_sync", "project_sync_viewport_v1", "mixer_viewport_set"}) {
    assert(!nativeProjectSyncIsIndependentControlCommand(type));
    record(type);
  }
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  // A deliberate Parts action still reaches the other VS Hook exactly once.
  const std::string part = R"({"type":"marker_go","markerId":"m8"})";
  nativeTimecodeLanRecordCommand(part);
  nativeTimecodeLanRecordCommand(part);
  assert(g_nativeTimecodeLanOutbox.size() == 3);
  g_nativeApplyingTimecodeLanRemoteCommand = true;
  nativeTimecodeLanRecordCommand(part); // Applying B's marker cannot echo to A.
  g_nativeApplyingTimecodeLanRemoteCommand = false;
  assert(g_nativeTimecodeLanOutbox.size() == 3);
  record("play_start"); record("stop");
  assert(g_nativeTimecodeLanOutbox.size() == 5);
  // Independent A->B does not strip position/transport from a traditional A->C link.
  g_nativeParallelTimecodeMode = "transmitter";
  record("edit_cursor_move");
  assert(g_nativeTimecodeLanOutbox.size() == 6);
  assert(g_nativeParallelTimecodeLanOutbox.size() == 1);
  g_nativeTimecodeLanMode = "transmitter";
  record("timecode_transport_sync");
  assert(g_nativeTimecodeLanOutbox.size() == 7);
  assert(nativeTimecodeLanStatusJson().find("\"transport\"") != std::string::npos);
  assert(nativeTimecodeLanOutboxJson(0).find("\"transport\"") != std::string::npos);
  g_nativeTimecodeLanMode = "receive";
  assert(nativeTimecodeLanStatusJson().find("\"projectSyncReady\":false") != std::string::npos);
  // Manual stopped positions cross exactly once, before the next Play.
  g_nativeTimecodeLanMode = "project_sync";
  g_nativeTimecodeLanProjectSyncRole = "primary";
  g_nativeParallelTimecodeMode = "off";
  g_nativeTimecodeLanOutbox.clear();
  testCursorPosition = 60;
  nativeTimecodeLanPollEditCursorOnMainThread(); // initial baseline, no jump on pair
  assert(g_nativeTimecodeLanOutbox.empty());
  testCursorPosition = 123.456;
  nativeTimecodeLanPollEditCursorOnMainThread();
  nativeTimecodeLanPollEditCursorOnMainThread();
  assert(g_nativeTimecodeLanOutbox.size() == 1);
  const std::string cursor = g_nativeTimecodeLanOutbox.back().command;
  assert(nativeJsonExtractString(cursor, "type") == "edit_cursor_move");
  assert(nativeJsonExtractString(cursor, "position") == "123.456");
  assert(nativeJsonExtractString(cursor, "noSeek") == "true");
  record("play_start");
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  // Neither direct UI commands nor polled cursor moves during Play travel.
  for (int state : {1, 4, 5}) {
    testPlayState = state;
    testCursorPosition += 12;
    nativeTimecodeLanPollEditCursorOnMainThread();
    record("edit_cursor_move");
    assert(g_nativeTimecodeLanOutbox.size() == 2);
  }
  testPlayState = 0;
  nativeTimecodeLanPollEditCursorOnMainThread();
  assert(g_nativeTimecodeLanOutbox.size() == 2); // no delayed movement after Stop
  g_nativeTimecodeLanProjectSyncRole = "secondary";
  testCursorPosition = 333;
  nativeTimecodeLanPollEditCursorOnMainThread();
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  g_nativeTimecodeLanProjectSyncRole = "primary";
  nativeTimecodeLanPollEditCursorOnMainThread();
  assert(g_nativeTimecodeLanOutbox.size() == 2); // role changes do not replay B's cursor
  { NativeProjectSyncAutomaticScope automaticStop; record("edit_cursor_move"); }
  assert(g_nativeTimecodeLanOutbox.size() == 2);
  checkAbsoluteAutoControls();
  std::cout << "PROJECT_SYNC_CONTROLS_OK: legacy file/structure commands rejected; controls survive; "
               "coalescing, role authority, no echo and pairing-only readiness preserved\n";
}
