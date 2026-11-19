import {Buffer} from 'node:buffer';
import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';

export const CHECKPOINT_ROUTE='/Session/MyCareer/checkpoint';
const F={quest:0xb4433aa7,context:0x7a81dcb9,name:0x92f4e11e,type:0x4009c831,state:0x4d461167,id:0x770c77d6};
// Native CRC names: START, PROGRESS_UPDATE, COMPLETION, TERMINATION.
const START=0x682d973f,PROGRESS=0x859c1990,COMPLETE=0x33492a41,TERMINATE=0x69bc56ca;
const TYPES=new Set([START,PROGRESS,COMPLETE,TERMINATE]);
const ENVELOPE=new Map([[0x01caaee8,'U64'],[0xda780e45,'U64'],[0x2f524d72,'DATETIME'],[0xd5e5f21d,'U64'],[0x5eeda966,'U64'],[0x1c5741a9,'U64'],[0x34dc552e,'U64'],[0x8fa306b4,'U64'],[0x4f63c5c3,'U64'],[0x66b17888,'BOOL']]);
const fail=(status,code)=>{throw new StoreError(status,code)};
function parse(body){
 const fields=protocol.decodeRequest(body).fields;
 const types=new Map([[F.quest,'STRINGCRC'],[F.context,'STRINGCRC'],[F.name,'STRINGCRC'],[F.type,'STRINGCRC'],[F.state,'STRING'],[F.id,'U64'],...ENVELOPE]);
 if(fields.some(f=>types.get(f.id)!==f.type))fail(400,'unsupported_checkpoint_field');
 const value=id=>fields.find(f=>f.id===id)?.value;
 for(const id of [F.quest,F.context,F.name,F.type])if(value(id)===undefined)fail(400,'checkpoint_field_required');
 if(!TYPES.has(value(F.type)))fail(400,'unsupported_checkpoint_transition');
 const state=value(F.state);
 if(typeof state!=='string'||!state.length||Buffer.byteLength(state)>512*1024)fail(400,'invalid_checkpoint_state');
 const id=value(F.id);
 if(value(F.type)!==START&&(!id||id==='0'))fail(400,'checkpoint_id_required');
 if(id&&BigInt(id)>BigInt(Number.MAX_SAFE_INTEGER))fail(400,'invalid_checkpoint_id');
 // Native common metadata supplies player/build scope. Never use a token or
 // a display name as save identity, and never merge separate player builds.
 if(value(0xd5e5f21d)===undefined||value(0x5eeda966)===undefined)fail(400,'checkpoint_player_scope_required');
 return {quest:value(F.quest),context:value(F.context),name:value(F.name),type:value(F.type),state,id:id?Number(id):null,player:value(0xd5e5f21d),build:value(0x5eeda966)};
}
export class NativeCheckpointStore {
 constructor(storage){
  this.storage=storage;this.sql=storage.sql;
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_checkpoints (id INTEGER PRIMARY KEY AUTOINCREMENT, player TEXT NOT NULL, build TEXT NOT NULL, quest INTEGER NOT NULL, context INTEGER NOT NULL, name INTEGER NOT NULL, kind INTEGER NOT NULL, state TEXT NOT NULL, revision INTEGER NOT NULL, updated_at TEXT NOT NULL)');
  this.sql.exec('CREATE TABLE IF NOT EXISTS native_checkpoint_history (checkpoint_id INTEGER NOT NULL, revision INTEGER NOT NULL, kind INTEGER NOT NULL, state TEXT NOT NULL, updated_at TEXT NOT NULL, PRIMARY KEY(checkpoint_id,revision))');
  const historyColumns=new Set(this.sql.exec('PRAGMA table_info(native_checkpoint_history)').toArray().map(c=>c.name));
  for(const column of ['quest_state','checkpoint_name'])if(!historyColumns.has(column))this.sql.exec('ALTER TABLE native_checkpoint_history ADD COLUMN '+column+' INTEGER');
 }
 handle(body){
  let input;try{input=parse(body)}catch(e){if(e instanceof StoreError)throw e;fail(400,'invalid_checkpoint_packet')}
  const saved=this.storage.transactionSync(()=>{
   const rows=(q,...a)=>this.sql.exec(q,...a).toArray();
   let row=input.id?rows('SELECT * FROM native_checkpoints WHERE id=?',input.id)[0]:rows('SELECT * FROM native_checkpoints WHERE player=? AND build=? AND quest=? AND kind IN (?,?) ORDER BY id DESC LIMIT 1',input.player,input.build,input.quest,START,PROGRESS)[0];
   if(input.id&&!row)fail(404,'checkpoint_not_found');
   // CRC32("QUEST_STATE") = 7a81dcb9; CRC32("CHECKPOINT_NAME") =
   // 92f4e11e. Both describe progress within QUEST_INSTANCE_ID, rather than
   // identifying a different owner/build/quest. Preserve their transitions.
   if(row&&row.player!==input.player)fail(409,'checkpoint_player_conflict');
   if(row&&row.build!==input.build)fail(409,'checkpoint_build_conflict');
   if(row&&row.quest!==input.quest)fail(409,'checkpoint_quest_conflict');
   if(row&&row.kind===input.type&&row.state===input.state&&row.context===input.context&&row.name===input.name)return row;
   // These are client checkpoint reports, not an authoritative quest state
   // machine. City recovery can submit another report for an existing instance
   // after completion/termination. Persist it within the immutable scope above,
   // keeping its previous state in history; do not invent a terminal admission
   // rule or grant rewards based on the reported kind.
   if(!row&&input.type!==START)fail(404,'checkpoint_not_found');
   if(!row&&rows('SELECT COUNT(*) AS n FROM native_checkpoints')[0].n>=256)fail(413,'checkpoint_limit');
   const total=rows('SELECT COALESCE(SUM(length(CAST(state AS BLOB))),0) AS n FROM native_checkpoints')[0].n+rows('SELECT COALESCE(SUM(length(CAST(state AS BLOB))),0) AS n FROM native_checkpoint_history')[0].n;
   if(total+Buffer.byteLength(input.state)>32*1024*1024)fail(413,'checkpoint_storage_limit');
   const at=new Date().toISOString();
   if(row){
    this.sql.exec('INSERT INTO native_checkpoint_history (checkpoint_id,revision,kind,state,updated_at,quest_state,checkpoint_name) VALUES (?,?,?,?,?,?,?)',row.id,row.revision,row.kind,row.state,row.updated_at,row.context,row.name);
    this.sql.exec('UPDATE native_checkpoints SET context=?,name=?,kind=?,state=?,revision=revision+1,updated_at=? WHERE id=?',input.context,input.name,input.type,input.state,at,row.id);
    this.sql.exec('DELETE FROM native_checkpoint_history WHERE checkpoint_id=? AND revision NOT IN (SELECT revision FROM native_checkpoint_history WHERE checkpoint_id=? ORDER BY revision DESC LIMIT 3)',row.id,row.id);
   }else{
    this.sql.exec('INSERT INTO native_checkpoints (player,build,quest,context,name,kind,state,revision,updated_at) VALUES (?,?,?,?,?,?,?,1,?)',input.player,input.build,input.quest,input.context,input.name,input.type,input.state,at);
    row=rows('SELECT * FROM native_checkpoints WHERE id=last_insert_rowid()')[0];
   }
   return rows('SELECT * FROM native_checkpoints WHERE id=?',row.id)[0];
  });
  // Acknowledge only a committed, account-scoped checkpoint. Optional rewards,
  // quest unlocks and global progression fields are not fabricated.
  return {status:200,body:protocol.encodeFields([{id:0xe3920695,type:'STRINGCRC',value:0x504521a8},{id:F.id,type:'U64',value:saved.id}])};
 }
}
