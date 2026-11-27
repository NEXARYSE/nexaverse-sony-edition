'use strict';
const fs = require('node:fs/promises');
const {createHash} = require('node:crypto');
const path = require('node:path');
const GAME_ID = 'gta-vi';
const graphics=require('./gta6-graphics-options.js');
const RENDER_RESOLUTIONS=graphics.resolutions;
const renderResolution=graphics.resolution;
// Fingerprint of the user's supported local Xbox image. No game bytes are bundled.
const REFERENCE = Object.freeze({name:'game_scarlett_master.exe', size:120181760,
  sha256:'e95020b2fa4a721db3df59fdecf75621730ac559d8835252f6c3b658d5a08993'});
function create({runtimeRoot, appData, reference = REFERENCE, probeGraphics}) {
  let hardwareProbe;
  const checked = new Map();
  const executable = () => path.join(runtimeRoot, 'NEXA GTA.exe');
  const required = ['NEXA GTA.exe','NEXA GTA Core.exe','NEXA GTA.ico','dxil/dxil.dll','dxil/dxcompiler.dll'];
  async function runtimeReady() { for (const name of required) await fs.access(path.join(runtimeRoot,name)); }
  async function validateExecutable(file, {force = false} = {}) {
    if (typeof file !== 'string' || !path.isAbsolute(file) || path.basename(file).toLowerCase() !== reference.name)
      throw Error('Select the supported GTA VI Xbox game_scarlett_master.exe.');
    const real = await fs.realpath(file);
    if (path.basename(real).toLowerCase() !== reference.name) throw Error('Select the GTA VI Xbox executable itself.');
    const stat = await fs.stat(real);
    if (!stat.isFile() || stat.size !== reference.size) throw Error('This is not the supported GTA VI Xbox executable.');
    const key = [real, stat.size, stat.mtimeMs, stat.ctimeMs, stat.ino].join('|');
    if (!force && checked.has(key)) return checked.get(key);
    const handle = await fs.open(real, 'r');
    try {
      const header = Buffer.alloc(64); await handle.read(header,0,64,0);
      if (header.toString('ascii',0,2) !== 'MZ') throw Error('Invalid Xbox executable.');
      const at = header.readUInt32LE(60);
      if (at > stat.size-26) throw Error('Invalid Xbox executable header.');
      const pe = Buffer.alloc(26); await handle.read(pe,0,26,at);
      if (pe.readUInt32LE(0)!==0x4550 || pe.readUInt16LE(4)!==0x8664 || pe.readUInt16LE(24)!==0x20b)
        throw Error('Select a supported x64 Xbox executable.');
      const hash = createHash('sha256');
      for await (const block of handle.createReadStream({start:0,autoClose:false})) hash.update(block);
      if (hash.digest('hex') !== reference.sha256) throw Error('Unsupported GTA VI Xbox build. Select an unmodified supported executable.');
      const after = await handle.stat();
      if (after.size!==stat.size || after.mtimeMs!==stat.mtimeMs || after.ctimeMs!==stat.ctimeMs)
        throw Error('The executable changed during validation. Please link it again.');
    } finally { await handle.close(); }
    checked.clear(); checked.set(key,real);
    return real;
  }
  async function graphicsCapabilities(){
    let manifest={};try{manifest=JSON.parse(await fs.readFile(path.join(runtimeRoot,'graphics-capabilities.json'),'utf8'));}catch{}
    const directory=path.join(runtimeRoot,'streamline');
    let installed=true;for(const name of ['sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll'])try{await fs.access(path.join(directory,name))}catch{installed=false}
    let fg=installed;for(const name of ['sl.dlss_g.dll','sl.reflex.dll','sl.pcl.dll','nvngx_dlssg.dll'])try{await fs.access(path.join(directory,name))}catch{fg=false}
    let hardware={dlss:false,frameGeneration:false};
    if(manifest.streamline===true&&installed){
      hardwareProbe ||= (probeGraphics?probeGraphics(directory):new Promise(resolve=>{
        const env={...process.env};for(const key of Object.keys(env))if(key.startsWith('VICTORY_'))delete env[key];
        Object.assign(env,{VICTORY_STREAMLINE_DIR:directory,VICTORY_LOG:'warn'});
        require('node:child_process').execFile(path.join(runtimeRoot,'NEXA GTA Core.exe'),['upscaler-check'],{cwd:runtimeRoot,env,windowsHide:true,timeout:20000,maxBuffer:2*1024*1024},(error,stdout,stderr)=>{
          const text=String(stdout)+' '+String(stderr);resolve({dlss:!error&&/DLSS supported/.test(text),frameGeneration:!error&&/frame generation supported/.test(text)});
        });
      })).catch(()=>({dlss:false,frameGeneration:false}));
      hardware=await hardwareProbe;
    }
    return {dlss:manifest.streamline===true&&installed&&hardware.dlss,frameGeneration:manifest.frameGeneration===true&&fg&&hardware.frameGeneration,vSync:manifest.vSync===true,hdr:false,directory};
  }
  async function libraryState(game) {
    try {
      const file=await validateExecutable(game.gtaXboxExecutable);
      await runtimeReady();
      return {exe:executable(),gtaLinked:true,gtaXboxExecutable:file,gtaLinkError:'',gtaBackend:'victory',gtaSoftwareRenderer:false,gtaRenderResolution:renderResolution(game.gtaRenderResolution),gtaGraphics:graphics.normalize(game.gtaGraphics),gtaGraphicsCapabilities:await graphicsCapabilities()};
    } catch(error) {
      return {exe:undefined,gtaLinked:false,gtaLinkError:game.gtaXboxExecutable?error.message:'',gtaRenderResolution:renderResolution(game.gtaRenderResolution),gtaGraphics:graphics.normalize(game.gtaGraphics),gtaGraphicsCapabilities:await graphicsCapabilities()};
    }
  }
  async function prepareStorage(env,settings) {
    // Optional local storage preference survives updates and is never bundled.
    try {
      const location=JSON.parse(await fs.readFile(path.join(appData,'NEXA GTA','7900-storage.json'),'utf8'));
      if (!path.isAbsolute(location.cacheDir)||!path.isAbsolute(location.dataDir)) throw Error('Invalid GTA storage paths.');
      env.VICTORY_CACHE_DIR=location.cacheDir;env.VICTORY_DATA_DIR=location.dataDir;
    } catch(error) { if(error.code!=='ENOENT') throw error; }
    await fs.mkdir(env.VICTORY_CACHE_DIR,{recursive:true});
    const file=path.join(env.VICTORY_DATA_DIR,'7952987A','storage','settings','settings.xml');
    await fs.mkdir(path.dirname(file),{recursive:true});
    let defaults=await fs.readFile(path.join(__dirname,'gta6-graphics-defaults.xml'),'utf8');
    const values=graphics.xmlValues(settings);
    for(const [name,value] of Object.entries(values)) defaults=defaults.replace(new RegExp('(<'+name+'\\s+value=")[^"]*("\\s*/>)','g'),(_,a,b)=>a+value+b);
    const [defaultWidth,defaultHeight]=env.VICTORY_DISPLAY_MODE.split('x');
    defaults=defaults.replace(/(<resolutionFullscreen\b[^>]*>)([\s\S]*?)(<\/resolutionFullscreen>)/g,(_,open,body,close)=>open+body.replace(/(<width\s+value=")[^"]*("\s*\/>)/g,(_,a,b)=>a+defaultWidth+b).replace(/(<height\s+value=")[^"]*("\s*\/>)/g,(_,a,b)=>a+defaultHeight+b)+close);
    try {
      let current=await fs.readFile(file,'utf8');const original=current;
      for(const name of Object.keys(values)) {
        const expression=new RegExp('(<'+name+'\\s+value=")[^"]*("\\s*/>)','g');
        const value=defaults.match(new RegExp('<'+name+'\\s+value="([^"]*)"'))?.[1];
        if(value!==undefined)current=current.replace(expression,(_,before,after)=>before+value+after);
      }
      // Stored output dimensions must agree with the console display mode.
      // Internal rendering is controlled separately by VICTORY_RENDER_SCALE.
      const [width,height]=env.VICTORY_DISPLAY_MODE.split('x');
      current=current.replace(/(<resolutionFullscreen\b[^>]*>)([\s\S]*?)(<\/resolutionFullscreen>)/g,(_,open,body,close)=>
        open+body.replace(/(<width\s+value=")[^"]*("\s*\/>)/g,(_,a,b)=>a+width+b)
          .replace(/(<height\s+value=")[^"]*("\s*\/>)/g,(_,a,b)=>a+height+b)+close);
      if(current!==original) {
        try {await fs.writeFile(file+'.before-0.1.89',original,{flag:'wx'});}catch(error){if(error.code!=='EEXIST')throw error;}
        const temporary=file+'.0.1.89.tmp';await fs.writeFile(temporary,current);await fs.rename(temporary,file);
      }
    } catch(error) {
      if(error.code!=='ENOENT')throw error;
      await fs.writeFile(file,defaults,{flag:'wx'});
    }
  }
  async function launchPlan(game) {
    if (game?.id !== GAME_ID) throw Error('NEXA GTA is restricted to GTA VI.');
    const file=await validateExecutable(game.gtaXboxExecutable,{force:true});
    await runtimeReady();
    const env={...process.env};
    // Use a repeatable hardware profile rather than inherited shader experiments.
    for (const key of Object.keys(env)) if (/^(VICTORY_|NEXA_|XCL_|LEONIDA_|XV_)/.test(key)) delete env[key];
    env.NEXA_GTA_PRESENTATION='1'; env.NEXA_GTA_BACKEND='victory';
    env.NEXA_LAUNCHER_VERSION=require('./package.json').version;
    env.VICTORY_GAME_DIR=path.dirname(file);
    env.VICTORY_CACHE_DIR=path.join(appData,'NEXA GTA','victory','cache');
    env.VICTORY_DATA_DIR=path.join(appData,'NEXA GTA','victory','data');
    env.VICTORY_NEXA='1';
    Object.assign(env, require('./gta6-profile.json'));
    // Keep 1080p output; the 720p profile uses the title's temporal upscaler.
    const settings=graphics.normalize(game.gtaGraphics);
    const rendering=RENDER_RESOLUTIONS[renderResolution(game.gtaRenderResolution)];
    env.VICTORY_DISPLAY_MODE=rendering.output;
    env.VICTORY_OUTPUT=env.VICTORY_DISPLAY_MODE;
    env.VICTORY_OWN_UPSCALER='1';
    env.VICTORY_RENDER_SCALE=rendering.scale;
    env.VICTORY_VSYNC=settings.vSync;
    const wantsDLSS=settings.upscaler==='dlss'||settings.frameGeneration==='2x';
    env.VICTORY_DLSS='0';env.VICTORY_FRAMEGEN='0';
    if(wantsDLSS){
      const capabilities=await graphicsCapabilities();
      if(!capabilities.dlss)throw Error('NVIDIA DLSS is not installed for this runtime. Keep Game temporal selected until setup is available.');
      env.VICTORY_STREAMLINE_DIR=capabilities.directory;
      env.VICTORY_DLSS='1';env.VICTORY_STREAMLINE_PRESENT='1';env.VICTORY_DLSS_MODE=rendering.native?'dlaa':settings.dlssMode;
      if(settings.frameGeneration==='2x'){if(!capabilities.frameGeneration)throw Error('Update the GTA VI runtime to use frame generation.');env.VICTORY_FRAMEGEN='1';env.VICTORY_FRAMEGEN_X='2';env.VICTORY_TEXTURE_ROOM_PERCENT='80';}
      delete env.VICTORY_OWN_UPSCALER;delete env.VICTORY_RENDER_SCALE;
    }
    await prepareStorage(env,settings);
    env.VICTORY_MEMORY_MAP='0';
    env.NEXA_GTA_ICON=path.join(runtimeRoot,'NEXA GTA.ico');
    env.VICTORY_DXIL=path.join(runtimeRoot,'dxil','dxil.dll');
    // Physical keyboard/mouse bindings remain in the VIctory runtime. These
    // are test automation and obsolete renderer settings, never normal input.
    for(const key of Object.keys(env)) if(key.startsWith('XCL_')||key.startsWith('LEONIDA_'))delete env[key];
    for(const key of ['NEXA_GRAPHICS','VICTORY_D3D12_WARP','VICTORY_KEYS','VICTORY_CAPTURE','VICTORY_CAPTURE_FRAMES','VICTORY_D3D12_GBV','NEXA_PIPELINE_TRACE','NEXA_SHADER_CAPTURE','NEXA_NATIVE_GRAPHICS_CAPTURE','XV_INPUT_PRESS'])delete env[key];
    return {exe:executable(),args:[file],cwd:runtimeRoot,env,windowsHide:false};
  }
  async function saveFolder(){
    let dataDir=path.join(appData,'NEXA GTA','victory','data');
    try{
      const location=JSON.parse(await fs.readFile(path.join(appData,'NEXA GTA','7900-storage.json'),'utf8'));
      if(!path.isAbsolute(location.dataDir))throw Error('Invalid GTA storage paths.');
      dataDir=location.dataDir;
    }catch(error){if(error.code!=='ENOENT')throw error;}
    return path.join(dataDir,'7952987A','saves');
  }
  return {executable,validateExecutable,libraryState,launchPlan,graphicsCapabilities,saveFolder};
}
module.exports={GAME_ID,REFERENCE,RENDER_RESOLUTIONS,renderResolution,create};
