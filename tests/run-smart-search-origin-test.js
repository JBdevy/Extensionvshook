// Compile actual search functions with a minimal transport/UI fixture.
// All generated outputs stay in an OS temporary test directory.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8').replace(/\r\n/g, '\n');
const extract = (from, to) => {
  const start = source.indexOf(from);
  const end = source.indexOf(to, start + from.length);
  if (start < 0 || end <= start) throw new Error(`Search function not found: ${from}`);
  return source.slice(start, end);
};
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-search-test-'));
fs.writeFileSync(path.join(temporary, 'search-under-test.h'), [
  extract('static std::string nativeUiMainRowKey(\n', 'static int nativeUiMainSelectionCount('),
  extract('static std::string nativeUiMainRowStableIdentity(\n  const NativeAppActivePanelModel::Row& row)\n{', 'struct NativeUiReorderFamilyInfo'),
  extract('static bool nativeUiCommitPendingSmartSearchQuery(bool force)\n', 'static std::string nativeUiPreviewBlocksKey('),
  extract('static void nativeUiCloseSmartSearch(bool returnToSource)\n', 'static bool nativeUiRequestTimerToggle('),
  extract('static bool nativeUiActivateSmartSearchResult(\n', 'static bool nativeApplySmartSearchCommand('),
  extract('static bool nativeApplySmartSearchCommand(\n', 'static std::string nativeBuildSmartSearchStateJson()')
].join('\n'));
const executable = path.join(temporary, 'search-test.exe');
const result = spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/utf-8',
  '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'search-test.obj'),
  path.join(__dirname, 'native_smart_search_origin_test.cpp')], { cwd: temporary, encoding: 'utf8' });
process.stdout.write(result.stdout || '');
process.stderr.write(result.stderr || '');
if (result.error) throw result.error;
if (result.status !== 0) process.exit(result.status || 1);
const test = spawnSync(executable, [], { cwd: temporary, encoding: 'utf8' });
process.stdout.write(test.stdout || '');
process.stderr.write(test.stderr || '');
if (test.error) throw test.error;
process.exit(test.status || 0);
