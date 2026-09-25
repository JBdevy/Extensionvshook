const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
const start = source.indexOf('static void openAppActiveScreenForDirector()\n{');
const end = source.indexOf('static void requestAppActiveScreenForDirector()', start);
if (start < 0 || end < start) throw new Error('Director transition implementation missing');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-director-paint-'));
try {
  fs.writeFileSync(path.join(dir, 'test.cpp'), `
#include <cassert>
#include <chrono>
#include <iostream>
struct State {
  bool directorInterfaceBlocked = false;
  bool pcAccessOverride = false;
  bool appActiveScreenOpen = false;
} g_state;
static bool g_nativeAppActiveResumeConfirmOpen = false;
static bool g_nativeAppActiveControlBlockedNoticeOpen = false;
static std::chrono::steady_clock::time_point g_lastAppActiveTransition{};
static bool blockPreference = false, windowOpen = true;
static int paints = 0;
static void* g_nativeAppActivePanelHwnd = nullptr;
static constexpr bool FALSE = false;
static bool nativeUiBlockInterfaceWhenDirectorConnectedEnabled() { return blockPreference; }
static bool nativeAppActivePanelIsOpen() { return windowOpen; }
static void nativeUiInvalidateRect(void*, void*, bool) { ++paints; }
${source.slice(start, end)}
int main() {
  for (int i = 0; i < 100; ++i) openAppActiveScreenForDirector();
  assert(paints == 0 && g_lastAppActiveTransition.time_since_epoch().count() != 0);
  blockPreference = true;
  openAppActiveScreenForDirector();
  assert(paints == 1 && g_state.directorInterfaceBlocked);
  for (int i = 0; i < 100; ++i) openAppActiveScreenForDirector();
  assert(paints == 1);
  g_state.pcAccessOverride = true;
  openAppActiveScreenForDirector();
  assert(paints == 2 && !g_state.directorInterfaceBlocked);
  g_nativeAppActiveResumeConfirmOpen = true;
  openAppActiveScreenForDirector();
  assert(paints == 3 && !g_nativeAppActiveResumeConfirmOpen);
  g_nativeAppActiveControlBlockedNoticeOpen = true;
  openAppActiveScreenForDirector();
  assert(paints == 4 && !g_nativeAppActiveControlBlockedNoticeOpen);
  windowOpen = false;
  g_state.pcAccessOverride = false;
  g_state.appActiveScreenOpen = true;
  openAppActiveScreenForDirector();
  assert(paints == 4 && g_state.directorInterfaceBlocked && !g_state.appActiveScreenOpen);
  blockPreference = false;
  openAppActiveScreenForDirector();
  assert(paints == 4 && !g_state.directorInterfaceBlocked);
  windowOpen = true;
  for (int i = 0; i < 100; ++i) openAppActiveScreenForDirector();
  assert(paints == 4);
  std::cout << "DIRECTOR_HEARTBEAT_PAINT_OK: unchanged heartbeats skip repaint; permissions, dialogs and closed window preserved\\n";
}
`);
  const exe = path.join(dir, process.platform === 'win32' ? 'test.exe' : 'test');
  const compile = process.platform === 'win32'
    ? ['cl', ['/nologo', '/EHsc', '/std:c++17', '/O2', 'test.cpp', `/Fe:${exe}`]]
    : ['c++', ['-std=c++17', '-O2', '-Wall', '-Wextra', 'test.cpp', '-o', exe]];
  for (const [command, args] of [compile, [exe, []]]) {
    const result = spawnSync(command, args, {cwd: dir, stdio: 'inherit'});
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally {
  fs.rmSync(dir, {recursive: true, force: true});
}
