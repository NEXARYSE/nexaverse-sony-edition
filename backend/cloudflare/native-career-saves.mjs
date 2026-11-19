import {Buffer} from 'node:buffer';
import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';
export const CAREER_UPLOAD_ROUTE='/Session/MyCareer/upload';
export const CAREER_ENUMERATE_ROUTE='/Session/MyCareer/enumerate';
// Reflected ECMA CRC64, initial/final complement; PPSA03669 RVA 0x28ddfb0.
const mask=0xffffffffffffffffn,poly=0xc96c5795d7870f42n;
const crcTable=Array.from({length:256},(_,i)=>{let c=BigInt(i);for(let b=0;b<8;b++)c=c&1n?(c>>1n)^poly:c>>1n;return c});
const crcLow=Uint32Array.from(crcTable,c=>Number(c&0xffffffffn)),crcHigh=Uint32Array.from(crcTable,c=>Number(c>>32n));
export function nativeSaveCrc64(bytes){let lo=0xffffffff,hi=0xffffffff;for(const b of bytes){const i=(lo^b)&255;lo=((lo>>>8)|(hi<<24))^crcLow[i];hi=(hi>>>8)^crcHigh[i]}return ((BigInt((hi^0xffffffff)>>>0)<<32n)|BigInt((lo^0xffffffff)>>>0)).toString()}
const fail=(status,code)=>{throw new StoreError(status,code)};
const digest=async b=>[...new Uint8Array(await crypto.subtle.digest('SHA-256',b))].map(v=>v.toString(16).padStart(2,'0')).join('');
export class NativeCareerSaves {
 constructor(storage){this.storage=storage;this.sql=storage.sql;
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_career_saves (save_id TEXT PRIMARY KEY,revision INTEGER NOT NULL,bytes INTEGER NOT NULL,sha256 TEXT NOT NULL,updated_at TEXT NOT NULL)');
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_career_versions (save_id TEXT NOT NULL,revision INTEGER NOT NULL,bytes INTEGER NOT NULL,sha256 TEXT NOT NULL,PRIMARY KEY(save_id,revision))');
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_career_chunks (save_id TEXT NOT NULL,revision INTEGER NOT NULL,part INTEGER NOT NULL,data BLOB NOT NULL,PRIMARY KEY(save_id,revision,part))');
 }
 enumerate(metadata,nativeCalendar=true){
  let fields;try{fields=protocol.decodeRequest(metadata).fields}catch{fail(400,'invalid_career_enumeration_packet')}
  if(fields.some(f=>![0x01caaee8,0xda780e45].includes(f.id)||f.type!=='U64'))fail(400,'unsupported_career_enumeration_field');
  const rows=this.sql.exec('SELECT * FROM native_career_saves ORDER BY save_id').toArray();
  const result=[{id:0xe3920695,type:'STRINGCRC',value:0x504521a8},{id:0x7eddfc4d,type:'U64',value:rows.length}];
  for(const row of rows){
   const chunks=this.sql.exec('SELECT data FROM native_career_chunks WHERE save_id=? AND revision=? ORDER BY part',row.save_id,row.revision).toArray();
   const bytes=Buffer.concat(chunks.map(c=>Buffer.from(c.data)));
   if(bytes.length!==row.bytes)fail(500,'stored_career_incomplete');
   result.push({id:0x67267fb1,type:'U64',value:row.save_id},{id:0x620144f4,type:'U64',value:nativeSaveCrc64(bytes)},{id:0xc3ccd0ad,type:'DATETIME',value:nativeCalendar?protocol.gameDateTime(Date.parse(row.updated_at)):Date.parse(row.updated_at)});
  }
  return {status:200,body:protocol.encodeFields(result)};
 }
 async upload(metadata,payload){
  let fields;try{fields=protocol.decodeRequest(metadata).fields}catch{fail(400,'invalid_career_packet')}
  const types=new Map([[0x67267fb1,'U64'],[0x53d25be9,'U64'],[0x01caaee8,'U64'],[0xda780e45,'U64']]);
  if(fields.some(f=>types.get(f.id)!==f.type))fail(400,'unsupported_career_upload_field');
  const value=id=>fields.find(f=>f.id===id)?.value;
  const id=value(0x67267fb1),size=value(0x53d25be9);payload=new Uint8Array(payload);
  if(id===undefined||size===undefined)fail(400,'career_upload_fields_required');
  if(!payload.length||payload.length>4*1024*1024||BigInt(size)!==BigInt(payload.length))fail(400,'incomplete_career_upload');
  const sha=await digest(payload);
  this.storage.transactionSync(()=>{
   const rows=(q,...a)=>this.sql.exec(q,...a).toArray();
   const prior=rows('SELECT * FROM native_career_saves WHERE save_id=?',id)[0];
   if(prior&&prior.sha256===sha&&prior.bytes===payload.length)return;
   if(!prior&&rows('SELECT COUNT(*) AS n FROM native_career_saves')[0].n>=10)fail(413,'native_career_slot_limit');
   const revision=(prior?.revision||0)+1;
   this.sql.exec('INSERT INTO native_career_versions VALUES (?,?,?,?)',id,revision,payload.length,sha);
   for(let offset=0,part=0;offset<payload.length;offset+=65536,part++)this.sql.exec('INSERT INTO native_career_chunks VALUES (?,?,?,?)',id,revision,part,payload.slice(offset,offset+65536));
   this.sql.exec('INSERT INTO native_career_saves VALUES (?,?,?,?,?) ON CONFLICT(save_id) DO UPDATE SET revision=excluded.revision,bytes=excluded.bytes,sha256=excluded.sha256,updated_at=excluded.updated_at',id,revision,payload.length,sha,new Date().toISOString());
   for(const old of rows('SELECT revision FROM native_career_versions WHERE save_id=? ORDER BY revision DESC LIMIT -1 OFFSET 3',id)){
    this.sql.exec('DELETE FROM native_career_chunks WHERE save_id=? AND revision=?',id,old.revision);this.sql.exec('DELETE FROM native_career_versions WHERE save_id=? AND revision=?',id,old.revision);
   }
  });
  // Native upload response at 0x1c05377 requires RESULT only. It is sent only
  // after the complete client-supplied save and manifest have committed.
  return {status:200,body:protocol.encodeFields([{id:0xe3920695,type:'STRINGCRC',value:0x504521a8}])};
 }
}
