import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';

export const CITY_ELECTION_ROUTE='/Session/City/election';
// PPSA03669 builder 0x72ffa0 requests 0x612f6478. On a missing service,
// 0x73026d enters the generic server-unavailable dialog. Its callback at
// 0x730c40 parses two JSON strings, then completes when candidates is empty.
// Our private network has no configured elections, histories or winners.
export function cityElection(metadata){
 let fields;try{fields=protocol.decodeRequest(metadata).fields}catch{throw new StoreError(400,'invalid_city_election_packet')}
 const allowed=new Map([[0x45c6fcb2,'STRINGCRC'],[0x01caaee8,'U64'],[0xda780e45,'U64']]);
 if(fields.some(f=>allowed.get(f.id)!==f.type)||!fields.some(f=>f.id===0x45c6fcb2))throw new StoreError(400,'invalid_city_election_fields');
 return {status:200,body:protocol.encodeFields([
  {id:0xe3920695,type:'STRINGCRC',value:0x504521a8},
  {id:0x9b205d17,type:'STRING',value:'[]'},
  {id:0x1a93a44c,type:'STRING',value:'{"candidates":[]}'},
 ])};
}
