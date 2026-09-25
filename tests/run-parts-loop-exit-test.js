const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
const start = source.indexOf('static void nativeReleaseLoopAfterPartsArrival(');
const end = source.indexOf('static void nativeRebuildState(', start);
if (start < 0 || end < start) throw new Error('Production helper missing');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-parts-loop-'));
try {
  fs.writeFileSync(path.join(temporary, 'parts-loop-exit-under-test.h'), source.slice(start, end));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'test.exe' : 'test');
  const fixture = path.join(__dirname, 'native_parts_loop_exit_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'test.obj'), fixture]
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', temporary, fixture, '-o', executable];
  for (const [command, parameters] of [[windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]]) {
    const result = spawnSync(command, parameters, {cwd: temporary, encoding: 'utf8'});
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally { fs.rmSync(temporary, {recursive: true, force: true}); }
