#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

struct ReaProject { std::string path; };
static ReaProject a{"A.rpp"}, b{"B.rpp"};
static ReaProject* current = &a;
static struct { std::string activeProjectSignature; int projectStableTicks = 0; } g_state;
static std::string g_nativePremixSelectedSongId;
static std::string g_nativeTimecodeLanSuppressedRemoteProjectSignature;
static std::atomic<bool> g_nativeForceStateBuild{false};
static bool g_nativeApplyingTimecodeLanRemoteCommand = false;
static int resets = 0;
static ReaProject* getCurrentProject(char* out, int size) {
  if (out && size > 0) { std::strncpy(out, current->path.c_str(), size - 1); out[size - 1] = 0; }
  return current;
}
static std::string projectId(ReaProject* project) { return std::to_string(reinterpret_cast<std::uintptr_t>(project)); }
static std::string getCurrentProjectSignature() { return projectId(current) + "|" + current->path; }
static std::string normalizeSlashes(std::string path) { std::replace(path.begin(), path.end(), '\\', '/'); return path; }
static std::string nativeBasenameNoRpp(std::string path) { return path.substr(0, path.size() - 4); }
static std::string nativeJsonExtractString(const std::string& body, const char* key) {
  const auto start = body.find(std::string("\"") + key + "\":\"");
  if (start == std::string::npos) return {};
  const auto value = start + std::strlen(key) + 4;
  return body.substr(value, body.find('"', value) - value);
}
static bool nativeLooksNumeric(const std::string& text) { return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos; }
static ReaProject* enumerate(int index, char* out, int size) {
  ReaProject* project = index == 0 ? &a : index == 1 ? &b : nullptr;
  if (project && out && size > 0) { std::strncpy(out, project->path.c_str(), size - 1); out[size - 1] = 0; }
  return project;
}
static auto EnumProjects_ptr = &enumerate;
static void select(ReaProject* project) { current = project; }
static auto SelectProjectInstance_ptr = &select;
static void nativeUiResetForProjectTabChange() { ++resets; g_nativePremixSelectedSongId.clear(); }
static void nativeTimecodeLanPublishActiveProjectTabOnMainThread(const std::string&) {}
static void nativeTimecodeLanRecordCommand(const std::string&) {}

#include "project-premix-under-test.h"

int main() {
  nativeObserveProjectTabChange();
  assert(resets == 0);
  g_nativePremixSelectedSongId = "old-song";
  assert(nativeSelectProjectFromCommand("{\"type\":\"set_project_tab\",\"projectTabIndex\":\"1\"}"));
  assert(current == &b && resets == 1 && g_nativePremixSelectedSongId.empty());
  // The next focus request can be in the same batch as the tab switch.
  g_nativePremixSelectedSongId = "new-song";
  nativeObserveProjectTabChange();
  assert(resets == 1 && g_nativePremixSelectedSongId == "new-song");
  // A manual host-tab change must be observed before accepting a new focus.
  current = &a;
  nativeObserveProjectTabChange();
  g_nativePremixSelectedSongId = "song-a";
  nativeObserveProjectTabChange();
  assert(resets == 2 && g_nativePremixSelectedSongId == "song-a");
  assert(nativePremixCommandMatchesCurrentProject("{}")); // legacy clients
  assert(nativePremixCommandMatchesCurrentProject("{\"premixProjectId\":\"" + projectId(&a) + "\"}"));
  assert(!nativePremixCommandMatchesCurrentProject("{\"premixProjectId\":\"" + projectId(&b) + "\"}"));
  assert(nativePremixCommandMatchesCurrentProject("{\"premixProjectPath\":\"A.rpp\"}"));
  assert(!nativePremixCommandMatchesCurrentProject("{\"premixProjectPath\":\"B.rpp\"}"));
  g_nativeApplyingTimecodeLanRemoteCommand = true;
  assert(nativePremixCommandMatchesCurrentProject("{\"premixProjectId\":\"remote-pointer\",\"premixProjectPath\":\"remote-path\"}"));
  assert(!nativeSelectProjectFromCommand("{\"type\":\"set_project_tab\",\"projectTabIndex\":\"9\"}"));
  assert(current == &a && resets == 2);
  std::cout << "PROJECT_TAB_PREMIX_OK: immediate reset, preserved new focus, manual tabs, stale command rejection and LAN compatibility\n";
}
