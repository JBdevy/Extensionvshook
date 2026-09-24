// Compile the actual clone/selection functions with an in-memory project fixture.
// Run from a VS Developer Command Prompt, like the other native UI fixtures.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8').replace(/\r\n/g, '\n');
function extract(from, to) {
  const start = source.indexOf(from);
  const end = source.indexOf(to, start + from.length);
  assert(start >= 0 && end > start, `Function not found: ${from}`);
  return source.slice(start, end);
}
assert.match(source, /RECT cloneButton\{multiButton\.left - modalButtonGap - topButtonW/);
assert.match(source, /addModalButton\("playlist_clone", "Clone", cloneButton,\s*"copy", true, selectedEditable\)/);
assert.match(source, /action == "playlist_clone"\) \{\s*nativeUiCloneSelectedPlaylist\(\)/);
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-playlist-clone-'));
fs.writeFileSync(path.join(temporary, 'playlist-types.h'), extract(
  'struct NativeUiPlaylistRecord {', 'struct NativeUiPlaylistRowHit {'));
fs.writeFileSync(path.join(temporary, 'playlist-clone-under-test.h'), [
  extract('static bool nativeUiPlaylistRecordBelongsToProject(\n', 'static bool nativeUiOpenPlaylistDropdown()\n'),
  extract('static std::string nativeUiNextPlaylistCloneName(\n', 'static bool nativeUiRenameSelectedPlaylist()\n')
].join('\n'));
const executable = path.join(temporary, 'playlist-clone.exe');
const compile = spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/utf-8',
  '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'playlist-clone.obj'),
  path.join(__dirname, 'native_playlist_clone_test.cpp')], { cwd: temporary, encoding: 'utf8' });
process.stdout.write(compile.stdout || '');
process.stderr.write(compile.stderr || '');
if (compile.error) throw compile.error;
if (compile.status !== 0) process.exit(compile.status || 1);
const result = spawnSync(executable, [], { cwd: temporary, encoding: 'utf8' });
process.stdout.write(result.stdout || '');
process.stderr.write(result.stderr || '');
if (result.error) throw result.error;
process.exit(result.status || 0);
