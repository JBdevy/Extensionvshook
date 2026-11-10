const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'),'utf8').replace(/\r\n/g, '\n');
const extract = (from,to) => { const a=source.indexOf(from),b=source.indexOf(to,a+from.length); assert(a>=0&&b>a,from); return source.slice(a,b); };
const temp = fs.mkdtempSync(path.join(os.tmpdir(),'vshook-control-mapping-'));
try {
  fs.writeFileSync(path.join(temp,'mapping-json-under-test.h'),extract('static std::string nativeJsonExtractString(const std::string& json, const std::string& key)\n','// Unlike nativeJsonExtractString()'));
  fs.writeFileSync(path.join(temp,'mapping-track-under-test.h'),extract('static MediaTrack* nativeProjectSyncResolveTrack(\n','\nstatic '));
  fs.writeFileSync(path.join(temp,'mapping-row-under-test.h'),extract('static std::string nativeUiMainRowKey(\n','static std::string nativeUiMainRowStableIdentity('));
  fs.writeFileSync(path.join(temp,'mapping-multiloop-under-test.h'),extract('static std::string nativeMultiLoopSongKey(', '\nstatic std::string nativeGetProjExtStateString'));
  const win=process.platform==='win32';
  const exe=path.join(temp,win?'mapping.exe':'mapping');
  const includes=[path.join(__dirname,'../src'),temp];
  const fixture=path.join(__dirname,'native_project_sync_mapping_test.cpp');
  const args=win?['/nologo','/std:c++17','/EHsc','/utf-8',...includes.map(p=>'/I'+p),'/Fe:'+exe,'/Fo:'+path.join(temp,'mapping.obj'),fixture]:['-std=c++17','-O2','-Wall','-Wextra',...includes.flatMap(p=>['-I',p]),fixture,'-o',exe];
  for(const [cmd,argv] of [[win?'cl.exe':(process.env.CXX||'c++'),args],[exe,[]]]) {
    const r=spawnSync(cmd,argv,{cwd:temp,encoding:'utf8'}); process.stdout.write(r.stdout||''); process.stderr.write(r.stderr||'');
    if(r.error)throw r.error; if(r.status!==0) {process.exitCode=r.status||1; break;}
  }
} finally { fs.rmSync(temp,{recursive:true,force:true}); }
