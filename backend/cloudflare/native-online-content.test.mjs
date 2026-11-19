import assert from 'node:assert/strict';
import fs from 'node:fs';import os from 'node:os';import path from 'node:path';
import {Miniflare,Response as WorkerResponse,convertV4MiniflareOptions} from 'miniflare';
import protocol from '../protocol.cjs';
const persistence=fs.mkdtempSync(path.join(os.tmpdir(),'nba22-native-content-'));
const start=()=>new Miniflare({...convertV4MiniflareOptions({name:'native-content-wire',unsafeInspectDurableObjects:true,bindings:{NEXAVERSE_NATIVE_CHECKPOINTS:'1'},outboundService:async r=>new WorkerResponse(JSON.stringify({id:r.headers.get('authorization')==='Bearer owner'?'111111111111111111':'222222222222222222',global_name:'Fixture'})),modules:true,scriptPath:'.wrangler/check/worker.js',compatibilityDate:'2026-11-15',compatibilityFlags:['nodejs_compat'],durableObjects:{PROTOCOL_JOURNAL:{className:'ProtocolJournal',useSQLite:true},PLAYER_SESSIONS:{className:'PlayerSession',useSQLite:true}}}),resourcePersistencePath:persistence});
let mf=start();
const f=(id,type,value)=>({id,type,value}),encode=protocol.encodeFields,decode=b=>protocol.decodeRequest(Buffer.from(b),{allowRepeated:true}).fields,val=(fields,id)=>fields.find(f=>f.id===id)?.value;
const post=(route,body,key=null,token='owner',prefix=null)=>mf.dispatchFetch('https://fixture/client'+route+(key?'?nx_session='+key:''),{method:'POST',headers:{authorization:'Bearer '+token,...(prefix===null?{}:{vcfieldlist_size:String(prefix)})},body});
const login=async token=>{const r=await post('/Session/login',encode([f(0x01caaee8,'U64',1000),f(0x3e6d9a3b,'STRING','Fixture')]),null,token);assert.equal(r.status,200);return val(decode(await r.arrayBuffer()),0xda780e45)};
const scope=[f(0xc07f858d,'STRINGCRC',0x12345678),f(0x28c55c06,'STRINGCRC',0x23456789)];
const payload=Buffer.from(Uint8Array.from({length:4154320},(_,i)=>i%251));
const meta=(bytes,name='MyCAREER0001')=>encode([...scope,f(0x53d25be9,'U64',bytes.length),f(0x80592dd9,'U64',1),f(0x8cb84fe9,'STRING',name),f(0xfa387529,'STRING',name+'.save'),f(0xe0e7d2ce,'STRING','Career fixture'),f(0xbc5d6ac3,'U64',0)]);
const upload=(bytes,key,token='owner',name)=>{const prefix=meta(bytes,name);return post('/Session/OnlineContent/upload',Buffer.concat([prefix,bytes]),key,token,prefix.length)};
const query=key=>post('/Session/OnlineContent',encode([...scope,f(0xd0101b9d,'STRINGCRC',0x636556e9)]),key);
const download=(id,key,token='owner',fields=scope)=>post('/Session/OnlineContent/download',encode([...fields,f(0xcab9ab81,'U64',id)]),key,token);
try{
 const key=await login('owner'),otherKey=await login('other');
 // Independently captured PS5 optional-document request, without the list operation.
 const optional=encode([f(0x01caaee8,'U64',1000),f(0x28c55c06,'STRINGCRC',0xede18177),f(0xc07f858d,'STRINGCRC',0x8e4e9201),f(0x80592dd9,'U64',0),f(0xbc5d6ac3,'U64',0)]);
 assert.equal((await post('/Session/OnlineContent',optional,'invalid')).status,401);
 for(const account of [[key,'owner'],[otherKey,'other']]){
  const missing=await post('/Session/OnlineContent',optional,...account);assert.equal(missing.status,200);
  assert.deepEqual(decode(await missing.arrayBuffer()),decode(encode([f(0xe3920695,'STRINGCRC',0xd35eb7a9)])));
 }
 const unsupported=await post('/Session/OnlineContent',encode([f(0x28c55c06,'STRINGCRC',0xede18176),f(0xc07f858d,'STRINGCRC',0x8e4e9201)]),key);assert.equal(unsupported.status,501);

 assert.equal((await upload(payload,'invalid')).status,401);
 assert.equal((await upload(payload,key,'other')).status,401);
 const prefix=meta(payload);assert.equal((await post('/Session/OnlineContent/upload',Buffer.concat([prefix,payload.subarray(0,32768)]),key,'owner',prefix.length)).status,400);
 let r=await upload(payload,key);assert.equal(r.status,200);const response=decode(await r.arrayBuffer()),id=val(response,0xcab9ab81);assert.equal(val(response,0xe3920695),0x504521a8);
 r=await upload(payload,key);assert.equal(r.status,200);assert.equal(val(decode(await r.arrayBuffer()),0xcab9ab81),id);
 let storage=await mf.unsafeGetDurableObjectStorage('native-content-wire','PlayerSession',{name:'111111111111111111'});assert.equal((await storage.exec('SELECT revision FROM native_content_files'))[0].revision,1);
 r=await query(key);assert.equal(r.status,200);let listed=decode(await r.arrayBuffer());assert.equal(val(listed,0xbca73cb1),'1');assert.equal(val(listed,0xcab9ab81),id);assert.equal(val(listed,0x38144069),String(payload.length));
 r=await download(id,key);assert.equal(r.status,200);let framed=Buffer.from(await r.arrayBuffer());assert.equal(val(decode(framed.subarray(0,Number(r.headers.get('vcfieldlist_size')))),0x53d25be9),String(payload.length));assert.deepEqual(framed.subarray(Number(r.headers.get('vcfieldlist_size'))),payload);
 assert.equal(val(decode(await(await download(id,otherKey,'other')).arrayBuffer()),0xe3920695),0xd35eb7a9);
 assert.equal(val(decode(await(await download(id,key,'owner',[f(0xc07f858d,'STRINGCRC',0x12345679),scope[1]])).arrayBuffer()),0xe3920695),0xd35eb7a9);
 await mf.dispose();mf=start();r=await download(id,key);assert.deepEqual(Buffer.from(await r.arrayBuffer()).subarray(Number(r.headers.get('vcfieldlist_size'))),payload);
 for(let n=2;n<=5;n++)assert.equal((await upload(Buffer.alloc(4096,n),key)).status,200);
 storage=await mf.unsafeGetDurableObjectStorage('native-content-wire','PlayerSession',{name:'111111111111111111'});assert.deepEqual((await storage.exec('SELECT revision FROM native_content_versions ORDER BY revision')).map(r=>r.revision),[3,4,5]);
 assert.equal((await upload(Buffer.alloc(64,9),key,'owner','MyCAREER0002')).status,200);listed=decode(await(await query(key)).arrayBuffer());assert.equal(val(listed,0xbca73cb1),'2');assert.equal(listed.filter(f=>f.id===0xcab9ab81).length,2);
 await storage.exec('DELETE FROM native_content_chunks WHERE file_id=? AND revision=5',id);assert.equal((await download(id,key)).status,500);
 assert.equal((await(await mf.dispatchFetch('https://fixture/health')).json()).mycareerReady,false);
 console.log('PASS: authenticated native content upload, exact 4,154,320-byte framed download, lookup, retry stability, restart, category/account isolation, three retained versions, truncated upload and stored-data integrity rejection. Live MyCAREER remains unverified.');
}finally{await mf.dispose()}
