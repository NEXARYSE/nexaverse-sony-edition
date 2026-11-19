// Account-scoped storage. Payloads are opaque game data, never synthetic 2K saves.
const MAX_SAVE=4*1024*1024,CHUNK=64*1024,KEEP=3;
export class StoreError extends Error{constructor(status,code){super(code);this.status=status}}
const fail=(status,code)=>{throw new StoreError(status,code)};
const revision=value=>{if(!Number.isSafeInteger(value)||value<0)fail(428,'revision_required');return value};
const text=(value,max)=>{if(typeof value!=='string'||!value.trim()||value.length>max||/[\u0000-\u001f]/u.test(value))fail(400,'invalid_name');return value.trim()};
const object=value=>{if(!value||typeof value!=='object'||Array.isArray(value))fail(400,'invalid_object');const s=JSON.stringify(value);if(s.length>65536)fail(413,'metadata_too_large');return s};
const uuid=value=>{if(typeof value!=='string'||! /^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/i.test(value))fail(400,'invalid_identifier');return value};
async function digest(bytes){return [...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(x=>x.toString(16).padStart(2,'0')).join('')}
export class CareerStore{
 constructor(storage){
  this.storage=storage;this.sql=storage.sql;
  this.sql.exec('CREATE TABLE IF NOT EXISTS player_profile (singleton INTEGER PRIMARY KEY CHECK(singleton=1), revision INTEGER NOT NULL, display_name TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT NOT NULL)');
  this.sql.exec('CREATE TABLE IF NOT EXISTS careers (id TEXT PRIMARY KEY, slot INTEGER NOT NULL UNIQUE, request_id TEXT NOT NULL UNIQUE, name TEXT NOT NULL, player TEXT NOT NULL, progression TEXT NOT NULL, revision INTEGER NOT NULL, created_at TEXT NOT NULL, updated_at TEXT NOT NULL)');
  this.sql.exec('CREATE TABLE IF NOT EXISTS career_saves (career_id TEXT NOT NULL, revision INTEGER NOT NULL, bytes INTEGER NOT NULL, sha256 TEXT NOT NULL, progression TEXT NOT NULL, created_at TEXT NOT NULL, PRIMARY KEY(career_id,revision))');
  this.sql.exec('CREATE TABLE IF NOT EXISTS career_save_chunks (career_id TEXT NOT NULL, revision INTEGER NOT NULL, part INTEGER NOT NULL, data BLOB NOT NULL, PRIMARY KEY(career_id,revision,part))');
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_profile_revision (singleton INTEGER PRIMARY KEY CHECK(singleton=1), revision INTEGER NOT NULL)');
 }
 rows(query,...args){return this.sql.exec(query,...args).toArray()}
 profile(displayName){
  const name=text(displayName,128),at=new Date().toISOString();
  this.sql.exec('INSERT INTO player_profile VALUES (1,0,?,?,?) ON CONFLICT(singleton) DO UPDATE SET display_name=excluded.display_name,updated_at=CASE WHEN player_profile.display_name<>excluded.display_name THEN excluded.updated_at ELSE player_profile.updated_at END,revision=player_profile.revision+CASE WHEN player_profile.display_name<>excluded.display_name THEN 1 ELSE 0 END',name,at,at);
  const p=this.rows('SELECT revision,display_name AS displayName,created_at AS createdAt,updated_at AS updatedAt FROM player_profile WHERE singleton=1')[0];
  const data=this.rows('SELECT revision,bytes,sha256 FROM career_saves WHERE career_id=? ORDER BY revision DESC LIMIT 1','__profile__')[0]||null;
  return {...p,data,gameFormatVerified:false};
 }
 career(id){uuid(id);const row=this.rows('SELECT * FROM careers WHERE id=?',id)[0];if(!row)fail(404,'career_not_found');return row}
 view(row){return {id:row.id,slot:row.slot,name:row.name,player:JSON.parse(row.player),progression:JSON.parse(row.progression),progressionSource:'client-reported',revision:row.revision,createdAt:row.created_at,updatedAt:row.updated_at,gameFormatVerified:false}}
 list(){return this.rows('SELECT * FROM careers ORDER BY slot').map(row=>this.view(row))}
 create(input){
  if(!input||typeof input!=='object'||Array.isArray(input))fail(400,'invalid_object');
  const requestId=uuid(input?.requestId),name=text(input?.name,80),player=object(input?.player||{});
  if(!Number.isInteger(input.slot)||input.slot<1||input.slot>10)fail(400,'invalid_slot');
  return this.storage.transactionSync(()=>{
   const prior=this.rows('SELECT * FROM careers WHERE request_id=?',requestId)[0];
   if(prior){if(prior.slot!==input.slot||prior.name!==name||prior.player!==player)fail(409,'request_id_conflict');return this.view(prior)}
   if(this.rows('SELECT id FROM careers WHERE slot=?',input.slot).length)fail(409,'slot_occupied');
   const id=crypto.randomUUID(),at=new Date().toISOString();
   this.sql.exec('INSERT INTO careers VALUES (?,?,?,?,?,?,0,?,?)',id,input.slot,requestId,name,player,'{}',at,at);
   return this.view(this.career(id));
  });
 }
 updateProgress(id,input){
  const base=revision(input?.baseRevision),progression=object(input?.progression);
  return this.storage.transactionSync(()=>{
   const row=this.career(id);if(row.revision!==base)fail(409,'revision_conflict');
   this.sql.exec('UPDATE careers SET progression=?,revision=revision+1,updated_at=? WHERE id=?',progression,new Date().toISOString(),id);
   return this.view(this.career(id));
  });
 }
 async save(id,base,bytes){
  uuid(id);revision(base);bytes=new Uint8Array(bytes);
  if(!bytes.length||bytes.length>MAX_SAVE)fail(413,'invalid_save_size');
  const sha256=await digest(bytes);
  return this.storage.transactionSync(()=>this.write(id,base,bytes,sha256));
 }
 async saveProfile(base,bytes,{native=false}={}){
  revision(base);bytes=new Uint8Array(bytes);if(!bytes.length||bytes.length>MAX_SAVE)fail(413,'invalid_save_size');
  const sha256=await digest(bytes);
  return this.storage.transactionSync(()=>{
   const p=this.rows('SELECT revision FROM player_profile WHERE singleton=1')[0];
   if(!p)fail(404,'profile_not_found');if(p.revision!==base)fail(409,'revision_conflict');
   const next=base+1,at=new Date().toISOString(),id='__profile__';
   this.sql.exec('INSERT INTO career_saves VALUES (?,?,?,?,?,?)',id,next,bytes.length,sha256,'{}',at);
   for(let offset=0,part=0;offset<bytes.length;offset+=CHUNK,part++)this.sql.exec('INSERT INTO career_save_chunks VALUES (?,?,?,?)',id,next,part,bytes.slice(offset,offset+CHUNK));
   this.sql.exec('UPDATE player_profile SET revision=?,updated_at=? WHERE singleton=1',next,at);
   if(native)this.sql.exec('INSERT INTO native_profile_revision VALUES (1,?) ON CONFLICT(singleton) DO UPDATE SET revision=excluded.revision',next);
   for(const old of this.rows('SELECT revision FROM career_saves WHERE career_id=? ORDER BY revision DESC LIMIT -1 OFFSET ?',id,KEEP)){
    this.sql.exec('DELETE FROM career_save_chunks WHERE career_id=? AND revision=?',id,old.revision);this.sql.exec('DELETE FROM career_saves WHERE career_id=? AND revision=?',id,old.revision);
   }
   return {revision:next,bytes:bytes.length,sha256,gameFormatVerified:false};
  });
 }
 async loadProfile(){const {row,bytes}=this.read('__profile__',undefined,false);if(await digest(bytes)!==row.sha256)fail(500,'save_integrity_error');return {revision:row.revision,sha256:row.sha256,bytes}}
 nativeProfileRevision(){return this.rows('SELECT revision FROM native_profile_revision WHERE singleton=1')[0]?.revision??null}
 write(id,base,bytes,sha256,restoredProgress){
  const row=this.career(id);if(row.revision!==base)fail(409,'revision_conflict');
  const next=base+1,at=new Date().toISOString(),progression=restoredProgress??row.progression;
  // A single SQLite transaction covers the manifest, all chunks and progression.
  this.sql.exec('INSERT INTO career_saves VALUES (?,?,?,?,?,?)',id,next,bytes.length,sha256,progression,at);
  for(let offset=0,part=0;offset<bytes.length;offset+=CHUNK,part++)this.sql.exec('INSERT INTO career_save_chunks VALUES (?,?,?,?)',id,next,part,bytes.slice(offset,offset+CHUNK));
  this.sql.exec('UPDATE careers SET revision=?,progression=?,updated_at=? WHERE id=?',next,progression,at,id);
  const stale=this.rows('SELECT revision FROM career_saves WHERE career_id=? ORDER BY revision DESC LIMIT -1 OFFSET ?',id,KEEP);
  for(const old of stale){this.sql.exec('DELETE FROM career_save_chunks WHERE career_id=? AND revision=?',id,old.revision);this.sql.exec('DELETE FROM career_saves WHERE career_id=? AND revision=?',id,old.revision)}
  return {career:this.view(this.career(id)),save:{revision:next,bytes:bytes.length,sha256,createdAt:at}};
 }
 history(id){this.career(id);return this.rows('SELECT revision,bytes,sha256,created_at AS createdAt FROM career_saves WHERE career_id=? ORDER BY revision DESC',id)}
 read(id,selected,requireCareer=true){
  if(requireCareer)this.career(id);if(selected!==undefined)revision(selected);
  const row=selected===undefined?this.rows('SELECT * FROM career_saves WHERE career_id=? ORDER BY revision DESC LIMIT 1',id)[0]:this.rows('SELECT * FROM career_saves WHERE career_id=? AND revision=?',id,selected)[0];
  if(!row)fail(404,'save_not_found');
  const chunks=this.rows('SELECT part,data FROM career_save_chunks WHERE career_id=? AND revision=? ORDER BY part',id,row.revision);
  const bytes=new Uint8Array(row.bytes);let offset=0;
  for(let i=0;i<chunks.length;i++){const c=chunks[i],data=new Uint8Array(c.data);if(c.part!==i||offset+data.length>bytes.length)fail(500,'save_integrity_error');bytes.set(data,offset);offset+=data.length}
  if(offset!==bytes.length)fail(500,'save_integrity_error');return {row,bytes};
 }
 async load(id,selected){const {row,bytes}=this.read(id,selected);if(await digest(bytes)!==row.sha256)fail(500,'save_integrity_error');return {revision:row.revision,sha256:row.sha256,bytes}}
 async restore(id,input){
  const base=revision(input?.baseRevision),selected=revision(input?.revision),loaded=await this.load(id,selected);
  return this.storage.transactionSync(()=>{
   // Re-read metadata under the write transaction; the chosen backup may have
   // been pruned by another device while its checksum was being verified.
   const {row}=this.read(id,selected);return this.write(id,base,loaded.bytes,loaded.sha256,row.progression);
  });
 }
}
