const DEFAULT_HOME_ORDER=Object.freeze(['gta-vi','cb-launcher','last-of-us-2','god-of-war-ragnarok','spider-man-2','infamous-second-son','mario-kart-world','007-first-light','black-flag-resynced','ufc-6']);
const rank=new Map(DEFAULT_HOME_ORDER.map((id,index)=>[id,index]));
const playedAt=value=>Number.isFinite(value)&&value>0?value:0;
function recentFirst(items){return [...items].sort((a,b)=>playedAt(b.lastPlayed)-playedAt(a.lastPlayed)||(rank.get(a.id)??Number.MAX_SAFE_INTEGER)-(rank.get(b.id)??Number.MAX_SAFE_INTEGER));}
function create(){return {order:async items=>recentFirst(items)}}
module.exports={DEFAULT_HOME_ORDER,recentFirst,create};
