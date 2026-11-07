// Exercise the production modal lifecycle and playlist writes without REAPER.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8').replace(/\r\n/g, '\n');
function extract(from, to) {
  const start = source.indexOf(from), end = source.indexOf(to, start + from.length);
  assert(start >= 0 && end > start, `Function not found: ${from}`);
  return source.slice(start, end);
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-modal-lifecycle-'));
try {
  fs.writeFileSync(path.join(temporary, 'modal-types.h'), [
    extract('enum class NativeMainModalKind {', 'static NativeMainModalKind g_nativeMainModalKind'),
    extract('enum class NativeUiTextInputKind {', 'static NativeUiTextInputKind g_nativeUiTextSelectionKind'),
    extract('struct NativeUiPlaylistRecord {', 'struct NativeUiPlaylistRowHit {'),
    extract('enum class NativeUiSelectPlaylistMode {', 'static std::vector<NativeUiPlaylistRecord> g_nativeUiPlaylists')
  ].join('\n'));
  fs.writeFileSync(path.join(temporary, 'modal-under-test.h'), [
    extract('static void nativeUiCloseMainModal()\n{', 'static bool nativeUiNumberRegionModeFromExtState()'),
    extract('static void nativeUiOpenCreatePlaylist()\n', 'static std::vector<NativeSongWindow>\nnativeUiPlaylistSongsFromRegionSelection()'),
    extract('static bool nativeUiCreatePlaylistFromSelection()\n', 'static void nativeUiOpenSelectPlaylist('),
    extract('static bool nativeUiAddSelectionToPlaylist()\n', 'static std::vector<const NativeAppActivePanelModel::Row*>\nnativeUiSelectedPlaylistRowsForEdit()')
  ].join('\n'));
  const executable = path.join(temporary, process.platform === 'win32' ? 'modal-test.exe' : 'modal-test');
  const fixture = path.join(__dirname, 'native_modal_lifecycle_test.cpp');
  const compiler = process.platform === 'win32' ? 'cl.exe' : (process.env.CXX || 'c++');
  const flags = process.platform === 'win32'
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary, '/Fe:' + executable, '/Fo:' + path.join(temporary, 'modal-test.obj'), fixture]
    : ['-std=c++17', '-UNDEBUG', '-I', temporary, fixture, '-o', executable];
  for (const [command, args] of [[compiler, flags], [executable, []]]) {
    const result = spawnSync(command, args, { cwd: temporary, encoding: 'utf8' });
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) { process.exitCode = result.status || 1; break; }
  }
} finally { fs.rmSync(temporary, { recursive: true, force: true }); }
