const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(process.env.VSHOOK_STOP_SOURCE || path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
const extract = (start, end) => {
  const first = source.indexOf(start);
  const last = source.indexOf(end, first + start.length);
  if (first < 0 || last <= first) throw new Error('Production segment missing: ' + start);
  return source.slice(first, last);
};
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-auto-stop-'));
try {
  fs.writeFileSync(path.join(temporary, 'stop-types.h'), extract('struct NativeSongWindow {', 'static std::vector<NativeSongWindow> g_nativeSongWindows;'));
  fs.writeFileSync(path.join(temporary, 'stop-resolver.h'), extract('static std::string nativeFindPlayingId(const', 'static std::string nativeStripJsonObjectBraces('));
  fs.writeFileSync(path.join(temporary, 'stop-runtime.h'), extract('static void nativeMaintainAutoStop(', 'static bool nativeIsRepeatEnabled(ReaProject* project)\n{'));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'test.exe' : 'test');
  const fixture = path.join(__dirname, 'native_auto_stop_start_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'test.obj'), fixture]
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', temporary, fixture, '-o', executable];
  for (const [command, parameters] of [[windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]]) {
    const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally { fs.rmSync(temporary, { recursive: true, force: true }); }
