import assert from 'node:assert/strict';
import {Miniflare,convertV4MiniflareOptions} from 'miniflare';
import protocol from '../protocol.cjs';
import fs from 'node:fs';import os from 'node:os';import path from 'node:path';
const persistence=fs.mkdtempSync(path.join(os.tmpdir(),'nexaverse-player-test-'));
const makeRuntime=()=>new Miniflare({...convertV4MiniflareOptions({name:'fixture',modules:true,scriptPath:'.wrangler/check/worker.js',compatibilityDate:'2026-11-15',compatibilityFlags:['nodejs_compat'],durableObjects:{PROTOCOL_JOURNAL:{className:'ProtocolJournal',useSQLite:true},PLAYER_SESSIONS:{className:'PlayerSession',useSQLite:true}}}),resourcePersistencePath:persistence});
let mf=makeRuntime();
try{
 const env=await mf.getBindings();
 const fixture=Buffer.from('01caaee83d9e508900000000000000013e6d9a3b6e46752f0000000000000005000000000000000000000000000000005465737400000000','hex');
 const first=env.PLAYER_SESSIONS.getByName('123456789012345678');
 const login=await first.handle('/Session/login',fixture,null);assert.equal(login.status,200);
 const fields=protocol.decodeRequest(Buffer.from(login.body)).fields;
 const key=fields.find(f=>f.id===0xda780e45).value;
 const manifest=protocol.decodeRequest(fields.find(f=>f.id===0xb5017d25).value,{allowRepeated:true});
 assert(manifest.fields.some(f=>f.value===0x8c8b8bf4));
 assert.equal((await first.handle('/Session/Accounts/get',protocol.encodeFields([]),key)).status,200);
 const other=env.PLAYER_SESSIONS.getByName('987654321098765432');
 assert.equal((await other.handle('/Session/Accounts/get',protocol.encodeFields([]),key)).status,401);
 assert.equal((await env.PLAYER_SESSIONS.getByName('123456789012345678').handle('/Session/OnlineContent',protocol.encodeFields([]),key)).status,501);
 const enumerate=protocol.encodeFields([{id:0xc07f858d,type:'STRINGCRC',value:0x7ee096e5},{id:0x28c55c06,type:'STRINGCRC',value:0xcdbd175e}]);const absent=await first.handle('/Session/OnlineContent',enumerate,key);assert.equal(absent.status,200);assert.equal(protocol.decodeRequest(Buffer.from(absent.body)).fields[0].value,0xd35eb7a9);assert.equal((await other.handle('/Session/OnlineContent',enumerate,key)).status,401);
 const denied=await mf.dispatchFetch('https://fixture/client/Session/login',{method:'POST',body:fixture});assert.equal(denied.status,401);
 const browser=await mf.dispatchFetch('https://fixture/client/Session/login',{method:'POST',body:fixture,headers:{origin:'https://other.example'}});assert.equal(browser.status,403);
 await mf.dispose();mf=makeRuntime();const restored=await mf.getBindings();assert.equal((await restored.PLAYER_SESSIONS.getByName('123456789012345678').handle('/Session/Accounts/get',protocol.encodeFields([]),key)).status,200);
 console.log('PASS: real Worker runtime: per-player Durable Object login/account flow, missing OnlineContent advertised, cross-account sessions denied, unfinished content returns 501 and unauthenticated/browser clients denied.');
}finally{await mf.dispose()}
