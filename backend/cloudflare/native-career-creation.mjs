import {Buffer} from 'node:buffer';
import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';
export const CAREER_CREATE_ROUTE='/Session/MyCareer/create';
export const CAREER_ATTRIBUTES_ROUTE='/Session/MyCareer/attributes';
const ID=0xd5e5f21d,SLOT=0x5eeda966;
const SELECTORS=[0x801f78b9,0xeabd3195,0xb70b12cf,0x76d988a5,0x4f54b460];
const BODY=[0x87258423,0xf73592c7,0x1263ffd0,0xd466a799];
const ATTRIBUTE=0xbeb8f114,INITIAL=0x96bc2b19,CAP=0x2947bfd5,ARRAYS=new Set([ATTRIBUTE,INITIAL,CAP]);
const types=new Map([...SELECTORS.map(id=>[id,'STRINGCRC']),...BODY.map(id=>[id,'U64']),[ATTRIBUTE,'STRINGCRC'],[INITIAL,'U64'],[CAP,'U64'],[ID,'U64'],[SLOT,'U64'],...[0x1c5741a9,0x34dc552e,0x8fa306b4,0x4f63c5c3,0x01caaee8,0xda780e45].map(id=>[id,'U64']),[0x66b17888,'BOOL'],[0xdcbe6cfd,'STRINGCRC'],[0xa0e40c36,'BOOL']]);
const fail=(status,code)=>{throw new StoreError(status,code)};
const hash=async bytes=>[...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(v=>v.toString(16).padStart(2,'0')).join('');
export class NativeCareerCreation {
 constructor(storage){this.storage=storage;this.sql=storage.sql;this.sql.exec('CREATE TABLE IF NOT EXISTS native_career_builds (id INTEGER PRIMARY KEY AUTOINCREMENT,slot TEXT NOT NULL UNIQUE,fingerprint TEXT NOT NULL,fields TEXT NOT NULL,created_at TEXT NOT NULL)');this.sql.exec('CREATE TABLE IF NOT EXISTS native_career_values (career_id INTEGER PRIMARY KEY, values_json TEXT NOT NULL)')}
 responseFor(row){
  const selected=JSON.parse(row.fields);
  const current=this.storage.transactionSync(()=>{
   const prior=this.sql.exec('SELECT values_json FROM native_career_values WHERE career_id=?',row.id).toArray()[0];
   if(prior)return JSON.parse(prior.values_json);
   // Backfill only an unplayed creation snapshot. An uploaded opaque save may
   // contain newer progression and must never be reset to its initial ratings.
   if(this.sql.exec('SELECT COUNT(*) AS n FROM native_career_saves').one().n)fail(409,'career_progression_import_required');
   const initial=selected.filter(f=>f.id===INITIAL).map(f=>f.value);
   if(initial.length!==22)fail(409,'career_attributes_incomplete');
   this.sql.exec('INSERT INTO native_career_values VALUES (?,?)',row.id,JSON.stringify(initial));
   return initial;
  });
  return {status:200,body:protocol.encodeFields([{id:0xe3920695,type:'STRINGCRC',value:0x504521a8},...selected,...current.map(value=>({id:0x9a0efa17,type:'U64',value})),{id:ID,type:'U64',value:row.id}])};
 }
 attributes(metadata){
  let fields;try{fields=protocol.decodeRequest(metadata).fields}catch{fail(400,'invalid_career_attributes_packet')}
  const envelope=new Map([[ID,'U64'],[SLOT,'U64'],...[0x1c5741a9,0x34dc552e,0x8fa306b4,0x4f63c5c3,0x01caaee8,0xda780e45].map(id=>[id,'U64']),[0x66b17888,'BOOL']]);
  if(fields.some(f=>envelope.get(f.id)!==f.type))fail(400,'unsupported_career_attributes_field');
  const value=id=>fields.find(f=>f.id===id)?.value;
  const id=value(ID);if(id===undefined||id==='0')fail(400,'career_id_required');
  // Query only this authenticated account's committed build. No fallback to a
  // different slot or account, and no synthetic progression/rewards.
  const row=this.sql.exec('SELECT * FROM native_career_builds WHERE id=?',id).toArray()[0];
  if(!row||value(SLOT)!==undefined&&value(SLOT)!==row.slot)fail(404,'native_career_not_found');
  return this.responseFor(row);
 }
 async create(metadata){
  let fields;try{fields=protocol.decodeRequest(metadata,{allowRepeated:true}).fields}catch{fail(400,'invalid_career_creation_packet')}
  const seen=new Set();
  for(const f of fields){if(types.get(f.id)!==f.type||seen.has(f.id)&&!ARRAYS.has(f.id))fail(400,'unsupported_career_creation_field');seen.add(f.id)}
  const value=id=>fields.find(f=>f.id===id)?.value;
  for(const id of [...SELECTORS,...BODY,SLOT])if(value(id)===undefined)fail(400,'career_creation_field_required');
  const slot=value(SLOT);if(BigInt(slot)>10n)fail(400,'invalid_career_slot');
  const attrs=fields.filter(f=>f.id===ATTRIBUTE),initial=fields.filter(f=>f.id===INITIAL),caps=fields.filter(f=>f.id===CAP);
  if(attrs.length!==22||initial.length!==attrs.length||caps.length!==attrs.length||new Set(attrs.map(f=>f.value)).size!==attrs.length)fail(400,'invalid_career_attribute_arrays');
  for(let i=0;i<attrs.length;i++)if(BigInt(initial[i].value)>BigInt(caps[i].value)||BigInt(caps[i].value)>255n)fail(400,'invalid_career_attribute_bounds');
  // Only fields read by the native creation callback/build state are echoed.
  // Account/session credentials and unrelated common metadata are excluded.
  const selected=new Set([...SELECTORS,...BODY,ATTRIBUTE,INITIAL,CAP,SLOT,0xdcbe6cfd,0xa0e40c36]);
  const response=fields.filter(f=>selected.has(f.id)).map(({id,type,value})=>({id,type,value}));
  response.sort((a,b)=>a.id-b.id);
  const canonical=JSON.stringify(response),fingerprint=await hash(Buffer.from(canonical));
  const row=this.storage.transactionSync(()=>{
   const prior=this.sql.exec('SELECT * FROM native_career_builds WHERE slot=?',slot).toArray()[0];
   if(prior){if(prior.fingerprint!==fingerprint)fail(409,'native_career_slot_conflict');return prior}
   if(this.sql.exec('SELECT COUNT(*) AS n FROM native_career_builds').one().n>=10)fail(413,'native_career_slot_limit');
   this.sql.exec('INSERT INTO native_career_builds(slot,fingerprint,fields,created_at) VALUES (?,?,?,?)',slot,fingerprint,canonical,new Date().toISOString());
   const created=this.sql.exec('SELECT * FROM native_career_builds WHERE id=last_insert_rowid()').one();
   this.sql.exec('INSERT INTO native_career_values VALUES (?,?)',created.id,JSON.stringify(response.filter(f=>f.id===INITIAL).map(f=>f.value)));
   // A pre-creation checkpoint has no career ID yet. Bind only that account's
   // unassigned START records for this exact slot after creation commits.
   this.sql.exec('UPDATE native_checkpoints SET player=? WHERE player=? AND build=? AND kind=?',String(created.id),'0',slot,0x682d973f);
   return created;
  });
  return this.responseFor(row);
 }
}
