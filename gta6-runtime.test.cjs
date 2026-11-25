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
  assert.equal(plan.env.NEXA_GTA_BACKEND,'victory');assert.equal(plan.env.VICTORY_NEXA,'1');assert.equal(plan.env.VICTORY_D3D12_WARP,undefined);
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
