module.exports=function createMutualFriends({identity,social,community,now=Date.now}){
 let pending=null,lastId=null,lastAt=0,stopping=false;
 async function sync(){
  if(stopping)return {status:'unavailable'};
  if(pending)return pending;
  pending=(async()=>{const account=await identity.status();if(!account.linked)return {status:'not-connected'};const state=social.status();if(state.status!=='ready'||!state.friendsLoaded||state.userId!==account.user.id)return {status:'not-connected'};if(lastId!==account.user.id||now()-lastAt>60000){await community.syncFriends([...new Set(state.friends.map(f=>f.id))]);lastId=account.user.id;lastAt=now()}return {status:'ready'}})().finally(()=>{pending=null});return pending;
 }
 async function get(id){const state=await sync();if(state.status!=='ready')return {...state,friends:[]};const result=await community.mutualFriends(id);if(result.status!=='ready')return {status:'unavailable',friends:[]};const ids=new Set(result.ids);return {status:'ready',friends:social.status().friends.filter(f=>ids.has(f.id)&&f.id!==id)}}
 async function remove(){stopping=true;try{if(pending)await pending.catch(()=>{});const account=await identity.status();if(account.linked)await community.forgetFriends();lastId=null;lastAt=0}finally{stopping=false}}
 return {sync,get,remove};
};
