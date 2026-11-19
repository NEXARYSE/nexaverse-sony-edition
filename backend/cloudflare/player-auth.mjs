export async function playerIdentity(request,fetcher=(url,options)=>fetch(url,options)){
 const authorization=request.headers.get('authorization')||'';
 if(!/^Bearer [^\s]{1,2048}$/.test(authorization))return null;
 const response=await fetcher('https://discord.com/api/v10/users/@me',{
  headers:{authorization},redirect:'manual',signal:AbortSignal.timeout(8000)
 });
 if(!response.ok)return null;
 const user=await response.json();
 return /^\d{17,20}$/.test(user.id)?{id:user.id,displayName:String(user.global_name||user.username||'Player').slice(0,128)}:null;
}
