import {Buffer} from 'node:buffer';
import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';
const F={type:0x28c55c06,category:0xc07f858d,size:0x53d25be9,version:0x80592dd9,name:0x8cb84fe9,filename:0xfa387529,description:0xe0e7d2ce,file:0xcab9ab81};
const field=(id,type,value)=>({id,type,value}),packet=(code,extra=[])=>protocol.encodeFields([field(0xe3920695,'STRINGCRC',code),...extra]);
const SUCCESS=0x504521a8,ABSENT=0xd35eb7a9,MAX=4*1024*1024;
const fail=(status,code)=>{throw new StoreError(status,code)};
const digest=async bytes=>Buffer.from(await crypto.subtle.digest('SHA-256',bytes)).toString('hex');
function crc(text){let c=0xffffffff;for(const b of Buffer.from(text)){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0)}return (c^0xffffffff)>>>0}
export class NativeOnlineContent {
 constructor(storage){this.storage=storage;this.sql=storage.sql;
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_content_files (id INTEGER PRIMARY KEY AUTOINCREMENT,category INTEGER NOT NULL,content_type INTEGER NOT NULL,filename TEXT NOT NULL,name TEXT NOT NULL,revision INTEGER NOT NULL,UNIQUE(category,content_type,filename))');
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_content_versions (file_id INTEGER NOT NULL,revision INTEGER NOT NULL,bytes INTEGER NOT NULL,sha256 TEXT NOT NULL,metadata TEXT NOT NULL,PRIMARY KEY(file_id,revision))');
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_content_chunks (file_id INTEGER NOT NULL,revision INTEGER NOT NULL,part INTEGER NOT NULL,data BLOB NOT NULL,PRIMARY KEY(file_id,revision,part))');
 }
 async handle(route,metadata,payload){
  const fields=protocol.decodeRequest(metadata).fields,value=id=>fields.find(f=>f.id===id)?.value;
  const type=value(F.type),category=value(F.category);
  if(!Number.isInteger(type)||!Number.isInteger(category))fail(400,'content_scope_required');
  const allowed=new Map([[F.type,'STRINGCRC'],[F.category,'STRINGCRC'],[F.size,'U64'],[F.version,'U64'],[F.name,'STRING'],[F.filename,'STRING'],[F.description,'STRING'],[F.file,'U64'],[0x01caaee8,'U64'],[0xda780e45,'U64'],[0xbc5d6ac3,'U64'],[0xf5c431fd,'U64'],[0xfa6ede21,'U64'],[0x8f3c989b,'STRINGCRC'],[0xd0101b9d,'STRINGCRC']]);
  if(fields.some(f=>allowed.get(f.id)!==f.type))fail(400,'unsupported_native_content_field');
  if(route==='/Session/OnlineContent/upload'){
   const filename=value(F.filename),name=value(F.name)||'';
   if(typeof filename!=='string'||!filename.length||Buffer.byteLength(filename)>256||/[\x00-\x1f]/.test(filename)||Buffer.byteLength(name)>256)fail(400,'invalid_content_filename');
   payload=Buffer.from(payload);
   if(!payload.length||payload.length>MAX||value(F.size)===undefined||BigInt(value(F.size))!==BigInt(payload.length))fail(400,'incomplete_native_content_upload');
   const savedMetadata=JSON.stringify(fields.filter(f=>![0x01caaee8,0xda780e45].includes(f.id)).map(({id,type,value})=>({id,type,value})).sort((a,b)=>a.id-b.id));
   if(Buffer.byteLength(savedMetadata)>8192)fail(413,'native_content_metadata_limit');
   const sha=await digest(payload);
   const saved=this.storage.transactionSync(()=>{
    const rows=(q,...args)=>this.sql.exec(q,...args).toArray();
    let prior=rows('SELECT * FROM native_content_files WHERE category=? AND content_type=? AND filename=?',category,type,filename)[0];
    if(prior){const version=rows('SELECT * FROM native_content_versions WHERE file_id=? AND revision=?',prior.id,prior.revision)[0];if(version.sha256===sha&&version.metadata===savedMetadata)return prior}
    if(!prior&&rows('SELECT COUNT(*) AS n FROM native_content_files')[0].n>=32)fail(413,'native_content_file_limit');
    if(rows('SELECT COALESCE(SUM(bytes),0) AS n FROM native_content_versions')[0].n+payload.length>64*1024*1024)fail(413,'native_content_storage_limit');
    if(!prior){this.sql.exec('INSERT INTO native_content_files(category,content_type,filename,name,revision) VALUES (?,?,?,?,0)',category,type,filename,name);prior=rows('SELECT * FROM native_content_files WHERE id=last_insert_rowid()')[0]}
    const revision=prior.revision+1;
    this.sql.exec('INSERT INTO native_content_versions VALUES (?,?,?,?,?)',prior.id,revision,payload.length,sha,savedMetadata);
    for(let offset=0,part=0;offset<payload.length;offset+=65536,part++)this.sql.exec('INSERT INTO native_content_chunks VALUES (?,?,?,?)',prior.id,revision,part,payload.subarray(offset,offset+65536));
    this.sql.exec('UPDATE native_content_files SET revision=?,name=? WHERE id=?',revision,name,prior.id);
    for(const old of rows('SELECT revision FROM native_content_versions WHERE file_id=? ORDER BY revision DESC LIMIT -1 OFFSET 3',prior.id)){
     this.sql.exec('DELETE FROM native_content_chunks WHERE file_id=? AND revision=?',prior.id,old.revision);this.sql.exec('DELETE FROM native_content_versions WHERE file_id=? AND revision=?',prior.id,old.revision);
    }
    return {...prior,revision};
   });
   return {status:200,body:packet(SUCCESS,[field(F.file,'U64',saved.id)])};
  }
  if(payload.length)fail(400,'unexpected_content_payload');
  if(route==='/Session/OnlineContent'){
   // PPSA03669 callers 0x1b4f2e0/0x1b88f43 fetch an optional shared
   // document without the enumeration operation. Callback 0x1b4f350 handles
   // an absent document explicitly. This network supplies no such document;
   // do not turn its absence into HTTP 501 or invent a JSON configuration.
   if(category===0x8e4e9201&&type===0xede18177&&value(0xd0101b9d)===undefined){
    if(fields.some(f=>![F.type,F.category,F.version,0x01caaee8,0xda780e45,0xbc5d6ac3].includes(f.id)))fail(400,'unsupported_optional_content_field');
    return {status:200,body:packet(ABSENT)};
   }
   if(value(0xd0101b9d)!==0x636556e9)fail(501,'content_query_operation_unsupported');
   const filter=value(0x8f3c989b);
   const rows=this.sql.exec('SELECT f.*,v.bytes FROM native_content_files f JOIN native_content_versions v ON v.file_id=f.id AND v.revision=f.revision WHERE f.category=? AND f.content_type=? ORDER BY f.id',category,type).toArray().filter(r=>filter===undefined||crc(r.name)===filter||crc(r.filename)===filter);
   if(!rows.length)return {status:200,body:packet(ABSENT)};
   return {status:200,body:packet(SUCCESS,[field(0xbca73cb1,'U64',rows.length),...rows.flatMap(r=>[field(F.file,'U64',r.id),field(0x38144069,'U64',r.bytes)])])};
  }
  if(route==='/Session/OnlineContent/download'){
   const id=value(F.file);if(id===undefined)fail(400,'content_file_required');
   const row=this.sql.exec('SELECT f.*,v.bytes,v.sha256 FROM native_content_files f JOIN native_content_versions v ON v.file_id=f.id AND v.revision=f.revision WHERE f.id=? AND f.category=? AND f.content_type=?',id,category,type).toArray()[0];
   if(!row)return {status:200,body:packet(ABSENT)};
   const bytes=Buffer.concat(this.sql.exec('SELECT data FROM native_content_chunks WHERE file_id=? AND revision=? ORDER BY part',row.id,row.revision).toArray().map(c=>Buffer.from(c.data)));
   if(bytes.length!==row.bytes||await digest(bytes)!==row.sha256)fail(500,'native_content_integrity_error');
   const prefix=packet(SUCCESS,[field(F.size,'U64',bytes.length),field(F.file,'U64',row.id)]);
   return {status:200,body:Buffer.concat([prefix,bytes]),fieldListSize:prefix.length};
  }
  return null;
 }
}
