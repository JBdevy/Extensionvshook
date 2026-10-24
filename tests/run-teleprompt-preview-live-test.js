const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
function extract(start, end) {
  const a = source.indexOf(start), b = source.indexOf(end, a);
  if (a < 0 || b < a) throw new Error('Missing production code: ' + start);
  return source.slice(a, b);
}
const fixture = `
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
${extract('struct NativeSongWindow {', 'struct NativePendingPlayTarget {')}
static std::mutex g_nativeMutex;
static bool g_nativeLiveMarkEnabled = false;
static std::map<std::string, bool> g_nativeLiveExecutedItems;
${extract('static void nativeApplyLiveExecutedFlags(', 'static std::string nativeSongToJson(')}
${extract('static void nativeUiHashBytes(', 'static uint64_t nativeUiRowsSourceSignature(')}
static std::string nativeTrim(std::string s) { return s; }
static std::string nativeLower(std::string s) { return s; }
static std::string nativeJsonString(const std::string& s) { return "\\\"" + s + "\\\""; }
static std::string nativeNumber(double n, int) { return std::to_string(n); }
static std::vector<int> nativeUiReadPreviewBlocks(int) { return {1}; }
static bool nativeSongIdMatches(const NativeSongWindow& song, const std::string& id) { return song.id == id || song.playlistEntryId == id; }
static std::string nativeBlockColorHexFromKey(const std::string&, int) { return "#FFE02E"; }
${extract('static double nativeRawDurationSec(', 'static double nativeComputeRegionsTotalSec(')}
${extract('struct NativeTelepromptPreviewSong {', 'struct NativeTelepromptRenderState {')}
using HDC = int; using HFONT = int; using HPEN = int; using HGDIOBJ = int;
using LONG = int; using COLORREF = unsigned;
struct RECT { int left, top, right, bottom; };
constexpr int DT_CENTER=1, DT_LEFT=2, DT_VCENTER=4, DT_SINGLELINE=8, DT_NOPREFIX=16, PS_SOLID=0;
static std::vector<RECT> strikes;
static int underlines = 0, texts = 0;
static void nativeAppActiveDrawText(HDC, const std::string&, RECT, int, COLORREF, HFONT) { ++texts; }
static int nativeUiTextWidth(HDC, const std::string& s, HFONT) { return static_cast<int>(s.size()) * 10; }
static HPEN CreatePen(int, int, COLORREF) { return 0; }
static HGDIOBJ SelectObject(HDC, HGDIOBJ) { return 0; }
static void MoveToEx(HDC, int, int, void*) {}
static void LineTo(HDC, int, int) { ++underlines; }
static void DeleteObject(HGDIOBJ) {}
static void nativeAppActiveFillRect(HDC, RECT rect, COLORREF) { strikes.push_back(rect); }
${extract('static void nativeTelepromptDrawPreviewLines(', 'static void nativeTelepromptDrawPreview(')}
int main() {
  NativeSongWindow parent; parent.id="family"; parent.name="Family"; parent.isHashParent=true;
  NativeSongWindow first; first.id="first"; first.name="First song"; first.start=10; first.end=20; first.isRegionChild=true; first.parentId=parent.id;
  NativeSongWindow second=first; second.id="second"; second.name="Second song"; second.start=20; second.end=30;
  NativeSongWindow single; single.id="single"; single.name="Single song"; single.start=30; single.end=40;
  std::vector<NativeSongWindow> songs={parent, first, second, single};
  const auto preview = [&]() {
    nativeApplyLiveExecutedFlags(songs);
    return nativeTelepromptBuildPreviewState({songs[0],songs[3]}, songs, 1, true, "first", 10, 20, "second", 20, 30);
  };
  g_nativeLiveExecutedItems["first"] = true;
  const auto disabled = preview();
  assert(disabled.blocks.size()==1 && disabled.blocks[0].songs.size()==3);
  assert(!disabled.blocks[0].songs[0].liveExecuted);
  g_nativeLiveMarkEnabled = true;
  const auto marked = preview();
  assert(marked.blocks[0].songs[0].liveExecuted && marked.blocks[0].songs[0].playing);
  assert(!marked.blocks[0].songs[1].liveExecuted && marked.blocks[0].songs[1].queued);
  assert(!marked.blocks[0].songs[2].liveExecuted);
  assert(marked.revision != disabled.revision);
  const auto json = nativeTelepromptPreviewStateJson(marked);
  assert(json.find("\\\"liveExecuted\\\":true") != std::string::npos);
  assert(json.find("\\\"liveMarked\\\":true") != std::string::npos);
  g_nativeLiveExecutedItems.erase("first");
  const auto reset = preview();
  assert(reset.revision == disabled.revision && !reset.blocks[0].songs[0].liveExecuted);
  g_nativeLiveExecutedItems["single"] = true;
  assert(preview().blocks[0].songs[2].liveExecuted);
  g_nativeLiveMarkEnabled = false;
  assert(preview().revision == disabled.revision);
  // A wrapped name gets one clipped strike per visible text line; block
  // headings/unmarked songs and blank lines receive no strike.
  nativeTelepromptDrawPreviewLines(1, {"first", "second", ""}, {10,20,80,110}, 30, false, 0xffffff, 1, false, true);
  assert(texts==2 && strikes.size()==2 && underlines==0);
  assert(strikes[0].left==10 && strikes[0].right==60 && strikes[0].top==34);
  assert(strikes[1].left==10 && strikes[1].right==70 && strikes[1].top==64);
  strikes.clear();
  nativeTelepromptDrawPreviewLines(1, {"very long title"}, {10,20,80,50}, 30, true, 0xffffff, 1, true, true);
  assert(strikes.size()==1 && strikes[0].left==10 && strikes[0].right==80 && underlines==1);
  strikes.clear();
  nativeTelepromptDrawPreviewLines(1, {"unmarked"}, {0,0,100,30}, 30, false, 0xffffff, 1, false);
  assert(strikes.empty());
  std::cout << "TELEPROMPT_PREVIEW_LIVE_OK: Live gate, family children, reset, revision, bridge and wrapped strikes\\n";
}
`;
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-preview-live-'));
try {
  const cpp = path.join(temporary, 'test.cpp'); fs.writeFileSync(cpp, fixture);
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'test.exe' : 'test');
  const args = windows ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/Fe:' + executable, cpp]
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', cpp, '-o', executable];
  for (const [command, parameters] of [[windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]]) {
    const result = spawnSync(command, parameters, {cwd: temporary, encoding: 'utf8'});
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally { fs.rmSync(temporary, {recursive: true, force: true}); }
