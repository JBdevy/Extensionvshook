const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
function extract(start, end) {
  const a = source.indexOf(start);
  const b = source.indexOf(end, a);
  if (a < 0 || b < a) throw new Error('Production implementation not found: ' + start);
  return source.slice(a, b);
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-idle-test-'));
try {
  fs.writeFileSync(path.join(temporary, 'idle-work-under-test.h'),
    extract('static bool nativeManualStopFadeoutTrackIdIsHidden(',
      'static void nativeWriteManualStopFadeoutTrackIds(') + '\n' +
    extract('static std::atomic<int64_t> g_nativeLastStateClientReadMs',
      'static bool VS_Hook_Native_SetActivePlaylist(') + '\n' +
    extract('static int nativeWaitForBridgeConnection(',
      'static void nativeBridgeServerThread()'));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'idle-test.exe' : 'idle-test');
  const fixture = path.join(__dirname, 'native_idle_work_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary,
      '/Fe:' + executable, '/Fo:' + path.join(temporary, 'test.obj'), fixture, 'ws2_32.lib']
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', temporary, fixture, '-o', executable];
  for (const [command, parameters] of [
    [windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []],
  ]) {
    const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
    process.stdout.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
