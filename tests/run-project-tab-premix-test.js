// Exercise real project switching and Premix project validation without REAPER.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8').replace(/\r\n/g, '\n');
function extract(from, to) {
  const start = source.indexOf(from);
  const end = source.indexOf(to, start + from.length);
  assert(start >= 0 && end > start, from);
  return source.slice(start, end);
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-project-premix-'));
try {
  fs.writeFileSync(path.join(temporary, 'project-premix-under-test.h'), [
    extract('static bool nativePremixCommandMatchesCurrentProject(', 'static bool nativeApplyPremixCommand(const std::string& commandBody)\n'),
    extract('static std::string nativeObserveProjectTabChange()\n', 'static void startupTimer()\n'),
    extract('static bool nativeSelectProjectFromCommand(', 'static bool nativeSaveProjectFromCommand('),
  ].join('\n'));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'project-premix.exe' : 'project-premix');
  const fixture = path.join(__dirname, 'native_project_tab_premix_test.cpp');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary,
      '/Fe:' + executable, '/Fo:' + path.join(temporary, 'project-premix.obj'), fixture]
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', temporary, fixture, '-o', executable];
  for (const [command, parameters] of [[windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]]) {
    const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
    process.stdout.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) { process.exitCode = result.status || 1; break; }
  }
} finally { fs.rmSync(temporary, { recursive: true, force: true }); }
