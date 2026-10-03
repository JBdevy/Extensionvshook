const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
function extract(start, end) {
  const a = source.indexOf(start), b = source.indexOf(end, a);
  if (a < 0 || b < a) throw new Error('Production implementation not found: ' + start);
  return source.slice(a, b);
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-passive-state-'));
try {
  const routeStart = source.indexOf('} else if (path == "/state" ||');
  const route = source.slice(source.indexOf('\n', routeStart),
    source.indexOf('    std::string snapshot;', routeStart));
  const runtime = extract('  const bool runtimeActive = nativeIsRuntimeControlActive();',
    '  // Espera alguns ciclos para o REAPER terminar');
  fs.writeFileSync(path.join(temporary, 'passive-state-under-test.h'),
    extract('static bool nativeIsRuntimeControlActive()\n{', 'static std::string nativePremixSongKey(') + '\n' +
    extract('static std::atomic<int64_t> g_nativeLastStateClientReadMs', 'static bool VS_Hook_Native_SetActivePlaylist(') + '\n' +
    extract('static std::string nativeRequestTarget(', 'static std::string nativeMimeTypeForPath(') + '\n' +
    'static void stateRequest(const std::string& req) {\n' + route + '\n}\n' +
    'static void runtimeTimerBody() {\n' + runtime + '\n}\n');
  const rebuild = extract('static void nativeRebuildState(',
    'static std::atomic<int64_t> g_nativeLastStateClientReadMs');
  function effect(start, end) {
    const a = rebuild.indexOf(start), b = rebuild.indexOf(end, a);
    if (a < 0 || b < a) throw new Error('State effect not found: ' + start);
    return rebuild.slice(a, b);
  }
  fs.writeFileSync(path.join(temporary, 'passive-effects-under-test.h'),
    effect('    if (allowRuntimeControl) nativeProcessTunerCommands(', '    const auto tunerOffsets') +
    effect('  if (allowRuntimeControl) nativeUpdateLiveMarkTracker(', '  nativeApplyLiveExecutedFlags(') +
    effect('  bool wasPlayingBeforeNativeState = false;', '  if (rebuildSnapshot) {') +
    effect('  const bool queueSeekPerformed =', '  const std::string& mixerJson') +
    effect('  if (allowRuntimeControl && partsArrived)', '  std::string queuedSongId;') +
    effect('    if (allowRuntimeControl) {\n      nativeTimerObservePlaybackLocked(', '    timerRunning ='));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'passive-state.exe' : 'passive-state');
  const fixture = path.join(__dirname, 'native_passive_state_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary,
      '/Fe:' + executable, '/Fo:' + path.join(temporary, 'test.obj'), fixture]
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
