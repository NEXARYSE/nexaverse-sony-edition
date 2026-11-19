import assert from 'node:assert/strict';
import fs from 'node:fs';import os from 'node:os';import path from 'node:path';
import {Miniflare,Response as WorkerResponse,convertV4MiniflareOptions} from 'miniflare';
import protocol from '../protocol.cjs';
const persistence=fs.mkdtempSync(path.join(os.tmpdir(),'nba22-profile-wire-'));
const outboundService=async request=>{
 assert.equal(request.url,'https://discord.com/api/v10/users/@me');
 const authorization=request.headers.get('authorization');
 const id=authorization==='Bearer owner'?'111111111111111111':authorization==='Bearer other'?'222222222222222222':null;
 return new WorkerResponse(JSON.stringify(id?{id,global_name:'Display Name'}:{}),{status:id?200:401});
};
const start=()=>new Miniflare({...convertV4MiniflareOptions({name:'profile-wire',outboundService,modules:true,scriptPath:'.wrangler/check/worker.js',compatibilityDate:'2026-11-15',compatibilityFlags:['nodejs_compat'],durableObjects:{PROTOCOL_JOURNAL:{className:'ProtocolJournal',useSQLite:true},PLAYER_SESSIONS:{className:'PlayerSession',useSQLite:true}}}),resourcePersistencePath:persistence});
const field=(id,type,value)=>({id,type,value});
const encode=protocol.encodeFields,decode=bytes=>protocol.decodeRequest(Buffer.from(bytes)).fields;
const value=(fields,id)=>fields.find(f=>f.id===id)?.value;
const metadata=encode([field(0xc07f858d,'STRINGCRC',0x7ee096e5),field(0x28c55c06,'STRINGCRC',0xcdbd175e),field(0x80592dd9,'U64',1),field(0x53d25be9,'U64',180001)]);
const login=encode([field(0x01caaee8,'U64',1000),field(0x3e6d9a3b,'STRING','Display Name')]);
let mf=start();
const post=(route,body,key=null,token='owner',size=null)=>mf.dispatchFetch('https://fixture/client'+route+(key?'?nx_session='+key:''),{method:'POST',headers:{authorization:'Bearer '+token,...(size===null?{}:{vcfieldlist_size:String(size)})},body});
async function signIn(token='owner'){
 const r=await post('/Session/login',login,null,token);assert.equal(r.status,200);
 const fields=decode(await r.arrayBuffer());return {key:value(fields,0xda780e45),fields};
}
try{
 const session=await signIn(),key=session.key;
 const manifest=protocol.decodeRequest(value(session.fields,0xb5017d25),{allowRepeated:true}).fields;
 assert(manifest.some(f=>f.value===0x8290b650));assert(manifest.some(f=>f.value===0xca7a1109));
 assert(manifest.some(f=>f.value===0x0635c793));
 const nameRequest=encode([field(0x6e46752f,'STRING','Jean-Luc'),field(0x6e46752f,'STRING',"O'Neal")]);
 let nr=await post('/Session/Names/validate',nameRequest,key);assert.equal(nr.status,200);
 const nameResults=protocol.decodeRequest(Buffer.from(await nr.arrayBuffer()),{allowRepeated:true}).fields;
 assert.deepEqual(nameResults.filter(f=>f.type==='BOOL').map(f=>f.value),[true,true]);
 assert.equal((await post('/Session/Names/validate',nameRequest,'123')).status,401);
 assert.equal((await post('/Session/Names/validate',nameRequest,key,'other')).status,401);
 let r=await post('/Session/OnlineContent',metadata,key);assert.equal(value(decode(await r.arrayBuffer()),0xe3920695),0xd35eb7a9);
 const blob=Buffer.from(Uint8Array.from({length:180001},(_,i)=>i%251)); // Synthetic opaque fixture, not a claimed game save.
 const upload=Buffer.concat([metadata,blob]);
 assert.equal((await post('/Session/OnlineContent/upload',upload,'123','owner',metadata.length)).status,401);
 const other=await signIn('other');assert.equal((await post('/Session/OnlineContent/upload',upload,key,'other',metadata.length)).status,401);
 assert.equal((await post('/Session/OnlineContent/upload',metadata,key,'owner',metadata.length)).status,400);
 assert.equal((await post('/Session/OnlineContent/upload',upload.subarray(0,32768),key,'owner',metadata.length)).status,400);
 r=await post('/Session/OnlineContent',metadata,key);assert.equal(value(decode(await r.arrayBuffer()),0xe3920695),0xd35eb7a9);
 assert.equal((await post('/Session/OnlineContent/upload',upload,key,'owner',metadata.length+1)).status,400);
 r=await post('/Session/OnlineContent/upload',upload,key,'owner',metadata.length);assert.equal(r.status,200);assert.equal(value(decode(await r.arrayBuffer()),0xe3920695),0x504521a8);
 r=await post('/Session/OnlineContent',metadata,key);const enumeration=decode(await r.arrayBuffer());assert.equal(value(enumeration,0xbca73cb1),'1');assert.equal(value(enumeration,0x38144069),String(blob.length));assert.equal(value(enumeration,0xcab9ab81),'1');
 const download=encode([...protocol.decodeRequest(metadata).fields,field(0xcab9ab81,'U64',1)]);
 async function checkDownload(){
  const r=await post('/Session/OnlineContent/download',download,key);assert.equal(r.status,200);
  const size=Number(r.headers.get('vcfieldlist_size')),bytes=Buffer.from(await r.arrayBuffer());
  const responseFields=decode(bytes.subarray(0,size));
  assert.equal(value(responseFields,0xe3920695),0x504521a8);
  // Independent native-client requirement: download callback uses this U64,
  // not enumeration's DATASIZE (0x38144069), for profile decoding.
  assert.equal(value(responseFields,0x53d25be9),String(blob.length));
  assert.deepEqual(bytes.subarray(size),blob);
 }
 await checkDownload();
 const bindings=await mf.getBindings(),journal=await bindings.PROTOCOL_JOURNAL.getByName('nba22-protocol-v1').status();
 assert(journal.recent.some(e=>e.summary.format.includes(';game-result=504521a8')));
 assert(!JSON.stringify(journal).includes('Display Name'));
 r=await post('/Session/OnlineContent',metadata,other.key,'other');assert.equal(value(decode(await r.arrayBuffer()),0xe3920695),0xd35eb7a9);
 await mf.dispose();mf=start();await checkDownload();
 console.log('PASS: advertised upload/download IDs, authenticated framed profile upload, persistent enumeration/download, byte-exact reload after runtime restart, malformed/missing payload rejection and cross-account denial. Synthetic wire fixtures do not prove in-game acceptance.');
}finally{await mf.dispose()}
