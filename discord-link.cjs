// One launcher action starts identity linking, then Discord's friends consent.
module.exports=function createDiscordLink({identity,social}){
 let pending=null;
 async function link(){
  const result=await identity.connect();
  if(!result.ok)return result;
  try{return {...result,friends:await social.connect()}}
  catch{return {...result,friends:{status:'error',friends:[],message:'Discord linked, but friends setup could not start. Retry in Friends.'}}}
 }
 return ()=>{if(pending)return pending;pending=link().finally(()=>{pending=null});return pending};
};
