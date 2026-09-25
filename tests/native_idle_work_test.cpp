#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <cerrno>
#ifdef _WIN32
#include <winsock2.h>
using native_socket_t = SOCKET;
static void closeSocket(SOCKET s) { closesocket(s); }
#else
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
using native_socket_t = int;
static void closeSocket(int s) { close(s); }
#endif

struct MediaTrack { std::string id, name; };
struct ReaProject { std::vector<MediaTrack> tracks; std::string selected; bool hasValue = true; };
static ReaProject first, second;
static ReaProject* current = &first;
static int guidReads = 0, nameReads = 0, playState = 0;
static std::string luaSelection, mode = "project_sync", parallelMode;
static const char* kManualStopFadeoutProjectSection = "section";
static const char* kManualStopFadeoutTracksKey = "key";
static ReaProject* getCurrentProject(char*, int) { return current; }
static int countTracks(ReaProject* p) { return static_cast<int>(p->tracks.size()); }
static MediaTrack* getTrack(ReaProject* p, int i) { return &p->tracks[i]; }
static int getProjectValue(ReaProject* p, const char*, const char*, char* buf, int size) {
  std::strncpy(buf, p->selected.c_str(), size - 1);
  buf[size - 1] = 0;
  return p->hasValue ? 1 : 0;
}
static auto CountTracks_ptr = &countTracks;
static auto GetTrack_ptr = &getTrack;
static auto GetProjExtState_ptr = &getProjectValue;
static int getPlayState(ReaProject*) { return playState; }
static auto GetPlayStateEx_ptr = &getPlayState;
static std::string nativeTrackGuid(MediaTrack* t, int) { ++guidReads; return t->id; }
static std::string nativeTrackName(MediaTrack* t, int) { ++nameReads; return t->name; }
static bool nativeMixerTrackIsHidden(const std::string& name) { return name == "hidden"; }
static std::string nativeReadLuaWindowExtState(const char*) { return luaSelection; }
static std::string nativeTrim(const std::string& s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
static std::string getTimecodeMode() { return mode; }
static std::string getParallelTimecodeMode() { return parallelMode; }
static bool panel = false, controller = false, tp[2] = {}, video = false, clockOpen[2] = {}, director = false;
static bool nativeAppActivePanelIsOpen() { return panel; }
static bool nativeHookControllerWindowIsOpen() { return controller; }
static bool nativeTelepromptWindowIsOpen(int slot) { return tp[slot - 1]; }
static bool nativeVideoWindowIsOpen() { return video; }
static bool nativeBigClockWindowIsOpen(int slot) { return clockOpen[slot]; }
static bool nativeIsDirectorControlActive() { return director; }
static std::mutex g_nativeTimecodeLanMutex, g_nativeMutex, g_nativeManualStopFadeoutMutex;
static bool g_nativeTimecodeLanPeerConnected = false, g_nativeTimerRunning = false;
static struct { bool active = false, restorePending = false; } g_nativeManualStopFadeoutRuntime;
static constexpr int kNativeBridgeLiveIntervalMs = 220;
static int g_nativeUiHeldNavigationKey = 0, builds = 0;
static bool lastForcedSnapshot = false;
static std::atomic<bool> g_nativeForceStateBuild{false}, g_nativeForceSnapshotBuild{false};
static std::chrono::steady_clock::time_point g_nativeLastLiveBuild;
static void nativeRebuildState(bool force) { ++builds; lastForcedSnapshot = force; }

#include "idle-work-under-test.h"

static std::vector<std::string> legacySelection() {
  std::vector<std::string> result;
  const auto raw = current && current->hasValue ? current->selected : luaSelection;
  size_t begin = 0;
  while (begin <= raw.size()) {
    const auto end = raw.find(';', begin);
    const auto id = nativeTrim(raw.substr(begin, end == std::string::npos ? end : end - begin));
    if (!id.empty() && !nativeManualStopFadeoutTrackIdIsHidden(current, id) &&
        std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return result;
}
static void sameAsLegacy() {
  const auto expected = legacySelection();
  guidReads = nameReads = 0;
  assert(nativeManualStopFadeoutTrackIds() == expected);
  assert(guidReads <= (current ? countTracks(current) : 0));
}

static void testBridgeSocketWait() {
#ifdef _WIN32
  WSADATA data{};
  assert(WSAStartup(MAKEWORD(2, 2), &data) == 0);
  using address_size_t = int;
#else
  using address_size_t = socklen_t;
  rlimit limit{};
  assert(getrlimit(RLIMIT_NOFILE, &limit) == 0);
  limit.rlim_cur = std::max(limit.rlim_cur, static_cast<rlim_t>(FD_SETSIZE + 256));
  assert(setrlimit(RLIMIT_NOFILE, &limit) == 0);
#endif
  auto listener = socket(AF_INET, SOCK_STREAM, 0);
  assert(listener != static_cast<native_socket_t>(-1));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
  assert(listen(listener, 4) == 0);
  address_size_t size = sizeof(address);
  assert(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
#ifndef _WIN32
  const int high = fcntl(listener, F_DUPFD, FD_SETSIZE + 200);
  assert(high >= FD_SETSIZE);
  closeSocket(listener);
  listener = high; // Reproduces the descriptor index observed in the live REAPER.
#endif
  const auto start = std::chrono::steady_clock::now();
  assert(nativeWaitForBridgeConnection(listener) == 0);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  assert(elapsed >= std::chrono::milliseconds(100)); // Idle wait must block, not spin.
  const auto client = socket(AF_INET, SOCK_STREAM, 0);
  assert(client != static_cast<native_socket_t>(-1));
  assert(connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
  assert(nativeWaitForBridgeConnection(listener) == 1);
  const auto accepted = accept(listener, nullptr, nullptr);
  assert(accepted != static_cast<native_socket_t>(-1));
  assert(send(client, "x", 1, 0) == 1);
  char received = 0;
  assert(recv(accepted, &received, 1, 0) == 1 && received == 'x');
  assert(send(accepted, "y", 1, 0) == 1);
  assert(recv(client, &received, 1, 0) == 1 && received == 'y');
  closeSocket(accepted);
  closeSocket(client);
  closeSocket(listener);
  assert(nativeWaitForBridgeConnection(listener) == -1); // Fatal errors exit the server loop.
  std::cout << "BRIDGE_SOCKET_WAIT_OK: descriptor=" << listener << '\n';
#ifdef _WIN32
  WSACleanup();
#endif
}

int main() {
  first.tracks = {{"a", "visible"}, {"b", "hidden"}, {"c", "visible"}};
  first.selected = " c ;b;a;c;missing;; a ";
  sameAsLegacy();
  assert((nativeManualStopFadeoutTrackIds() == std::vector<std::string>{"c", "a", "missing"}));
  first.tracks[0].name = "hidden"; // Rename takes effect on the next call.
  sameAsLegacy();
  first.tracks.erase(first.tracks.begin()); // Removed IDs remain persisted.
  sameAsLegacy();
  second.tracks = {{"a", "visible"}, {"c", "hidden"}};
  second.selected = "a;c;missing";
  current = &second;
  sameAsLegacy(); // No cache leaks across projects.
  second.hasValue = false;
  luaSelection = "c;a;a;legacy";
  sameAsLegacy();
  second.hasValue = true;
  second.selected.clear(); // Explicit empty project value overrides Lua.
  sameAsLegacy();
  assert(guidReads == 0);
  current = nullptr;
  sameAsLegacy();
  current = &first;
  first.tracks = {{"x", "visible"}, {"x", "hidden"}};
  first.selected = "x";
  sameAsLegacy(); // First matching track wins, including malformed duplicate GUIDs.
  first.tracks[0].name = "hidden";
  first.tracks[1].name = "visible";
  sameAsLegacy();
  CountTracks_ptr = nullptr;
  sameAsLegacy();
  CountTracks_ptr = &countTracks;
  GetTrack_ptr = nullptr;
  sameAsLegacy();
  GetTrack_ptr = &getTrack;
  first.tracks.clear();
  first.selected.clear();
  for (int i = 0; i < 500; ++i) {
    const auto id = std::to_string(i);
    first.tracks.push_back({id, i % 3 == 0 ? "hidden" : "visible"});
    first.selected += id + ";";
  }
  guidReads = 0;
  const auto expected = legacySelection();
  const auto legacyReads = guidReads;
  guidReads = nameReads = 0;
  assert(nativeManualStopFadeoutTrackIds() == expected);
  assert(guidReads == 500 && legacyReads == 125250);
  std::cout << "FADEOUT_GUID_READS: " << legacyReads << " -> " << guidReads << '\n';

  assert(nativeBridgeStateIntervalMs() == 1000);
  for (auto flag : {&panel, &controller, &tp[0], &tp[1], &video,
      &clockOpen[0], &clockOpen[1], &director, &g_nativeTimecodeLanPeerConnected,
      &g_nativeTimerRunning, &g_nativeManualStopFadeoutRuntime.active,
      &g_nativeManualStopFadeoutRuntime.restorePending}) {
    *flag = true;
    assert(nativeBridgeStateIntervalMs() == 220);
    *flag = false;
  }
  for (int state : {1, 2, 4, 5}) {
    playState = state;
    assert(nativeBridgeStateIntervalMs() == 220);
  }
  playState = 0;
  for (const auto value : {"", "receive", "transmitter"}) {
    mode = value;
    assert(nativeBridgeStateIntervalMs() == 220);
  }
  mode = "project_sync";
  parallelMode = "transmitter";
  assert(nativeBridgeStateIntervalMs() == 220);
  parallelMode.clear();
  GetPlayStateEx_ptr = nullptr;
  assert(nativeBridgeStateIntervalMs() == 220);
  GetPlayStateEx_ptr = &getPlayState;
  current = nullptr;
  assert(nativeBridgeStateIntervalMs() == 220);
  current = &first;

  nativeBridgeTick(); // First snapshot is immediate.
  assert(builds == 1);
  g_nativeLastLiveBuild = std::chrono::steady_clock::now() - std::chrono::milliseconds(500);
  nativeBridgeTick();
  assert(builds == 1); // Idle snapshot waits; discovery does not pass through this throttle.
  panel = true;
  nativeBridgeTick();
  assert(builds == 2); // Opening a window immediately restores the active cadence.
  panel = false;
  g_nativeForceStateBuild = true;
  nativeBridgeTick();
  assert(builds == 3);
  g_nativeUiHeldNavigationKey = 1;
  g_nativeForceSnapshotBuild = true;
  nativeBridgeTick();
  assert(builds == 3 && g_nativeForceSnapshotBuild.load());
  g_nativeUiHeldNavigationKey = 0;
  nativeBridgeTick();
  assert(builds == 4 && lastForcedSnapshot);
  g_nativeLastLiveBuild = std::chrono::steady_clock::now() - std::chrono::milliseconds(1100);
  nativeBridgeTick();
  assert(builds == 5); // Standby still publishes periodically.
  nativeBridgeNoteStateClient(); // Remote reader, no window or Director heartbeat.
  assert(nativeBridgeStateClientActive());
  assert(nativeBridgeStateIntervalMs() == 220);
  nativeBridgeTick();
  assert(builds == 6); // The first remote read requests an immediate fresh state.
  g_nativeLastLiveBuild = std::chrono::steady_clock::now() - std::chrono::milliseconds(500);
  nativeBridgeTick();
  assert(builds == 7); // App polling keeps full cadence with all windows closed.
  nativeBridgeNoteStateClient();
  assert(!g_nativeForceStateBuild.load()); // Polling does not force every request.
  g_nativeLastStateClientReadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count() - 13000;
  assert(!nativeBridgeStateClientActive());
  assert(nativeBridgeStateIntervalMs() == 1000);
  nativeBridgeNoteStateClient();
  assert(g_nativeForceStateBuild.load()); // Reconnection wakes again.
  testBridgeSocketWait();
  std::cout << "NATIVE_IDLE_WORK_OK\n";
}
