const fs=require('node:fs/promises'),path=require('node:path');
const {checkHost}=require('./runtime-compatibility.cjs');
const {playerName}=require('./ps5-profile.cjs');
const GAME_ID='nba-2k23-next-gen';
async function validateDirectory(directory){
 if(typeof directory!=='string'||!path.isAbsolute(directory))throw Error('Choose your NBA 2K23 PS5 game folder.');
 const root=await fs.realpath(directory);
 if(!(await fs.stat(path.join(root,'eboot.bin'))).isFile())throw Error('The selected folder has no eboot.bin.');
 const metadata=path.join(root,'sce_sys','param.json');
 if((await fs.stat(metadata)).size>1048576)throw Error('Invalid game metadata.');
 const p=JSON.parse(await fs.readFile(metadata,'utf8'));
 const names=Object.values(p.localizedParameters||{}).filter(v=>v&&typeof v==='object').map(v=>v.titleName);
 if(!/^PPSA\d{5}$/.test(p.titleId||'')||!String(p.contentId||'').includes('NBA2K23')||!names.some(n=>/^NBA\s*2K23$/i.test(n||'')))throw Error('Choose NBA 2K23 NEXT GEN for PS5. Other games and PS4 builds are not accepted.');
 return root;
}
function create({runtimeRoot,userData,verifyDiscord,verifyHost=checkHost}){
 const executable=()=>path.join(runtimeRoot,'kyty_emulator.exe');
 async function launchPlan(game){
  if(game.id!==GAME_ID)throw Error('This integration only accepts NBA 2K23 NEXT GEN.');
  verifyHost();
  const user=await verifyDiscord();
  if(!user||!/^\d{17,20}$/.test(user.id))throw Error('Link Discord to play NBA 2K23 NEXT GEN.');
  const directory=await validateDirectory(game.gameDirectory),exe=executable();
  await fs.access(exe);
  const cwd=path.join(userData,'nba23-kyty',user.id);await fs.mkdir(cwd,{recursive:true});
  // 2K22's authenticated service bridge is title-specific and must not route this title.
  const env=Object.fromEntries(Object.entries(process.env).filter(([key])=>!key.startsWith('NEXAVERSE_NBA22_')&&key!=='NEXAVERSE_CAPTURE_PROTOCOL'));
  // Request a 1080p console output; window dimensions alone do not change the
  // title's internal render targets when the emulated display reports 4K.
  env.NEXAVERSE_VIDEO_OUT_1080P='1';
  env.NEXAVERSE_NBA23_MENU_ART=path.join(runtimeRoot,'nba23-menu-nexaverse.dds');
  // Normal gameplay has no per-draw or request profiling. Crash metadata is
  // emitted once only on failure and records code locations and roster indices.
  if(process.env.NEXAVERSE_NBA23_DIAGNOSTICS!=='1'){
   for(const key of ['NEXAVERSE_NBA23_GRAPHICS_TRACE','NEXAVERSE_NBA23_PROTOCOL_TRACE','NEXAVERSE_TEXTURE_LIFETIME_TRACE','NEXAVERSE_NBA23_LOADING_TRACE'])delete env[key];
  }
  env.NEXAVERSE_NBA23_CRASH_TRACE='1';
  env.NEXAVERSE_NBA23_SHADER_CACHE_ABI49='1';
  const optimize=process.env.NEXAVERSE_NBA23_OPTIMIZATIONS==='0'?'0':'1';
  env.KYTY_TILE_LAYOUT_CACHE=optimize;
  env.KYTY_IMAGE_SIZE_CACHE=optimize;
  env.KYTY_IMAGE_SPECIALIZATION_CACHE=optimize;
  env.NEXAVERSE_NBA23_PRIVATE_AUTH='1';
  env.NEXAVERSE_NBA23_MENU_COLOR=path.join(runtimeRoot,'nba23-menu-nexaverse.png');
  const metadata=JSON.parse(await fs.readFile(path.join(directory,'sce_sys','param.json'),'utf8'));
  const patchArgs=[];
  if(metadata.titleId==='PPSA07169' && (metadata.contentVersion||metadata.appVersion)==='01.016.000'){
   const patch=path.join(runtimeRoot,'nba23-1080p-dec1-PPSA07169-01.016.000.json');
   await fs.access(patch);patchArgs.push('--game-patch',patch);
   // Retain real guest visibility tests instead of forcing every queried
   // object visible. An explicit override keeps the old fallback available.
   if(process.env.NEXAVERSE_NBA23_OCCLUSION!=='0')patchArgs.push('--occlusion-queries');
  }
  return {discordUserId:user.id,exe,cwd,env,windowsHide:true,args:['--game',directory,...patchArgs,'--screen-width','1920','--screen-height','1080','--vblank-frequency','60','--present-mode','Mailbox','--frame-rate-limit','0','--no-splash','--shader-cache','--tessellation','--readback-storage-images','--amd-cpu','--playgo-hack','--redzone','--user-name',playerName(user),'--user-id','1000']};
 }
 return {executable,validateDirectory,launchPlan};
}
module.exports={GAME_ID,validateDirectory,create};
