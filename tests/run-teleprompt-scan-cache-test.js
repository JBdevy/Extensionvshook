// Run from a Visual Studio developer environment (cl available).
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
const start = source.indexOf('struct NativeTelepromptScanItem {');
const end = source.indexOf('static std::string nativeBuildTelepromptStateJson(', start);
if (start < 0 || end < start) throw new Error('Production scan implementation not found');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-scan-test-'));
fs.writeFileSync(path.join(temporary, 'teleprompt-scan-under-test.h'), source.slice(start, end));
const executable = path.join(temporary, 'scan-test.exe');
const result = spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/utf-8',
  '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'scan-test.obj'),
  path.join(__dirname, 'native_teleprompt_scan_cache_test.cpp')], { cwd: temporary, encoding: 'utf8' });
if (result.error) throw result.error;
process.stdout.write(result.stdout || '');
process.stderr.write(result.stderr || '');
if (result.status !== 0) process.exit(result.status || 1);
const test = spawnSync(executable, [], { cwd: temporary, encoding: 'utf8' });
process.stdout.write(test.stdout || '');
process.stderr.write(test.stderr || '');
if (test.error) throw test.error;
process.exit(test.status || 0);
