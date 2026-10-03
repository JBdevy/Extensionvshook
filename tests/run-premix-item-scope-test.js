// Test actual native UI/JSON row builders against the same in-memory project.
// Optional source path supports reproducing failures against an older revision.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(
  process.argv[2] || path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8'
).replace(/\r\n/g, '\n');
function extract(from, to) {
  const start = source.indexOf(from);
  const end = source.indexOf(to, start + from.length);
  assert(start >= 0 && end > start, `Production function/type not found: ${from}`);
  return source.slice(start, end);
}
const jsonBuilder = extract('static std::string nativeBuildPremixItemRowsJson(',
  'static std::string nativeMixerTimelineCatalogSourceSignature()');
const hasGridScope = jsonBuilder.includes('bool includeOverlappingItems');
if (hasGridScope) {
  assert.match(source,
    /timelineItems\s*=\s*nativeBuildPremixItemRowsJson\(activeProject,\s*mixerTimelineSong,\s*true\)/,
    'Mixer timeline must explicitly request overlapping items');
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-premix-scope-'));
try {
  fs.writeFileSync(path.join(temporary, 'premix-types.h'), [
    extract('struct NativeSongWindow {', 'struct NativePendingPlayTarget {'),
    extract('enum class NativeUiPremixRowKind {', 'struct NativeUiPremixSliderHit {')
  ].join('\n'));
  fs.writeFileSync(path.join(temporary, 'premix-scope-under-test.h'), [
    `#define PREMIX_HAS_EXPLICIT_GRID_SCOPE ${hasGridScope ? 1 : 0}`,
    extract('static std::string nativeMixerTrackFilterKey(std::string trackName)\n',
      'static bool nativeUiOpenPremixFromRename()\n'), jsonBuilder
  ].join('\n'));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'premix-scope.exe' : 'premix-scope');
  const fixture = path.join(__dirname, 'native_premix_item_scope_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary,
      '/Fe:' + executable, '/Fo:' + path.join(temporary, 'premix-scope.obj'), fixture]
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', temporary, fixture, '-o', executable];
  for (const [command, parameters] of [
    [windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]
  ]) {
    const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
    process.stdout.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) {
      process.exitCode = result.status || 1;
      break;
    }
  }
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
