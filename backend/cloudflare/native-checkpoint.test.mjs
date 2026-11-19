import assert from 'node:assert/strict';
import fs from 'node:fs';import os from 'node:os';import path from 'node:path';
import {Miniflare,Response as WorkerResponse,convertV4MiniflareOptions} from 'miniflare';
import protocol from '../protocol.cjs';
const persistence=fs.mkdtempSync(path.join(os.tmpdir(),'nba22-native-checkpoint-'));
const discord=async r=>new WorkerResponse(JSON.stringify({id:r.headers.get('authorization')==='Bearer owner'?'111111111111111111':'222222222222222222',global_name:'Test Display'}));
const start=()=>new Miniflare({...convertV4MiniflareOptions({name:'checkpoint-wire',bindings:{NEXAVERSE_NATIVE_CHECKPOINTS:'1'},outboundService:discord,unsafeInspectDurableObjects:true,modules:true,scriptPath:'.wrangler/check/worker.js',compatibilityDate:'2026-11-15',compatibilityFlags:['nodejs_compat'],durableObjects:{PROTOCOL_JOURNAL:{className:'ProtocolJournal',useSQLite:true},PLAYER_SESSIONS:{className:'PlayerSession',useSQLite:true}}}),resourcePersistencePath:persistence});
const f=(id,type,value)=>({id,type,value}),encode=protocol.encodeFields,decode=bytes=>protocol.decodeRequest(Buffer.from(bytes)).fields;
const value=(fields,id)=>fields.find(f=>f.id===id)?.value;
let mf=start();
const post=(route,body,key=null,token='owner')=>mf.dispatchFetch('https://fixture/client'+route+(key?'?nx_session='+key:''),{method:'POST',headers:{authorization:'Bearer '+token},body});
const login=async token=>{const r=await post('/Session/login',encode([f(0x01caaee8,'U64',1000),f(0x3e6d9a3b,'STRING','Test Display')]),null,token);assert.equal(r.status,200);return decode(await r.arrayBuffer())};
const request=(kind=0x682d973f,id=null,state='<synthetic-checkpoint>one</synthetic-checkpoint>',extra=[],{quest=123,questState=456,checkpointName=789,player=55,build=66}={})=>encode([f(0xb4433aa7,'STRINGCRC',quest),f(0x7a81dcb9,'STRINGCRC',questState),f(0x92f4e11e,'STRINGCRC',checkpointName),f(0x4009c831,'STRINGCRC',kind),f(0x4d461167,'STRING',state),f(0xd5e5f21d,'U64',player),f(0x5eeda966,'U64',build),...(id?[f(0x770c77d6,'U64',id)]:[]),...extra]);
const route='/Session/MyCareer/checkpoint';
try{
 const fields=await login('owner'),key=value(fields,0xda780e45),otherKey=value(await login('other'),0xda780e45);
 const manifest=protocol.decodeRequest(value(fields,0xb5017d25),{allowRepeated:true}).fields;
 assert(manifest.some(f=>f.value===0xfcfb638d));
 assert.equal((await post(route,request(),'invalid')).status,401);
 assert.equal((await post(route,request(),key,'other')).status,401);
 assert.equal((await post(route,request(0x859c1990),key)).status,400);
 assert.equal((await post(route,request(123),key)).status,400);
 assert.equal((await post(route,request(0x682d973f,null,'',[]),key)).status,400);
 assert.equal((await post(route,request(0x682d973f,null,'state',[f(0xdeadbeef,'U64',1)]),key)).status,400);
 assert.equal((await post(route,request(0x682d973f,null,'state',[f(0x01caaee8,'U64',1001)]),key)).status,403);
 let r=await post(route,request(),key);assert.equal(r.status,200);let result=decode(await r.arrayBuffer());assert.equal(value(result,0xe3920695),0x504521a8);const id=value(result,0x770c77d6);assert.equal(id,'1');
 r=await post(route,request(),key);assert.equal(value(decode(await r.arrayBuffer()),0x770c77d6),id);
 const storage=await mf.unsafeGetDurableObjectStorage('checkpoint-wire','PlayerSession',{name:'111111111111111111'});
 let row=(await storage.exec('SELECT * FROM native_checkpoints WHERE id=1'))[0];assert.equal(row.revision,1);assert.equal(row.state,'<synthetic-checkpoint>one</synthetic-checkpoint>');
 assert.equal((await post(route,request(0x859c1990,id,'other'),otherKey,'other')).status,404);
 r=await post(route,request(0x859c1990,id,'<synthetic-checkpoint>two</synthetic-checkpoint>',[],{questState:457,checkpointName:790}),key);assert.equal(r.status,200);
 row=(await storage.exec('SELECT * FROM native_checkpoints WHERE id=1'))[0];assert.equal(row.revision,2);assert.equal(row.state,'<synthetic-checkpoint>two</synthetic-checkpoint>');assert.equal(row.context,457);assert.equal(row.name,790);
 for(const changed of [{player:56},{build:67},{quest:124}])assert.equal((await post(route,request(0x859c1990,id,'cross-scope',[],changed),key)).status,409);
 const history=await storage.exec('SELECT * FROM native_checkpoint_history WHERE checkpoint_id=1');assert.equal(history.length,1);assert.equal(history[0].state,'<synthetic-checkpoint>one</synthetic-checkpoint>');assert.equal(history[0].quest_state,456);assert.equal(history[0].checkpoint_name,789);
 // Reopen an existing pre-migration history schema, preserving every state byte.
 await storage.exec('ALTER TABLE native_checkpoint_history DROP COLUMN quest_state');
 await storage.exec('ALTER TABLE native_checkpoint_history DROP COLUMN checkpoint_name');
 await mf.dispose();mf=start();
 assert.equal((await post(route,request(0x33492a41,id,'complete'),key)).status,200);
 const migrated=await mf.unsafeGetDurableObjectStorage('checkpoint-wire','PlayerSession',{name:'111111111111111111'});
 const columns=await migrated.exec('PRAGMA table_info(native_checkpoint_history)');assert(columns.some(c=>c.name==='quest_state'));assert(columns.some(c=>c.name==='checkpoint_name'));
 const prior=await migrated.exec('SELECT state FROM native_checkpoint_history WHERE checkpoint_id=1 AND revision=1');assert.equal(prior[0].state,'<synthetic-checkpoint>one</synthetic-checkpoint>');
 assert.equal((await post(route,request(0x33492a41,id,'complete'),key)).status,200);
 // A failed City transition can resume an explicitly identified quest instance.
 // Completion/termination are persisted reports, not a permanent write lock.
 assert.equal((await post(route,request(0x859c1990,id,'recovered-progress',[],{questState:458,checkpointName:791}),key)).status,200);
 assert.equal((await post(route,request(0x69bc56ca,id,'terminated'),key)).status,200);
 assert.equal((await post(route,request(0x859c1990,id,'resumed-after-termination'),key)).status,200);
 const resumed=(await migrated.exec('SELECT * FROM native_checkpoints WHERE id=1'))[0];
 assert.equal(resumed.state,'resumed-after-termination');assert.equal(resumed.kind,0x859c1990);
 const recoveryHistory=await migrated.exec('SELECT * FROM native_checkpoint_history WHERE checkpoint_id=1 ORDER BY revision');
 assert.equal(recoveryHistory.length,3);assert(recoveryHistory.some(row=>row.state==='terminated'));
 for(const changed of [{player:56},{build:67},{quest:124}])assert.equal((await post(route,request(0x859c1990,id,'cross-scope-after-recovery',[],changed),key)).status,409);
 assert.equal((await post(route,request(0x859c1990,id,'other-account'),otherKey,'other')).status,404);
 const bindings=await mf.getBindings(),journal=await bindings.PROTOCOL_JOURNAL.getByName('nba22-protocol-v1').status();
 assert(!JSON.stringify(journal).includes('synthetic-checkpoint'));assert(!JSON.stringify(journal).includes('Test Display'));
 assert.equal((await (await mf.dispatchFetch('https://fixture/health')).json()).mycareerReady,false);
 console.log('PASS: native checkpoint manifest, authenticated start/progress/completion/termination recovery, exact state persistence, restart, idempotent retry, bounded history, immutable career/quest scope, account isolation, malformed request rejection, redacted journal. In-game City remains unverified.');
}finally{await mf.dispose()}
