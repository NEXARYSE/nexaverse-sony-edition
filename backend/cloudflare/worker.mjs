import {DurableObject} from 'cloudflare:workers';
import {timingSafeEqual} from 'node:crypto';
import {Buffer} from 'node:buffer';
import protocol from '../protocol.cjs';
import sessions from '../session-prototype.cjs';
import {playerIdentity} from './player-auth.mjs';
import {CareerStore,StoreError} from './career-store.mjs';
import {splitContentFrame,profileContent} from './profile-content.mjs';
import {NativeCheckpointStore,CHECKPOINT_ROUTE} from './native-checkpoint.mjs';
import {NativeCareerSaves,CAREER_UPLOAD_ROUTE,CAREER_ENUMERATE_ROUTE} from './native-career-saves.mjs';
import {NativeCareerCreation,CAREER_CREATE_ROUTE,CAREER_ATTRIBUTES_ROUTE} from './native-career-creation.mjs';
import {cityElection,CITY_ELECTION_ROUTE} from './native-city-election.mjs';
import {cityConfig,CITY_CONFIG_ROUTE} from './native-city-config.mjs';
import {NativeOnlineContent} from './native-online-content.mjs';
const repeatedRoute=route=>route==='/Session/Names/validate'||route===CAREER_CREATE_ROUTE||route===CAREER_ENUMERATE_ROUTE||route===CAREER_ATTRIBUTES_ROUTE;
const framedRoute=route=>route.startsWith('/Session/OnlineContent')||route===CAREER_UPLOAD_ROUTE;

const LIMIT=4*1024*1024;
function json(status,value){return Response.json(value,{status,headers:{'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}})}
function authorized(request,env){
 const expected=env.NEXAVERSE_RESEARCH_TOKEN;
 if(typeof expected!=='string'||!/^[a-f0-9]{64}$/.test(expected))return false;
 const given=request.headers.get('authorization')||'';
 const a=Buffer.from(given),b=Buffer.from('Bearer '+expected);
 return a.length===b.length&&timingSafeEqual(a,b);
}
async function readBounded(request){
 const declared=request.headers.get('content-length');
 if(declared&&(!/^\d+$/.test(declared)||Number(declared)>LIMIT))throw Error('too_large');
 if(!request.body)return new Uint8Array();
 const reader=request.body.getReader(),chunks=[];let size=0;
 try{while(true){const part=await reader.read();if(part.done)break;size+=part.value.byteLength;if(size>LIMIT){await reader.cancel();throw Error('too_large')}chunks.push(part.value)}}finally{reader.releaseLock()}
 const body=new Uint8Array(size);let offset=0;for(const chunk of chunks){body.set(chunk,offset);offset+=chunk.length}return body;
}

export class ProtocolJournal extends DurableObject {
 constructor(ctx,env){
  super(ctx,env);this.sql=ctx.storage.sql;
  this.sql.exec('CREATE TABLE IF NOT EXISTS protocol_events (id INTEGER PRIMARY KEY, at TEXT NOT NULL, route TEXT NOT NULL, bytes INTEGER NOT NULL, summary TEXT NOT NULL)');
 }
 async record(event){
  // Store schema summaries only: no body, account ID, username, session token,
  // authorization header, query string, or payload fingerprint is persisted.
  const safe={format:event.protocol?.format||'unrecognized',fields:(event.protocol?.fields||[]).slice(0,128).map(f=>({name:String(f.name).slice(0,64),type:String(f.type).slice(0,24),...(Number.isInteger(f.bytes)?{bytes:f.bytes}:{})}))};
  this.sql.exec('INSERT INTO protocol_events (at,route,bytes,summary) VALUES (?,?,?,?)',new Date().toISOString(),String(event.route).slice(0,160),Number(event.bytes),JSON.stringify(safe));
  this.sql.exec('DELETE FROM protocol_events WHERE id NOT IN (SELECT id FROM protocol_events ORDER BY id DESC LIMIT 256)');
 }
 async status(){
  const count=this.sql.exec('SELECT COUNT(*) AS count FROM protocol_events').one().count;
  const rows=this.sql.exec('SELECT at,route,bytes,summary FROM protocol_events ORDER BY id DESC LIMIT 16').toArray();
  const recent=rows.map(row=>({...row,summary:JSON.parse(row.summary)}));
  return {retainedEvents:count,last:recent[0]||null,recent};
 }
}

// Separate storage per verified Discord account. OAuth tokens and raw protocol
// packets are never persisted. Sessions, profile data and saves survive eviction.
export class PlayerSession extends DurableObject {
 constructor(ctx,env){
  super(ctx,env);this.ctx=ctx;this.events=[];this.window=0;this.requests=0;this.careers=new CareerStore(ctx.storage);this.nativeContent=new NativeOnlineContent(ctx.storage);this.checkpointsEnabled=env.NEXAVERSE_NATIVE_CHECKPOINTS==='1';this.checkpoints=this.checkpointsEnabled?new NativeCheckpointStore(ctx.storage):null;this.nativeCareers=this.checkpointsEnabled?new NativeCareerSaves(ctx.storage):null;this.nativeCreation=this.checkpointsEnabled?new NativeCareerCreation(ctx.storage):null;
  this.ready=ctx.storage.get('sessions').then(saved=>{
   this.sessions=new Map(saved||[]);
   this.adapter=sessions.createSessionPrototype({sessions:this.sessions,profileServices:true,checkpointServices:this.checkpointsEnabled});
  });
 }
 allowRequest(){
  const window=Math.floor(Date.now()/60000);
  if(window!==this.window){this.window=window;this.requests=0}
  return ++this.requests<=120;
 }
 async careerApi(method,path,body,baseRevision,displayName){
  await this.ready;
  if(!this.allowRequest())return {status:429,error:'request_limit'};
  try{
   this.careers.profile(displayName);
   const parse=()=>{try{return JSON.parse(new TextDecoder().decode(body))}catch{throw new StoreError(400,'invalid_json')}};
   if(path==='/profile'&&method==='GET')return {status:200,data:this.careers.profile(displayName)};
   if(path==='/profile/data'&&method==='PUT')return {status:200,data:await this.careers.saveProfile(baseRevision,body)};
   if(path==='/profile/data'&&method==='GET')return {status:200,binary:await this.careers.loadProfile()};
   if(path==='/careers'&&method==='GET')return {status:200,data:{careers:this.careers.list(),gameFormatVerified:false}};
   if(path==='/careers'&&method==='POST')return {status:201,data:this.careers.create(parse())};
   const match=/^\/careers\/([a-f0-9-]{36})\/(save|progress|history|restore)$/.exec(path);
   if(!match)return {status:404,error:'unknown_route'};
   const [,id,action]=match;
   if(action==='progress'&&method==='PATCH')return {status:200,data:this.careers.updateProgress(id,parse())};
   if(action==='save'&&method==='PUT')return {status:200,data:await this.careers.save(id,baseRevision,body)};
   if(action==='save'&&method==='GET')return {status:200,binary:await this.careers.load(id)};
   if(action==='history'&&method==='GET')return {status:200,data:{saves:this.careers.history(id)}};
   if(action==='restore'&&method==='POST')return {status:200,data:await this.careers.restore(id,parse())};
   return {status:405,error:'unsupported_method'};
  }catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
 }
 async handle(route,body,sessionToken,displayName,fieldListSize,nativeCalendar=false){
  await this.ready;
  if(!this.allowRequest())return {status:429,error:'request_limit'};
  if(typeof displayName==='string')this.careers.profile(displayName);
  let frame;try{frame=framedRoute(route)?splitContentFrame(body,fieldListSize):{metadata:Buffer.from(body),payload:Buffer.alloc(0)}}catch(error){return {status:400,error:error.message}}
  let schema;try{schema=protocol.summary(protocol.decodeRequest(frame.metadata,{allowRepeated:repeatedRoute(route)}))}catch{}
  this.events.push({route,bytes:body.byteLength,schema});if(this.events.length>32)this.events.shift();
  let result=this.adapter.handle(route,frame.metadata,{sessionToken,nativeCalendar});
  // The adapter returns null only after validating the issued game session.
  if(!result&&route.startsWith('/Session/OnlineContent'))try{
   result=await profileContent(this.careers,route,frame.metadata,frame.payload,displayName);
  }catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&route.startsWith('/Session/OnlineContent'))try{result=await this.nativeContent.handle(route,frame.metadata,frame.payload)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.checkpoints&&route===CHECKPOINT_ROUTE)try{result=this.checkpoints.handle(frame.metadata)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.nativeCareers&&route===CAREER_UPLOAD_ROUTE)try{result=await this.nativeCareers.upload(frame.metadata,frame.payload)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.nativeCareers&&route===CAREER_ENUMERATE_ROUTE)try{result=this.nativeCareers.enumerate(frame.metadata,nativeCalendar)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.nativeCreation&&route===CAREER_CREATE_ROUTE)try{result=await this.nativeCreation.create(frame.metadata)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.nativeCreation&&route===CAREER_ATTRIBUTES_ROUTE)try{result=this.nativeCreation.attributes(frame.metadata)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.checkpointsEnabled&&route===CITY_ELECTION_ROUTE)try{result=cityElection(frame.metadata)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  if(!result&&this.checkpointsEnabled&&route===CITY_CONFIG_ROUTE)try{result=cityConfig(frame.metadata)}catch(error){if(error instanceof StoreError)return {status:error.status,error:error.message};throw error}
  await this.ctx.storage.put('sessions',[...this.sessions]);
  return result?{...result,body:result.body?new Uint8Array(result.body):undefined}
   :{status:501,error:'online_content_protocol_not_implemented'};
 }
}

export default {
 async fetch(request,env){
  const url=new URL(request.url);
  if(url.pathname==='/health'&&request.method==='GET')return json(200,{service:'nexaverse-nba22-compatibility',hosting:'cloudflare-workers',stage:'profile-storage-and-protocol-research',careerStorageReady:true,gameFormatVerified:false,mycareerReady:false});
  if(url.pathname.startsWith('/client/v1/')){
   if(request.headers.has('origin')||request.headers.has('sec-fetch-site'))return json(403,{error:'browser_request_denied'});
   if(!['GET','POST','PATCH','PUT'].includes(request.method))return json(405,{error:'unsupported_method'});
   try{
    const user=await playerIdentity(request);if(!user)return json(401,{error:'discord_login_required'});
    let body;try{body=await readBounded(request)}catch{return json(413,{error:'request_too_large'})}
    const header=request.headers.get('if-match'),base=header&&/^(?:"(0|[1-9]\d*)"|(0|[1-9]\d*))$/.test(header)?Number(header.replaceAll('"','')):null;
    const result=await env.PLAYER_SESSIONS.getByName(user.id).careerApi(request.method,url.pathname.slice('/client/v1'.length),body,base,user.displayName);
    if(result.error)return json(result.status,{error:result.error,mycareerReady:false});
    if(result.binary)return new Response(result.binary.bytes,{status:200,headers:{'Content-Type':'application/octet-stream','Content-Length':String(result.binary.bytes.byteLength),'ETag':'"'+result.binary.revision+'"','X-Save-SHA256':result.binary.sha256,'Cache-Control':'no-store'}});
    return json(result.status,result.data);
   }catch{return json(502,{error:'career_storage_unavailable',mycareerReady:false})}
  }
  if(url.pathname.startsWith('/client/')){
   if(request.headers.has('origin')||request.headers.has('sec-fetch-site'))return json(403,{error:'browser_request_denied'});
   const route=url.pathname.slice('/client'.length);
   if(route!=='/Session'&&!/^\/Session\/[A-Za-z0-9_/-]{1,120}$/.test(route))return json(404,{error:'unknown_route'});
   if(request.method!=='POST')return json(405,{error:'unsupported_method'});
   let stage='discord';
   try{
    const user=await playerIdentity(request);if(!user)return json(401,{error:'discord_login_required'});
    stage='body';
    let body;try{body=await readBounded(request)}catch{return json(413,{error:'request_too_large'})}
    let schema=null;try{
     const decoded=protocol.decodeRequest(framedRoute(route)?splitContentFrame(body,request.headers.get('vcfieldlist_size')).metadata:Buffer.from(body),{allowRepeated:repeatedRoute(route)});schema=protocol.summary(decoded);
     // These four observed CRC fields select content operations/categories.
     // Never include profile IDs, tokens, names, strings or binary contents.
     if(route.startsWith('/Session/OnlineContent'))for(const id of [0x28c55c06,0xc07f858d,0xd0101b9d,0x8f3c989b]){
      const field=decoded.fields.find(f=>f.id===id&&f.type==='STRINGCRC');
      if(field)schema.format+=';'+id.toString(16)+'='+field.value.toString(16);
     }
    }catch{}
    stage='player';
    const nativeCalendar=String(env.NEXAVERSE_NATIVE_CALENDAR_USERS||'').split(',').includes(user.id);
    if(schema)schema.format+=';calendar='+(nativeCalendar?'julian':'legacy');
    const result=await env.PLAYER_SESSIONS.getByName(user.id).handle(route,body,url.searchParams.get('nx_session'),user.displayName,request.headers.get('vcfieldlist_size'),nativeCalendar);
    stage='journal';
    if(schema)schema.format+=';http='+result.status;
    // HTTP 200 can still carry a native game failure. Record only the result
    // enum, never save bytes, session credentials or account/profile fields.
    if(schema&&result.body)try{
     const responseFields=protocol.decodeRequest(Buffer.from(result.body).subarray(0,result.fieldListSize??result.body.byteLength),{allowRepeated:repeatedRoute(route)||route==='/Session/OnlineContent'}).fields;
     const code=responseFields.find(f=>f.id===0xe3920695&&f.type==='STRINGCRC')?.value;
     if(Number.isInteger(code))schema.format+=';game-result='+code.toString(16);
    }catch{}
    if(/^\/Session\/(?:login|update|City\/(?:election|config)|Accounts\/get|Names\/validate|OnlineContent(?:\/(?:upload|download))?|MyCareer\/(?:checkpoint|upload|create|enumerate|attributes))$/.test(route))console.log(JSON.stringify({event:'native_checkpoint',route,status:result.status,error:result.error||null,schema}));
    await env.PROTOCOL_JOURNAL.getByName('nba22-protocol-v1').record({route,bytes:body.byteLength,protocol:schema});
    if(!result.body)return json(result.status,{error:result.error,mycareerReady:false});
    const headers={'Content-Type':'application/octet-stream','Content-Length':String(result.body.byteLength),'VCFIELDLIST_SIZE':String(result.fieldListSize??result.body.byteLength),'Cache-Control':'no-store'};
    const requestId=request.headers.get('vc-request-id');if(/^\d{1,20}$/.test(requestId||''))headers['Vc-Request-Id']=requestId;
    return new Response(result.body,{status:result.status,headers});
   }catch(error){
    const message=String(error?.message||'');
    const reason=message.includes('Illegal invocation')?'illegal-invocation':message.includes('AbortSignal')?'abort-signal':message.includes('redirect')?'redirect':message.includes('not a function')?'not-a-function':message.includes('fetch')?'fetch':message.includes('JSON')?'json':'other';
    const frames=String(error?.stack||'').split('\n').slice(1,3).map(line=>line.replace(/[^A-Za-z0-9_:. /()<>-]/g,'').slice(0,120));
    try{await env.PROTOCOL_JOURNAL.getByName('nba22-protocol-v1').record({route:'/gateway/error/'+stage,bytes:0,protocol:{format:String(error?.name||'Error')+':'+reason,fields:frames.map(name=>({name,type:'stack-frame'}))}})}catch{}
    return json(502,{error:'player_service_unavailable',mycareerReady:false});
   }
  }
  if(!authorized(request,env))return json(401,{error:'private_backend_authentication_required'});
  try{
   const journal=env.PROTOCOL_JOURNAL.getByName('nba22-protocol-v1');
   if(url.pathname==='/admin/status'&&request.method==='GET')return json(200,{mycareerReady:false,...await journal.status()});
   if(url.pathname!=='/Session'&&!/^\/Session\/[A-Za-z0-9_/-]{1,120}$/.test(url.pathname))return json(404,{error:'unknown_route'});
   if(request.method!=='POST'&&request.method!=='GET')return json(405,{error:'unsupported_method'});
   let body;try{body=await readBounded(request)}catch{return json(413,{error:'request_too_large'})}
   let decoded=null;if(url.pathname==='/Session/login'){try{decoded=protocol.summary(protocol.decodeRequest(body))}catch{}}
   await journal.record({route:url.pathname,bytes:body.byteLength,protocol:decoded});
   return json(501,{error:'game_protocol_not_implemented',mycareerReady:false});
  }catch{return json(500,{error:'backend_error'})}
 }
};
