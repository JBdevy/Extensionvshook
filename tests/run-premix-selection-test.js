// Exercise production Premix selection and response identity without REAPER.
// An optional source path permits checking the same regression against a baseline.
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

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-premix-selection-'));
try {
  fs.writeFileSync(path.join(temporary, 'premix-song-type.h'),
    extract('struct NativeSongWindow {', 'struct NativePendingPlayTarget {'));
  fs.writeFileSync(path.join(temporary, 'premix-selection-under-test.h'), [
    extract('static bool nativePremixGlobalEnabled()\n', '// O grid do Mixer segue'),
    extract('static std::string nativeBuildPremixJson(ReaProject* project, const std::vector<NativeSongWindow>& songs)\n',
      'static std::string nativeMultiLoopEscape(')
  ].join('\n'));

  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'premix-selection.exe' : 'premix-selection');
  const fixture = path.join(__dirname, 'native_premix_selection_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary,
      '/Fe:' + executable, '/Fo:' + path.join(temporary, 'premix-selection.obj'), fixture]
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
