import {Buffer} from 'node:buffer';
import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';

const F={result:0xe3920695,count:0xbca73cb1,size:0x38144069,downloadSize:0x53d25be9,file:0xcab9ab81};
const SUCCESS=0x504521a8,ABSENT=0xd35eb7a9;
const field=(id,type,value)=>({id,type,value});
const encode=fields=>protocol.encodeFields(fields);
const packet=(code,extra=[])=>encode([field(F.result,'STRINGCRC',code),...extra]);

// VCFIELDLIST_SIZE delimits descriptor metadata from streaming upload bytes.
// A malformed frame is never acknowledged or stored as a successful save.
export function splitContentFrame(body,header){
 const bytes=Buffer.from(body);
 if(header===undefined||header===null)return {metadata:bytes,payload:Buffer.alloc(0)};
 if(!/^(0|[1-9]\d*)$/.test(String(header)))throw new StoreError(400,'invalid_field_list_size');
 const length=Number(header);
 if(!Number.isSafeInteger(length)||length<16||length>bytes.length||length%8)throw new StoreError(400,'invalid_field_list_size');
 const metadata=bytes.subarray(0,length);protocol.decodeRequest(metadata);
 return {metadata,payload:bytes.subarray(length)};
}

export async function profileContent(store,route,metadata,payload,displayName){
 const fields=protocol.decodeRequest(metadata).fields;
 const value=id=>fields.find(f=>f.id===id)?.value;
 if(value(0xc07f858d)!==0x7ee096e5||value(0x28c55c06)!==0xcdbd175e)return null;
 const profile=store.profile(displayName||'Player');
 const nativeData=profile.data&&store.nativeProfileRevision()===profile.data.revision;
 if(route==='/Session/OnlineContent/upload'){
  if(!payload.length)throw new StoreError(400,'profile_payload_required');
  // Native upload builder publishes its complete payload size in this U64.
  // Reject an individual 32 KiB sceHttpSendRequest chunk as an incomplete save.
  const declared=value(0x53d25be9);
  if(typeof declared!=='string'||!/^\d+$/.test(declared)||BigInt(declared)!==BigInt(payload.length))throw new StoreError(400,'incomplete_profile_upload');
  // Revisions are checked again inside the atomic storage transaction. A racing
  // device gets a conflict instead of overwriting a newer save after a hash await.
  await store.saveProfile(profile.revision,payload,{native:true});
  return {status:200,body:packet(SUCCESS)};
 }
 if(route==='/Session/OnlineContent'){
  if(payload.length)throw new StoreError(400,'unexpected_content_payload');
  if(!nativeData)return {status:200,body:packet(ABSENT)};
  return {status:200,body:packet(SUCCESS,[field(F.count,'U64',1),field(F.size,'U64',profile.data.bytes),field(F.file,'U64',1)])};
 }
 if(route==='/Session/OnlineContent/download'){
  if(payload.length)throw new StoreError(400,'unexpected_content_payload');
  if(value(F.file)!=='1')throw new StoreError(404,'profile_file_not_found');
  if(!nativeData)return {status:200,body:packet(ABSENT)};
  const saved=await store.loadProfile();
  // PPSA03669 DOWNLOAD_REQUEST callback at RVA 0xf82e0 reads 0x53d25be9
  // into the decoder's byte count. Enumeration reads a different size field.
  const prefix=packet(SUCCESS,[field(F.downloadSize,'U64',saved.bytes.length),field(F.file,'U64',1)]);
  return {status:200,body:Buffer.concat([prefix,Buffer.from(saved.bytes)]),fieldListSize:prefix.length};
 }
 return null;
}
