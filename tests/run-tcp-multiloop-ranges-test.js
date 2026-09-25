const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
const start = source.indexOf('static std::string nativeBuildTCPMultiLoopRangesJson(');
const end = source.indexOf('static void nativeReleaseLoopAfterPartsArrival(', start);
if (start < 0 || end < start) throw new Error('Missing production range publisher');
const fixture = `
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
struct ReaProject {};
struct NativeSongWindow { std::string id; double start, end; bool block = false; };
struct NativeMultiLoopPair { bool valid; double start, end; };
struct State { bool loop[4] = {}; };
static constexpr int kNativeMultiLoopSlotCount = 4;
static std::map<std::string, State> g_nativeMultiLoopStates;
static int pairLookups = 0;
static std::string nativeMultiLoopSongKey(const NativeSongWindow& song) { return song.block ? "" : song.id; }
static std::string nativeJsonString(const std::string& value) { return "\\\"" + value + "\\\""; }
static std::string nativeNumber(double value) { std::ostringstream s; s << value; return s.str(); }
static NativeMultiLoopPair nativeFindMultiLoopPair(ReaProject*, const std::string&, double start, double end, int slot) {
  ++pairLookups; return {slot != 3, start + slot, end};
}
${source.slice(start, end)}
int main() {
  ReaProject project;
  std::vector<NativeSongWindow> songs = {{"a", 10, 50}, {"b", 100, 150}, {"block", 0, 200, true}};
  assert(nativeBuildTCPMultiLoopRangesJson(&project, songs) == "[]");
  g_nativeMultiLoopStates["a"].loop[1] = true;
  g_nativeMultiLoopStates["a"].loop[3] = true; // Enabled but missing its second marker.
  g_nativeMultiLoopStates["b"].loop[0] = true;
  g_nativeMultiLoopStates["block"].loop[0] = true;
  const auto json = nativeBuildTCPMultiLoopRangesJson(&project, songs);
  assert(json == R"([{"songId":"a","slot":2,"startPos":11,"endPos":50},{"songId":"b","slot":1,"startPos":100,"endPos":150}])");
  assert(pairLookups == 3); // Disabled slots and block headers cause no lookup.
  g_nativeMultiLoopStates["a"].loop[1] = false;
  assert(nativeBuildTCPMultiLoopRangesJson(&project, songs).find("\\\"a\\\"") == std::string::npos);
  std::cout << "TCP_MULTILOOP_RANGES_OK: enabled pairs across songs, no modal/Repeat dependency, disabled and invalid pairs omitted\\n";
}
`;
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-tcp-ranges-'));
try {
  const cpp = path.join(temporary, 'test.cpp'); fs.writeFileSync(cpp, fixture);
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'test.exe' : 'test');
  const args = windows ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/Fe:' + executable, cpp] : ['-std=c++17', '-O2', '-Wall', '-Wextra', cpp, '-o', executable];
  for (const [command, parameters] of [[windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]]) {
    const result = spawnSync(command, parameters, {cwd: temporary, encoding: 'utf8'});
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally { fs.rmSync(temporary, {recursive: true, force: true}); }
