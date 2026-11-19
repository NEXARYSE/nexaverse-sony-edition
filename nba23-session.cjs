const {createRelay}=require('./nba23-cloud.cjs');
const fs=require('node:fs'),path=require('node:path');
function create({cloudAccessToken,relayFactory=createRelay,diagnostics=false}={}){
 const active=new Set();
 async function prepare(plan){
  if(typeof cloudAccessToken!=='function')throw Error('The NBA 2K23 connection service needs your linked Discord account.');
  let timingRecords=0;
  const onTiming=plan.env.NEXAVERSE_NBA23_LOADING_TRACE==='1'?record=>{
   if(timingRecords++>=256)return;
   // Routes, HTTP status and elapsed time only; never queries, identities or packets.
   fs.appendFileSync(path.join(plan.cwd,'nba23-loading-timings.jsonl'),JSON.stringify(record)+'\n');
  }:undefined;
  if(onTiming)fs.writeFileSync(path.join(plan.cwd,'nba23-loading-timings.jsonl'),'');
  const token=await cloudAccessToken(plan.discordUserId),backend=relayFactory({token,onTiming});
  let address;
  try{address=await backend.listen(0)}catch(error){await backend.close().catch(()=>{});throw Error('Could not start the NBA 2K23 connection service: '+error.message)}
  let closing;
  const release=()=>closing||(closing=backend.close().finally(()=>active.delete(release)));
  active.add(release);
  const env={...plan.env,NEXAVERSE_NBA23_BACKEND_PORT:String(address.port),NEXAVERSE_NBA23_LOCAL_PROFILE:'1'};
  if(diagnostics){env.NEXAVERSE_NBA23_PROTOCOL_TRACE='1';env.NEXAVERSE_NBA23_GRAPHICS_TRACE='1';}
  return {...plan,env,args:[...plan.args,'--network-connected','--emulate-psn','--printf-direction','Silent'],release};
 }
 function spawn(plan,spawnProcess){
  try{const child=spawnProcess(plan.exe,plan.args,{cwd:plan.cwd,env:plan.env,stdio:'ignore',detached:true,windowsHide:Boolean(plan.windowsHide)});const release=()=>plan.release().catch(()=>{});child.once('error',release);child.once('exit',release);return child;}
  catch(error){plan.release().catch(()=>{});throw error;}
 }
 async function close(){await Promise.allSettled([...active].map(release=>release()))}
 return {prepare,spawn,close};
}
module.exports={create};
