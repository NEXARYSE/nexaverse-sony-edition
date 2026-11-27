const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs/promises'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto');
const {create,GAME_ID}=require('./gta6-runtime.cjs');
async function fixture(t){
 const root=await fs.mkdtemp(path.join(os.tmpdir(),'nexa-gta-gate-'));
 t.after(async()=>{const real=await fs.realpath(root);assert.equal(path.dirname(real).toLowerCase(),(await fs.realpath(os.tmpdir())).toLowerCase());assert(path.basename(real).startsWith('nexa-gta-gate-'));await fs.rm(real,{recursive:true,force:true});});
 const runtimeRoot=path.join(root,'runtime');await fs.mkdir(path.join(runtimeRoot,'dxil'),{recursive:true});
 for(const n of ['NEXA GTA.exe','NEXA GTA Core.exe','NEXA GTA.ico','dxil/dxil.dll','dxil/dxcompiler.dll'])await fs.writeFile(path.join(runtimeRoot,n),'synthetic fixture, never executed');
 const data=Buffer.alloc(128);data.write('MZ');data.writeUInt32LE(64,60);data.writeUInt32LE(0x4550,64);data.writeUInt16LE(0x8664,68);data.writeUInt16LE(0x20b,88);
 const image=path.join(root,'game_scarlett_master.exe');await fs.writeFile(image,data);const reference={name:path.basename(image),size:data.length,sha256:crypto.createHash('sha256').update(data).digest('hex')};
 return {root,runtimeRoot,image,data,runtime:create({runtimeRoot,appData:root,reference})};
}
test('no linked Xbox image means no enabled executable, even with a stale Windows exe',async t=>{const f=await fixture(t);const state=await f.runtime.libraryState({id:GAME_ID,exe:'GTA5.exe'});assert.equal(state.gtaLinked,false);assert.equal(state.exe,undefined);await assert.rejects(f.runtime.launchPlan({id:GAME_ID,exe:f.image}));});
test('supported image enables only GTA VI and launches the runtime with an argv path',async t=>{const f=await fixture(t);const game={id:GAME_ID,gtaXboxExecutable:f.image,gtaBackend:'nexa'};const state=await f.runtime.libraryState(game);assert.equal(state.gtaLinked,true);assert.equal(state.exe,f.runtime.executable());const plan=await f.runtime.launchPlan(game);assert.equal(plan.exe,f.runtime.executable());assert.deepEqual(plan.args,[await fs.realpath(f.image)]);assert.equal(plan.env.NEXA_GRAPHICS,undefined);assert.equal(plan.env.NEXA_GTA_BACKEND,'victory');assert.equal(plan.env.NEXA_PIPELINE_TRACE,undefined);await assert.rejects(f.runtime.launchPlan({...game,id:'gta-v'}),/restricted/);});
test('renaming another PE to the expected name cannot bypass the fingerprint',async t=>{const f=await fixture(t);const wrong=Buffer.from(f.data);wrong[100]=1;await fs.writeFile(f.image,wrong);await assert.rejects(f.runtime.validateExecutable(f.image),/Unsupported/);assert.equal((await f.runtime.libraryState({gtaXboxExecutable:f.image})).gtaLinked,false);});
test('changed, removed and truncated linked files are refused at launch',async t=>{const f=await fixture(t);const game={id:GAME_ID,gtaXboxExecutable:f.image};assert.equal((await f.runtime.libraryState(game)).gtaLinked,true);await fs.writeFile(f.image,Buffer.alloc(128));await assert.rejects(f.runtime.launchPlan(game),/Invalid/);await fs.writeFile(f.image,'MZ');assert.equal((await f.runtime.libraryState(game)).gtaLinked,false);await fs.unlink(f.image);await assert.rejects(f.runtime.launchPlan(game));});
test('other executable names and missing runtime files are refused',async t=>{const f=await fixture(t);const other=path.join(f.root,'GTA5_Enhanced.exe');await fs.writeFile(other,f.data);await assert.rejects(f.runtime.validateExecutable(other),/supported/);await fs.unlink(path.join(f.runtimeRoot,'NEXA GTA Core.exe'));await assert.rejects(f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image}));});

test('legacy and software preferences cannot select another runtime',async t=>{
 const f=await fixture(t);const game={id:GAME_ID,gtaXboxExecutable:f.image};
 for(const backend of ['nexa','xcl','victory',undefined]){
  const plan=await f.runtime.launchPlan({...game,gtaBackend:backend,gtaSoftwareRenderer:true});
  assert.equal(plan.env.NEXA_GTA_BACKEND,'victory');assert.equal(plan.env.VICTORY_NEXA,'1');assert.equal(plan.env.VICTORY_D3D12_WARP,undefined);assert.equal(plan.env.VICTORY_MEMORY_MAP,'0');
  assert(plan.env.VICTORY_CACHE_DIR.startsWith(f.root));assert.equal(plan.env.VICTORY_GAME_DIR,path.dirname(f.image));
 }
});
test('inherited diagnostic shader flags are cleared while Windows environment survives',async t=>{
 const f=await fixture(t);const keys=['VICTORY_WAVE_WIDE','VICTORY_KEYS','VICTORY_CAPTURE_WORLD','NEXA_MENU_EVENT','XCL_TEST','XV_INPUT_PRESS'];
 const before=Object.fromEntries(keys.map(k=>[k,process.env[k]]));for(const key of keys)process.env[key]='diagnostic';
 try {const plan=await f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image});for(const key of keys)assert.equal(plan.env[key],undefined);assert(plan.env[Object.keys(plan.env).find(k=>k.toLowerCase()==='path')]===process.env.PATH);}
 finally {for(const key of keys)if(before[key]===undefined)delete process.env[key];else process.env[key]=before[key];}
});
test('a missing runtime dependency disables Play and blocks launch',async t=>{
 const f=await fixture(t);const game={id:GAME_ID,gtaXboxExecutable:f.image};await fs.unlink(path.join(f.runtimeRoot,'dxil','dxcompiler.dll'));
 assert.equal((await f.runtime.libraryState(game)).gtaLinked,false);await assert.rejects(f.runtime.launchPlan(game));
});


test('saved rendering choices select 720p temporal upscaling or native 1080p',async t=>{
 const f=await fixture(t);for(const [resolution,dimensions] of [[undefined,'1920x1080'],['720p','1920x1080'],['1080p','1920x1080'],['invalid','1920x1080'],[['1080p'],'1920x1080']]){
  const game={id:GAME_ID,gtaXboxExecutable:f.image,gtaRenderResolution:resolution};const plan=await f.runtime.launchPlan(game);
  assert.equal(plan.env.VICTORY_DISPLAY_MODE,dimensions);assert.equal(plan.env.VICTORY_OUTPUT,dimensions);assert.equal(plan.env.VICTORY_RENDER_SCALE,resolution==='1080p'?'1.0':'0.6666666666666666');assert.equal(plan.env.VICTORY_OWN_UPSCALER,'1');
  const state=await f.runtime.libraryState(game);assert.equal(state.gtaRenderResolution,resolution==='1080p'?'1080p':'720p');
 }
});

test('release profile retains tested rendering flags and never enables the experiments',async t=>{
 const f=await fixture(t);const plan=await f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image});
 for(const [key,value] of Object.entries({NEXA_GTA_CONSOLE:'series-s',VICTORY_HEAP_WORKER:'0',VICTORY_SCRATCH_ANY_SIZE:'0',VICTORY_TEXTURE_DROP_MIPS:'0',VICTORY_RELAY:'1',VICTORY_RAYS:'0',VICTORY_RENDER_SCALE:'0.6666666666666666'}))assert.equal(plan.env[key],value,key);
 assert.equal(plan.env.VICTORY_KEYS,undefined);assert.equal(plan.env.VICTORY_CAPTURE,undefined);
});

test('graphics migration backs up existing settings and preserves unrelated options and saves',async t=>{
 const f=await fixture(t);const folder=path.join(f.root,'NEXA GTA','victory','data','7952987A','storage','settings');await fs.mkdir(folder,{recursive:true});
 const file=path.join(folder,'settings.xml');const before='<root><hdr value="true" /><lodScale value="1.000000" /><motionBlur value="true" /><audio value="77" /></root>';
 await fs.writeFile(file,before);const save=path.join(folder,'save.fixture');await fs.writeFile(save,'untouched progress');
 await f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image});const after=await fs.readFile(file,'utf8');
 assert.match(after,/<hdr value="false"/);assert.match(after,/<lodScale value="0.500000"/);assert.match(after,/<motionBlur value="false"/);assert.match(after,/<audio value="77"/);
 assert.equal(await fs.readFile(file+'.before-0.1.89','utf8'),before);assert.equal(await fs.readFile(save,'utf8'),'untouched progress');
 await f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image});assert.equal(await fs.readFile(file+'.before-0.1.89','utf8'),before);
});

test('local storage selection retains cache and data outside the installed runtime',async t=>{
 const f=await fixture(t);const location={cacheDir:path.join(f.root,'custom-cache'),dataDir:path.join(f.root,'custom-data')};
 await fs.mkdir(path.join(f.root,'NEXA GTA'),{recursive:true});await fs.writeFile(path.join(f.root,'NEXA GTA','7900-storage.json'),JSON.stringify(location));
 const plan=await f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image});assert.equal(plan.env.VICTORY_CACHE_DIR,location.cacheDir);assert.equal(plan.env.VICTORY_DATA_DIR,location.dataDir);
 await fs.access(path.join(location.dataDir,'7952987A','storage','settings','settings.xml'));
});
test('launch identity uses the packaged launcher version instead of inherited identifiers',async t=>{
 const f=await fixture(t);const old=process.env.NEXA_LAUNCHER_VERSION;process.env.NEXA_LAUNCHER_VERSION='0.0.0-stale';
 try{const plan=await f.runtime.launchPlan({id:GAME_ID,gtaXboxExecutable:f.image});assert.equal(plan.env.NEXA_LAUNCHER_VERSION,require('./package.json').version)}finally{if(old===undefined)delete process.env.NEXA_LAUNCHER_VERSION;else process.env.NEXA_LAUNCHER_VERSION=old}
});
