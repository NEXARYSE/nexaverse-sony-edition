'use strict';
const fs = require('node:fs/promises');
const {createHash} = require('node:crypto');
const path = require('node:path');
const GAME_ID = 'gta-vi';
const RENDER_RESOLUTIONS = Object.freeze({'720p':Object.freeze({output:'1920x1080',scale:'0.6666666666666666'}),'1080p':Object.freeze({output:'1920x1080',scale:'1.0'})});
function renderResolution(value) { return typeof value==='string' && Object.hasOwn(RENDER_RESOLUTIONS,value) ? value : '720p'; }
// Fingerprint of the user's supported local Xbox image. No game bytes are bundled.
const REFERENCE = Object.freeze({name:'game_scarlett_master.exe', size:120181760,
  sha256:'e95020b2fa4a721db3df59fdecf75621730ac559d8835252f6c3b658d5a08993'});
function create({runtimeRoot, appData, reference = REFERENCE}) {
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
  async function libraryState(game) {
    try {
      const file=await validateExecutable(game.gtaXboxExecutable);
      await runtimeReady();
      return {exe:executable(),gtaLinked:true,gtaXboxExecutable:file,gtaLinkError:'',gtaBackend:'victory',gtaSoftwareRenderer:false,gtaRenderResolution:renderResolution(game.gtaRenderResolution)};
    } catch(error) {
      return {exe:undefined,gtaLinked:false,gtaLinkError:game.gtaXboxExecutable?error.message:'',gtaRenderResolution:renderResolution(game.gtaRenderResolution)};
    }
  }
  async function prepareStorage(env) {
    // Optional local storage preference survives updates and is never bundled.
    try {
      const location=JSON.parse(await fs.readFile(path.join(appData,'NEXA GTA','7900-storage.json'),'utf8'));
      if (!path.isAbsolute(location.cacheDir)||!path.isAbsolute(location.dataDir)) throw Error('Invalid GTA storage paths.');
      env.VICTORY_CACHE_DIR=location.cacheDir;env.VICTORY_DATA_DIR=location.dataDir;
    } catch(error) { if(error.code!=='ENOENT') throw error; }
    await fs.mkdir(env.VICTORY_CACHE_DIR,{recursive:true});
    const file=path.join(env.VICTORY_DATA_DIR,'7952987A','storage','settings','settings.xml');
    await fs.mkdir(path.dirname(file),{recursive:true});
    const defaults=await fs.readFile(path.join(__dirname,'gta6-graphics-defaults.xml'),'utf8');
    try {
      let current=await fs.readFile(file,'utf8');const original=current;
      for(const name of ['hdr','desiredFPS','lodScale','motionBlur','rtDynamicShadows','shadowParticleShadows','waterReflectionSSR','frameRateLimit','vSync']) {
        const expression=new RegExp('(<'+name+'\\s+value=")[^"]*("\\s*/>)','g');
        const value=defaults.match(new RegExp('<'+name+'\\s+value="([^"]*)"'))?.[1];
        if(value!==undefined)current=current.replace(expression,(_,before,after)=>before+value+after);
      }
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
    env.VICTORY_GAME_DIR=path.dirname(file);
    env.VICTORY_CACHE_DIR=path.join(appData,'NEXA GTA','victory','cache');
    env.VICTORY_DATA_DIR=path.join(appData,'NEXA GTA','victory','data');
    env.VICTORY_NEXA='1';
    Object.assign(env, require('./gta6-profile.json'));
    await prepareStorage(env);
    // Keep 1080p output; the 720p profile uses the title's temporal upscaler.
    const rendering=RENDER_RESOLUTIONS[renderResolution(game.gtaRenderResolution)];
    env.VICTORY_DISPLAY_MODE=rendering.output;
    env.VICTORY_OUTPUT=env.VICTORY_DISPLAY_MODE;
    env.VICTORY_OWN_UPSCALER='1';
    env.VICTORY_RENDER_SCALE=rendering.scale;
    env.VICTORY_MEMORY_MAP='0';
    env.NEXA_GTA_ICON=path.join(runtimeRoot,'NEXA GTA.ico');
    env.VICTORY_DXIL=path.join(runtimeRoot,'dxil','dxil.dll');
    // Physical keyboard/mouse bindings remain in the VIctory runtime. These
    // are test automation and obsolete renderer settings, never normal input.
    for(const key of Object.keys(env)) if(key.startsWith('XCL_')||key.startsWith('LEONIDA_'))delete env[key];
    for(const key of ['NEXA_GRAPHICS','VICTORY_D3D12_WARP','VICTORY_KEYS','VICTORY_CAPTURE','VICTORY_CAPTURE_FRAMES','VICTORY_D3D12_GBV','NEXA_PIPELINE_TRACE','NEXA_SHADER_CAPTURE','NEXA_NATIVE_GRAPHICS_CAPTURE','XV_INPUT_PRESS'])delete env[key];
    return {exe:executable(),args:[file],cwd:runtimeRoot,env,windowsHide:false};
  }
  return {executable,validateExecutable,libraryState,launchPlan};
}
module.exports={GAME_ID,REFERENCE,RENDER_RESOLUTIONS,renderResolution,create};
