const fs = require('fs')
const path = require('path')

const sourcePath = path.resolve(__dirname, '..', 'src', 'vshook_extension.cpp')
const source = fs.readFileSync(sourcePath, 'utf8')
const start = source.indexOf('static void nativeRefreshAppActivePanelModel()')
const end = source.indexOf('static void nativeSetRepeatEnabled(', start)
const block = source.slice(start, end)

if (start < 0 || end < 0 ||
    !source.includes('g_nativeUiPanelModelSourceDirty{true}') ||
    !block.includes('g_nativeUiPanelModelSourceDirty.exchange(') ||
    !block.includes('if (!sourceDirty &&') ||
    !block.includes('(activeModel ? 100 : 250)')) {
  throw new Error('controle incremental do modelo do painel nativo nao foi encontrado')
}

const positionWrite = source.indexOf('g_nativeCurrentPlayPosition = playPos;')
const dirtyWrite = source.indexOf('g_nativeUiPanelModelSourceDirty.store(', positionWrite)
if (positionWrite < 0 || dirtyWrite < positionWrite || dirtyWrite - positionWrite > 300) {
  throw new Error('snapshot nao invalida o modelo do painel depois de atualizar o transporte')
}

if (!source.includes('configNow - lastConfigRead).count() < 250') ||
    !source.includes('nativeTimecodeLanRefreshConfigOnMainThread(true);')) {
  throw new Error('configuracao LAN voltou a ser relida em cada tick')
}
if (!source.includes('now - lastHeartbeatRead).count() < 100')) {
  throw new Error('heartbeat Lua voltou a consultar ExtState em cada tick')
}

console.log(`NATIVE_PANEL_MODEL_THROTTLE_OK: ${sourcePath}`)
