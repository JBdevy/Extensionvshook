const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
function functionBody(name) {
  const pattern = new RegExp('^static\\s+[^;{}]*?\\b' + name + '\\s*\\([^;{}]*?\\)\\s*\\{', 'm');
  const match = pattern.exec(source);
  assert(match, 'Production implementation missing: ' + name);
  let depth = 1, quote = '', comment = '';
  for (let i = match.index + match[0].length; i < source.length; ++i) {
    const ch = source[i], pair = source.slice(i, i + 2);
    if (comment === '//') { if (ch === '\n') comment = ''; }
    else if (comment === '/*') { if (pair === '*/') { comment = ''; ++i; } }
    else if (quote) { if (ch === '\\') ++i; else if (ch === quote) quote = ''; }
    else if (pair === '//' || pair === '/*') { comment = pair; ++i; }
    else if (ch === '"' || ch === "'") quote = ch;
    else if (ch === '{') ++depth;
    else if (ch === '}' && --depth === 0) return source.slice(match.index, i + 1);
  }
  throw new Error('Unclosed implementation: ' + name);
}
// Integration guards: retired entry points cannot survive behind another caller.
for (const retired of ['nativeProjectSyncRefreshManifestOnMainThread',
  'nativeProjectSyncPrepareBundleFromCommand', 'nativeProjectSyncApplyBundleFromCommand',
  'nativeProjectSyncPollLiveProjectOnMainThread', 'nativeProjectSyncCommitPendingOpenOnMainThread',
  'nativeProjectSyncPublishInitialPlaylistOnMainThread', 'nativePublishPlaylistSnapshot']) {
  assert(!source.includes(retired + '('), 'Retired project/media path remains: ' + retired);
}
const status = functionBody('nativeTimecodeLanStatusJson');
assert(!/manifest|structuralManifest|Bundle|PreflightRequest|projectSyncApply/.test(status));
const dispatch = functionBody('nativeApplyHttpCommandOnMainThread');
assert(dispatch.indexOf('nativeProjectSyncIsRemovedCommand') < dispatch.indexOf('nativeObserveProjectTabChange'));
const inbox = source.slice(source.indexOf('path == "/timecode/inbox"'), source.indexOf('path == "/command"'));
assert(inbox.includes('!nativeProjectSyncIsControlCommand(commandType)'));
assert(inbox.includes('NativeControlMapping::setField(command, "__vshookLanRemote", "true")'));
assert(inbox.indexOf('!nativeProjectSyncIsControlCommand') < inbox.indexOf('nativeQueueHttpCommandBatchAtomic'));
assert(!source.includes('addModalButton("project_sync_apply"'));
assert(!source.includes('addModalButton("project_sync_apply_config"'));

// Native automation must remain local even when it reuses a public handler.
for (const name of ['nativeBridgeTick', 'nativeUpdateManualStopFadeoutOnMainThread',
  'nativeProcessMultiLoopsOnMainThread', 'nativePrepareQueuedTunerNearRegionEndOnMainThread']) {
  assert(functionBody(name).includes('NativeProjectSyncAutomaticScope localAutomation'));
}
assert(!source.includes('nativeProjectSyncPollViewportOnMainThread('));
assert(functionBody('nativeApplyProjectSyncLiveCommand').indexOf('GetSet_ArrangeView2_ptr') < 0);
assert(dispatch.includes('!nativeProjectSyncIsIndependentControlCommand(syncCommandType)'));
assert(dispatch.includes('nativeProjectSyncCommandIsAutomatic(commandBody)'));
const transport = functionBody('nativeApplyTransportCommand');
assert(transport.indexOf('return nativeApplyTransportCommand(blockPlayCommand)') <
  transport.indexOf('nativeTimecodeLanRecordCommand(outgoingControl)'));
assert(transport.includes('"desiredPlaying", wantsPlay ? "true" : "false"'));
assert(transport.indexOf('nativeTimecodeLanPollEditCursorOnMainThread();') <
  transport.indexOf('nativeTimecodeLanRecordCommand(outgoingControl)'));
assert(functionBody('nativeProcessPendingSelectionOnMainThread').includes('!editCursorOnlyWhilePlaying'));
const received = functionBody('nativeApplyTimecodeLanCommand');
assert(received.includes(': g_nativeTimecodeLanMode == "receive";'));

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-controls-test-'));
try {
  const names = ['nativeJsonEscape', 'nativeJsonString', 'nativeTrim', 'nativeLower',
    'nativeJsonExtractString', 'nativeBoolFromText', 'nativeProjectSyncCommandIsAutomatic', 'nativeTimecodeLanRecordCommand', 'nativeTimecodeLanPollEditCursorOnMainThread', 'nativeTimecodeLanStatusJson', 'nativeTimecodeLanOutboxJson', 'nativeApplyAutoCommand'];
  fs.writeFileSync(path.join(temporary, 'controls-under-test.h'), names.map(functionBody).join('\n\n'));
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'test.exe' : 'test');
  fs.writeFileSync(path.join(temporary, 'cursor-under-test.h'), functionBody('nativeProcessPendingSelectionOnMainThread'));
  for (const fixtureName of ['native_project_sync_controls_test.cpp', 'native_project_sync_cursor_test.cpp']) {
  const fixture = path.join(__dirname, fixtureName);
  const include = path.join(__dirname, '../src');
  const args = windows
    ? ['/nologo', '/std:c++17', '/EHsc', '/utf-8', '/I' + temporary, '/I' + include,
      '/Fe:' + executable, '/Fo:' + path.join(temporary, 'test.obj'), fixture]
    : ['-std=c++17', '-O2', '-Wall', '-Wextra', '-I', temporary, '-I', include, fixture, '-o', executable];
  for (const [command, parameters] of [
    [windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []],
  ]) {
    const result = spawnSync(command, parameters, {cwd: temporary, encoding: 'utf8'});
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
  }
} finally { fs.rmSync(temporary, {recursive: true, force: true}); }
