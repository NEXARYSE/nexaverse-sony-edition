import assert from 'node:assert/strict';
import {Miniflare,Response as WorkerResponse,convertV4MiniflareOptions} from 'miniflare';
import protocol from '../protocol.cjs';
const mf=new Miniflare(convertV4MiniflareOptions({name:'city-config-wire',unsafeInspectDurableObjects:true,bindings:{NEXAVERSE_NATIVE_CHECKPOINTS:'1'},outboundService:async()=>new WorkerResponse(JSON.stringify({id:'111111111111111111',global_name:'Fixture'})),modules:true,scriptPath:'.wrangler/check/worker.js',compatibilityDate:'2026-11-15',compatibilityFlags:['nodejs_compat'],durableObjects:{PROTOCOL_JOURNAL:{className:'ProtocolJournal',useSQLite:true},PLAYER_SESSIONS:{className:'PlayerSession',useSQLite:true}}}));
const f=(id,type,value)=>({id,type,value}),encode=protocol.encodeFields;
const decode=body=>protocol.decodeRequest(Buffer.from(body),{allowRepeated:true}).fields;
const value=(fields,id)=>fields.find(f=>f.id===id)?.value;
const post=(route,fields,key)=>mf.dispatchFetch('https://fixture/client'+route+(key?'?nx_session='+key:''),{method:'POST',headers:{authorization:'Bearer fixture'},body:encode(fields)});
try{
 const login=await post('/Session/login',[f(0x01caaee8,'U64',1000),f(0x3e6d9a3b,'STRING','Fixture')]);assert.equal(login.status,200);
 const fields=decode(await login.arrayBuffer()),key=value(fields,0xda780e45),manifest=decode(value(fields,0xb5017d25));
 const entry=manifest.findIndex(f=>f.value===0xc39a9528);assert(entry>=0);assert(value(manifest.slice(entry,entry+5),0x12bbf3ab).includes('/Session/City/config?'));
 const route='/Session/City/config',request=[f(0xd5e5f21d,'U64',1),f(0x952ef509,'WSTRING','CATEGORY:BALL_STREET')];
 assert.equal((await post(route,request,'invalid')).status,401);
 for(const invalid of [[],request.slice(0,1),[request[0],f(0x952ef509,'STRING','unknown')],[...request,f(1,'U64',1)],[f(0xd5e5f21d,'STRING','1'),request[1]]])assert.equal((await post(route,invalid,key)).status,400);
 for(const tag of ['CATEGORY:BALL_STREET','CATEGORY:BALL_STREET_NBA','CATEGORY:ICON','category:ball_street','PLAYER:1234567890123456','shared settings']){
  const response=await post(route,[request[0],f(0x952ef509,'WSTRING',tag)],key);assert.equal(response.status,200);
  assert.deepEqual(decode(await response.arrayBuffer()),decode(encode([f(0xe3920695,'STRINGCRC',0x504521a8)])));
 }
 assert.equal((await post(route,[f(0xd5e5f21d,'U64',0),request[1]],key)).status,200);
 for(const tag of ['', ' ', 'A'.repeat(129),'bad\nkey'])assert.equal((await post(route,[request[0],f(0x952ef509,'WSTRING',tag)],key)).status,400);
 // No synthetic career, rating, checkpoint, reward, or City-session state.
 const storage=await mf.unsafeGetDurableObjectStorage('city-config-wire','PlayerSession',{name:'111111111111111111'});
 for(const table of ['native_career_builds','native_career_values','native_career_saves','native_checkpoints'])assert.equal((await storage.exec('SELECT COUNT(*) AS n FROM '+table))[0].n,0);
 assert.equal((await(await mf.dispatchFetch('https://fixture/health')).json()).mycareerReady,false);
 console.log('PASS: missing City config service advertised, authenticated native request, optional overrides omitted together, strict fields, stable retries and no career mutations. City entry remains unverified.');
}finally{await mf.dispose()}
