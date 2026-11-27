const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs/promises'),path=require('node:path'),os=require('node:os'),crypto=require('node:crypto'),vm=require('node:vm');
const graphics=require('./gta6-graphics-options.js'),{create}=require('./gta6-runtime.cjs');
async function fixture(t,hardware={dlss:true,frameGeneration:true}){
 const root=await fs.mkdtemp(path.join(os.tmpdir(),'nexa-graphics-'));
 t.after(async()=>{const real=await fs.realpath(root);assert.equal(path.dirname(real).toLowerCase(),(await fs.realpath(os.tmpdir())).toLowerCase());assert(path.basename(real).startsWith('nexa-graphics-'));await fs.rm(real,{recursive:true,force:true})});
 const runtimeRoot=path.join(root,'runtime');await fs.mkdir(path.join(runtimeRoot,'dxil'),{recursive:true});
 for(const n of ['NEXA GTA.exe','NEXA GTA Core.exe','NEXA GTA.ico','dxil/dxil.dll','dxil/dxcompiler.dll'])await fs.writeFile(path.join(runtimeRoot,n),'synthetic; never executed');
 const bytes=Buffer.alloc(128);bytes.write('MZ');bytes.writeUInt32LE(64,60);bytes.writeUInt32LE(0x4550,64);bytes.writeUInt16LE(0x8664,68);bytes.writeUInt16LE(0x20b,88);
 const image=path.join(root,'game_scarlett_master.exe');await fs.writeFile(image,bytes);
 const reference={name:path.basename(image),size:bytes.length,sha256:crypto.createHash('sha256').update(bytes).digest('hex')};
 const runtime=create({runtimeRoot,appData:root,reference,probeGraphics:async()=>hardware});
 async function enable(){await fs.mkdir(path.join(runtimeRoot,'streamline'));for(const name of ['sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll','sl.dlss_g.dll','sl.reflex.dll','sl.pcl.dll','nvngx_dlssg.dll'])await fs.writeFile(path.join(runtimeRoot,'streamline',name),'synthetic; never loaded');await fs.writeFile(path.join(runtimeRoot,'graphics-capabilities.json'),JSON.stringify({streamline:true,frameGeneration:true}));}
 return {root,runtimeRoot,runtime,enable,game:{id:'gta-vi',gtaXboxExecutable:image}};
}
test('all four rendering modes reach display mode, render scale and stored output dimensions',async t=>{
 const f=await fixture(t);for(const [mode,spec]of Object.entries(graphics.resolutions)){
  const plan=await f.runtime.launchPlan({...f.game,gtaRenderResolution:mode});assert.equal(plan.env.VICTORY_OUTPUT,spec.output);assert.equal(plan.env.VICTORY_DISPLAY_MODE,spec.output);assert.equal(plan.env.VICTORY_RENDER_SCALE,spec.scale);
  const xml=await fs.readFile(path.join(plan.env.VICTORY_DATA_DIR,'7952987A/storage/settings/settings.xml'),'utf8');const section=xml.match(/<resolutionFullscreen>([\s\S]*?)<\/resolutionFullscreen>/)[1];const [w,h]=spec.output.split('x');assert(section.includes(`width value="${w}"`));assert(section.includes(`height value="${h}"`));
 }
});
test('DLSS and two-times generation use present cleanup, native DLAA, a VRAM reserve and no legacy scale override',async t=>{
 const f=await fixture(t);await f.enable();
 for(const mode of Object.keys(graphics.resolutions)){
  const plan=await f.runtime.launchPlan({...f.game,gtaRenderResolution:mode,gtaGraphics:{frameGeneration:'2x'}});
  assert.equal(plan.env.VICTORY_FRAMEGEN,'1');assert.equal(plan.env.VICTORY_FRAMEGEN_X,'2');assert.equal(plan.env.VICTORY_DLSS,'1');assert.equal(plan.env.VICTORY_STREAMLINE_PRESENT,'1');assert.equal(plan.env.VICTORY_DLSS_MODE,mode==='720p'?'quality':'dlaa');assert.equal(plan.env.VICTORY_TEXTURE_ROOM_PERCENT,'80');assert.equal(plan.env.VICTORY_OWN_UPSCALER,undefined);assert.equal(plan.env.VICTORY_RENDER_SCALE,undefined);
 }
 const off=await f.runtime.launchPlan({...f.game,gtaGraphics:{frameGeneration:'off'}});assert.equal(off.env.VICTORY_FRAMEGEN,'0');assert.equal(off.env.VICTORY_DLSS,'0');assert.equal(off.env.VICTORY_OWN_UPSCALER,'1');
});
test('unsupported hardware or missing NVIDIA files cannot advertise DLSS or silently enable generation',async t=>{
 const f=await fixture(t,{dlss:false,frameGeneration:false});assert.equal((await f.runtime.graphicsCapabilities()).dlss,false);await assert.rejects(f.runtime.launchPlan({...f.game,gtaGraphics:{upscaler:'dlss'}}),/not installed/);await f.enable();const caps=await f.runtime.graphicsCapabilities();assert.equal(caps.frameGeneration,false);assert.equal(caps.dlss,false);await assert.rejects(f.runtime.launchPlan({...f.game,gtaGraphics:{frameGeneration:'2x'}}),/not installed/);
});
test('XML preferences change independently and survive a launcher state round-trip',async t=>{
 const f=await fixture(t),prefs={motionBlur:'true',lodScale:'0.25',waterReflectionSSR:'true',sharpenEnabled:'false',frameLimit:'60'};
 const game={...f.game,gtaGraphics:prefs};const plan=await f.runtime.launchPlan(game);const file=path.join(plan.env.VICTORY_DATA_DIR,'7952987A/storage/settings/settings.xml');const xml=await fs.readFile(file,'utf8');for(const text of ['motionBlur value="true"','lodScale value="0.250000"','waterReflectionSSR value="true"','desiredFPS value="60"','frameRateLimit value="60"'])assert(xml.includes(text),text);
 const state=await f.runtime.libraryState(game);assert.equal(state.gtaGraphics.motionBlur,'true');await f.runtime.launchPlan({...game,...state});assert.equal(await fs.readFile(file,'utf8'),xml);
});
test('graphics IPC rejects unknown values and a launch race before writing',async()=>{
 const source=await fs.readFile(path.join(__dirname,'main.cjs'),'utf8'),handlers={},games=[{id:'gta-vi'}],sessions=new Set(),launchingGames=new Set();let writes=0,race=false;
 const context={require,handleIpc:(k,f)=>handlers[k]=f,sessions,launchingGames,edit:async f=>{if(race)launchingGames.add('gta-vi');await f(games);writes++},win:{webContents:{send(){}}}};
 vm.runInNewContext(source.split('\n').find(line=>line.includes("handleIpc('gta6-graphics'")),context);const save=handlers['gta6-graphics'];await save(null,{frameGeneration:'2x'});assert.equal(games[0].gtaGraphics.frameGeneration,'2x');
 for(const value of [{frameGeneration:'6x'},{hdr:'true'},{unknown:'1'},null,JSON.parse('{"__proto__":{"bad":true}}')])await assert.rejects(save(null,value));
 race=true;await assert.rejects(save(null,{motionBlur:'true'}),/Close GTA VI/);assert.equal(writes,1);assert.equal(games[0].gtaGraphics.motionBlur,'false');
});
