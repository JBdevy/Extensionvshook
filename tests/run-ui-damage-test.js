// Run from a Visual Studio developer environment. Outputs only to OS temp.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-ui-damage-test-'));
const executable = path.join(temporary, 'ui-damage-test.exe');
const compile = spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/utf-8',
  '/Fe:' + executable, '/Fo:' + path.join(temporary, 'ui-damage-test.obj'),
  path.join(__dirname, 'native_ui_damage_test.cpp'), 'user32.lib', 'gdi32.lib'],
{ cwd: temporary, encoding: 'utf8' });
process.stdout.write(compile.stdout || '');
process.stderr.write(compile.stderr || '');
if (compile.error) throw compile.error;
if (compile.status !== 0) process.exit(compile.status || 1);
const result = spawnSync(executable, [], { cwd: temporary, encoding: 'utf8' });
process.stdout.write(result.stdout || '');
process.stderr.write(result.stderr || '');
if (result.error) throw result.error;
process.exit(result.status || 0);
