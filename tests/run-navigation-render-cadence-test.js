const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8')
  .replace(/\r\n/g, '\n');
function extract(start, end) {
  const a = source.indexOf(start);
  const b = source.indexOf(end, a);
  if (a < 0 || b < a) throw new Error('Production implementation not found: ' + start);
  return source.slice(a, b);
}

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-navigation-cadence-'));
try {
  fs.writeFileSync(path.join(temporary, 'navigation-cadence-under-test.h'),
    extract('static bool nativeUiAdvanceSmoothScroll(\n',
      'static bool nativeUiAdvanceSmoothScrolls()') + '\n' +
    extract('static bool nativeUiNavigationKeyIsRepeat(LPARAM keyData)\n',
      'static void nativeUiSmartSearchRememberScrollTarget(') + '\n' +
    extract('static void nativeUiRefreshNavigationNow(HWND hwnd)\n',
      'static bool nativeUiInsertTextAtCursor(') + '\n' +
    extract('static void nativeUiResetNavigationRepeat(int releasedKey)\n',
      'static bool nativeUiSmoothScrollActive()'));
  const windows = process.platform === 'win32';
  const fixture = path.join(__dirname, 'native_navigation_render_cadence_test.cpp');
  for (const platform of ['windows', 'mac']) {
    const executable = path.join(temporary, `navigation-${platform}${windows ? '.exe' : ''}`);
    const define = platform === 'mac' ? 'NAVIGATION_TEST_MAC' : 'NAVIGATION_TEST_WINDOWS';
    const args = windows
      ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/D' + define, '/I' + temporary,
        '/Fe:' + executable, '/Fo:' + path.join(temporary, platform + '.obj'), fixture]
      : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-D' + define,
        '-I', temporary, fixture, '-o', executable];
    for (const [command, parameters] of [
      [windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []],
    ]) {
      const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
      process.stdout.write(result.stdout || '');
      process.stderr.write(result.stderr || '');
      if (result.error) throw result.error;
      if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
    }
  }
} finally {
  const resolved = path.resolve(temporary);
  const expectedParent = path.resolve(os.tmpdir()) + path.sep;
  if (!resolved.startsWith(expectedParent) ||
      !path.basename(resolved).startsWith('vshook-navigation-cadence-')) {
    throw new Error('Refusing to remove an unexpected temporary directory: ' + resolved);
  }
  fs.rmSync(resolved, { recursive: true, force: true });
}
