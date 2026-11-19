import assert from 'node:assert/strict';
import {Miniflare,Response as WorkerResponse,convertV4MiniflareOptions} from 'miniflare';
import protocol from '../protocol.cjs';
const mf=new Miniflare(convertV4MiniflareOptions({name:'city-election-wire',bindings:{NEXAVERSE_NATIVE_CHECKPOINTS:'1'},outboundService:async()=>new WorkerResponse(JSON.stringify({id:'111111111111111111',global_name:'Fixture'})),modules:true,scriptPath:'.wrangler/check/worker.js',compatibilityDate:'2026-11-15',compatibilityFlags:['nodejs_compat'],durableObjects:{PROTOCOL_JOURNAL:{className:'ProtocolJournal',useSQLite:true},PLAYER_SESSIONS:{className:'PlayerSession',useSQLite:true}}}));
const f=(id,type,value)=>({id,type,value}),encode=protocol.encodeFields,decode=body=>protocol.decodeRequest(Buffer.from(body),{allowRepeated:true}).fields,value=(fields,id)=>fields.find(f=>f.id===id)?.value;
const post=(route,fields,key=null)=>mf.dispatchFetch('https://fixture/client'+route+(key?'?nx_session='+key:''),{method:'POST',headers:{authorization:'Bearer fixture'},body:encode(fields)});
try{
 let r=await post('/Session/login',[f(0x01caaee8,'U64',1000),f(0x3e6d9a3b,'STRING','Fixture')]);assert.equal(r.status,200);
 const login=decode(await r.arrayBuffer()),key=value(login,0xda780e45),manifest=decode(value(login,0xb5017d25));assert(manifest.some(f=>f.value===0x612f6478));
 const route='/Session/City/election',request=[f(0x45c6fcb2,'STRINGCRC',1)];
 assert.equal((await post(route,request,'invalid')).status,401);
 assert.equal((await post(route,[],key)).status,400);
 assert.equal((await post(route,[f(0x45c6fcb2,'U64',1)],key)).status,400);
 assert.equal((await post(route,[...request,f(1,'U64',1)],key)).status,400);
 for(let attempt=0;attempt<2;attempt++){
  r=await post(route,request,key);assert.equal(r.status,200);const fields=decode(await r.arrayBuffer());
  assert.equal(value(fields,0xe3920695),0x504521a8);
  assert.deepEqual(JSON.parse(value(fields,0x9b205d17)),[]);
  const election=JSON.parse(value(fields,0x1a93a44c));assert.deepEqual(election,{candidates:[]});assert.equal(election.election_id,undefined);assert.equal(election.start_time,undefined);assert.equal(election.end_time,undefined);
 }
 assert.equal((await(await mf.dispatchFetch('https://fixture/health')).json()).mycareerReady,false);
 console.log('PASS: advertised PS5 City-election service, authenticated empty history and no configured candidates, native JSON descriptor types, retry stability, malformed request rejection. Live City entry remains unverified.');
}finally{await mf.dispose()}
