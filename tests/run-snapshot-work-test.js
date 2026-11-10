const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
function extract(from, to) {
  const start = source.indexOf(from);
  const end = source.indexOf(to, start + from.length);
  assert(start >= 0 && end > start, from);
  return source.slice(start, end);
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-snapshot-work-'));
try {
  fs.writeFileSync(path.join(temporary, 'lyrics-under-test.h'),
    extract('static std::string nativeMediaItemNotes(MediaItem* item)\n',
      'static bool nativeItemStartsInRange('));
  // Run the real snapshot invalidation and cache-commit code with a fake host.
  const decision = extract('  static ReaProject* cachedSnapshotProject = nullptr;',
    '    cachedSongs = nativeCollectProjectSongs(activeProject, cachedMarkersJson);');
  fs.writeFileSync(path.join(temporary, 'snapshot-under-test.h'),
    'static bool snapshotTick(ReaProject* activeProject, bool forceSnapshot = false) {\n' +
    decision + '\n    ++builds;\n  }\n  return rebuildSnapshot;\n}\n');
  const collection = extract('static std::vector<NativeSongWindow> nativeCollectProjectSongs(',
    'static void showResolumeColumnCountForProject()');
  assert(collection.includes('NativeLyricsRangeReader lyrics(project);'));
  assert(collection.includes('s.lyricsText = lyrics.forRange(s.start, s.end);'));
  const windows = process.platform === 'win32';
  for (const name of ['native_lyrics_snapshot_test', 'native_empty_snapshot_test']) {
    const executable = path.join(temporary, name + (windows ? '.exe' : ''));
    const fixture = path.join(__dirname, name + '.cpp');
    const args = windows
      ? ['/nologo', '/std:c++17', '/O2', '/EHsc', '/utf-8', '/I' + temporary,
        '/Fe:' + executable, '/Fo:' + path.join(temporary, name + '.obj'), fixture]
      : ['-std=c++17', '-O2', '-I', temporary, fixture, '-o', executable];
    for (const [command, parameters] of [
      [windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]
    ]) {
      const result = spawnSync(command, parameters, { cwd: temporary, encoding: 'utf8' });
      process.stdout.write(result.stdout || '');
      process.stderr.write(result.stderr || '');
      if (result.error) throw result.error;
      if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
    }
  }
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
