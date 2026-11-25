'use strict';
const fs = require('node:fs/promises');
const {createHash} = require('node:crypto');
const path = require('node:path');
const GAME_ID = 'gta-vi';
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
      return {exe:executable(),gtaLinked:true,gtaXboxExecutable:file,gtaLinkError:'',gtaBackend:'victory',gtaSoftwareRenderer:false};
    } catch(error) {
      return {exe:undefined,gtaLinked:false,gtaLinkError:game.gtaXboxExecutable?error.message:''};
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
module.exports={GAME_ID,REFERENCE,create};
