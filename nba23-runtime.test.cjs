const {test}=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs/promises'),path=require('node:path'),os=require('node:os');
const {create,validateDirectory}=require('./nba23-runtime.cjs');
test('NBA 2K23 PS5 folder validation, emulator routing and save/backend isolation',async()=>{
 const temp=await fs.mkdtemp(path.join(os.tmpdir(),'nexaverse-nba23-'));
 try{
  const gameDirectory=path.join(temp,'NBA 2K23 PS5'),runtimeRoot=path.join(temp,'runtime');
  await fs.mkdir(path.join(gameDirectory,'sce_sys'),{recursive:true});await fs.mkdir(runtimeRoot);
  await fs.writeFile(path.join(gameDirectory,'eboot.bin'),'fixture');await fs.writeFile(path.join(runtimeRoot,'kyty_emulator.exe'),'fixture');
  const param={titleId:'PPSA99999',contentId:'UP0000-PPSA99999_00-NBA2K23000000000',localizedParameters:{'en-US':{titleName:'NBA 2K23'}}};
  const write=p=>fs.writeFile(path.join(gameDirectory,'sce_sys','param.json'),JSON.stringify(p));await write(param);
  const user={id:'123456789012345678',global_name:'Test Player'};
  const adapter=create({runtimeRoot,userData:temp,verifyHost:()=>{},verifyDiscord:async()=>user});
  const game={id:'nba-2k23-next-gen',gameDirectory};
  const plan=await adapter.launchPlan(game);
  assert.equal(plan.env.NEXAVERSE_VIDEO_OUT_1080P,'1');assert.equal(plan.env.NEXAVERSE_NBA23_PRIVATE_AUTH,'1');
  assert(!plan.args.includes('--game-patch'));
  const patchPath=path.join(runtimeRoot,'nba23-1080p-dec1-PPSA07169-01.016.000.json');
  await fs.writeFile(patchPath,'{}');
  await write({...param,titleId:'PPSA07169',contentVersion:'01.016.000'});
  const supportedPlan=await adapter.launchPlan(game);
  assert.equal(supportedPlan.args[supportedPlan.args.indexOf('--game-patch')+1],patchPath);
  assert(supportedPlan.args.includes('--occlusion-queries'));
  const previousOcclusion=process.env.NEXAVERSE_NBA23_OCCLUSION;
  try{
   process.env.NEXAVERSE_NBA23_OCCLUSION='0';
   assert(!(await adapter.launchPlan(game)).args.includes('--occlusion-queries'));
  }finally{
   if(previousOcclusion===undefined)delete process.env.NEXAVERSE_NBA23_OCCLUSION;
   else process.env.NEXAVERSE_NBA23_OCCLUSION=previousOcclusion;
  }
  await write({...param,titleId:'PPSA07169',contentVersion:'01.017.000'});
  assert(!(await adapter.launchPlan(game)).args.includes('--game-patch'));
  assert(!(await adapter.launchPlan(game)).args.includes('--occlusion-queries'));
  await write(param);
  assert.equal(plan.exe,path.join(runtimeRoot,'kyty_emulator.exe'));
  assert.equal(plan.args[plan.args.indexOf('--game')+1],await fs.realpath(gameDirectory));
  assert.equal(plan.args[plan.args.indexOf('--user-name')+1],'Test Player');
  assert.equal(plan.args[plan.args.indexOf('--frame-rate-limit')+1],'0');
  assert.equal(plan.args[plan.args.indexOf('--screen-width')+1],'1920');
  assert.equal(plan.args[plan.args.indexOf('--screen-height')+1],'1080');
  for(const key of ['KYTY_TILE_LAYOUT_CACHE','KYTY_IMAGE_SIZE_CACHE','KYTY_IMAGE_SPECIALIZATION_CACHE'])assert.equal(plan.env[key],'1');
  for(const key of ['NEXAVERSE_NBA23_PROTOCOL_TRACE','NEXAVERSE_NBA23_GRAPHICS_TRACE','NEXAVERSE_TEXTURE_LIFETIME_TRACE','NEXAVERSE_NBA23_LOADING_TRACE'])assert.equal(plan.env[key],undefined);
  assert.equal(plan.env.NEXAVERSE_NBA23_CRASH_TRACE,'1');
  assert.equal(plan.env.NEXAVERSE_NBA23_SHADER_CACHE_ABI49,'1');
  assert(!plan.args.includes('--relaxed-shader-float'));assert(plan.args.includes('--shader-cache'));assert(plan.args.includes('--tessellation'));assert.equal(plan.args[plan.args.indexOf('--present-mode')+1],'Mailbox');assert(plan.windowsHide);
  assert.equal(plan.cwd,path.join(temp,'nba23-kyty',user.id));
  assert(!Object.keys(plan.env).some(k=>k.startsWith('NEXAVERSE_NBA22_')));
  assert(!plan.args.includes('--network-connected'));assert(!plan.args.includes('--psn'));
  const other=await create({runtimeRoot,userData:temp,verifyHost:()=>{},verifyDiscord:async()=>({...user,id:'987654321098765432'})}).launchPlan(game);
  assert.notEqual(other.cwd,plan.cwd);
  await assert.rejects(adapter.launchPlan({...game,id:'nba-2k22-next-gen'}),/only accepts/);
  await assert.rejects(create({runtimeRoot,userData:temp,verifyHost:()=>{},verifyDiscord:async()=>null}).launchPlan(game),/Link Discord/);
  await write({...param,titleId:'CUSA99999'});await assert.rejects(validateDirectory(gameDirectory),/PS4/);
  await write({...param,contentId:'UP0000-PPSA99999_00-NBA2K22000000000'});await assert.rejects(validateDirectory(gameDirectory),/not accepted/);
  await write({...param,localizedParameters:{'en-US':{titleName:'NBA 2K22'}}});await assert.rejects(validateDirectory(gameDirectory),/not accepted/);
  await write(param);await fs.rename(path.join(gameDirectory,'eboot.bin'),path.join(gameDirectory,'missing.bin'));await assert.rejects(validateDirectory(gameDirectory));
 }finally{
  assert(path.resolve(temp).startsWith(path.resolve(os.tmpdir())+path.sep+'nexaverse-nba23-'));
  await fs.rm(temp,{recursive:true,force:true});
 }
});
