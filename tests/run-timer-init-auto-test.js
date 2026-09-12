const fs = require('node:fs')
const os = require('node:os')
const path = require('node:path')
const assert = require('node:assert/strict')
const { spawnSync } = require('node:child_process')
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8').replace(/\r\n/g, '\n')
const extract = (from, to) => {
  const start = source.indexOf(from)
  const end = source.indexOf(to, start + from.length)
  assert(start >= 0 && end > start, 'Funcao de timer ausente: ' + from)
  return source.slice(start, end)
}
assert.match(source, /nativeTimerObservePlaybackLocked\(\s*activeProject, playing, paused, playingId\)/)
assert.match(source, /"timer_set_init_auto" && type != "timer_init_auto_toggle"/)
assert.match(source, /SetExtState_ptr\(kLuaWindowExtStateSection,\s*"TIMER_INIT_AUTO_V1",[\s\S]{0,100}?true\)/)
assert.match(source, /GetExtState_ptr\(\s*kLuaWindowExtStateSection, "TIMER_INIT_AUTO_V1"\)/)
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-timer-test-'))
fs.writeFileSync(path.join(temporary, 'timer-under-test.h'), [
  extract('static void nativeTimerStartLocked()\n', 'static bool nativeApplyTimerCommand(const std::string& commandBody)\n{'),
].join('\n'))
const executable = path.join(temporary, 'timer-test.exe')
const compile = spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/utf-8',
  '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'timer-test.obj'),
  path.join(__dirname, 'native_timer_init_auto_test.cpp')], { cwd: temporary, encoding: 'utf8' })
process.stdout.write(compile.stdout || '')
process.stderr.write(compile.stderr || '')
if (compile.error) throw compile.error
if (compile.status !== 0) process.exit(compile.status || 1)
const test = spawnSync(executable, [], { cwd: temporary, encoding: 'utf8' })
process.stdout.write(test.stdout || '')
process.stderr.write(test.stderr || '')
if (test.error) throw test.error
process.exit(test.status || 0)

