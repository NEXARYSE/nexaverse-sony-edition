const fs=require('node:fs/promises'),path=require('node:path');
const DEFAULT_HOME_ORDER=Object.freeze(['gta-vi','cb-launcher','last-of-us-2','god-of-war-ragnarok','spider-man-2','infamous-second-son','mario-kart-world','007-first-light','black-flag-resynced','ufc-6']);
const rank=new Map(DEFAULT_HOME_ORDER.map((id,index)=>[id,index]));
const playedAt=value=>Number.isFinite(value)&&value>0?value:0;
function recentFirst(items,baseline={}){
 const promoted=g=>playedAt(g.lastPlayed)>playedAt(baseline[g.id])?playedAt(g.lastPlayed):0;
 return [...items].sort((a,b)=>promoted(b)-promoted(a)||(rank.get(a.id)??Number.MAX_SAFE_INTEGER)-(rank.get(b.id)??Number.MAX_SAFE_INTEGER));
}
function create({statePath}){
 let statePromise;
 function baseline(items){
  if(!statePromise)statePromise=(async()=>{
   try{const saved=JSON.parse(await fs.readFile(statePath,'utf8'));if(saved.version===1&&saved.baselineLastPlayed&&typeof saved.baselineLastPlayed==='object'&&!Array.isArray(saved.baselineLastPlayed))return saved.baselineLastPlayed;}
   catch(error){if(error.code!=='ENOENT'&&!(error instanceof SyntaxError))throw error;}
   const baselineLastPlayed=Object.fromEntries(items.map(g=>[g.id,playedAt(g.lastPlayed)]));
   await fs.mkdir(path.dirname(statePath),{recursive:true});await fs.writeFile(statePath+'.tmp',JSON.stringify({version:1,baselineLastPlayed}));await fs.rename(statePath+'.tmp',statePath);return baselineLastPlayed;
  })().catch(error=>{statePromise=null;throw error;});return statePromise;
 }
 return {order:async items=>recentFirst(items,await baseline(items))};
}
module.exports={DEFAULT_HOME_ORDER,recentFirst,create};
