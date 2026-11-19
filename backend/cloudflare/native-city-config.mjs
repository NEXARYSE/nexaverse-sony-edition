import protocol from '../protocol.cjs';
import {StoreError} from './career-store.mjs';

export const CITY_CONFIG_ROUTE='/Session/City/config';
// PPSA03669 0x1e0c6b0 requests service 0xc39a9528 with a player ID and UTF-16 TAG CATEGORY:BALL_STREET.
// An absent service enters the blocking dialog at 0x1e0c7d6. The response
// visitor at 0x1e0d870 checks RESULT, then optionally reads compressed data
// (0x603bd442) and its decoded size (0x5ab6fdf4) through 0x941c50.
// TAG is an opaque native lookup key, not an uppercase CATEGORY enum. The
// client also constructs other tags dynamically (observed as 48-byte UTF-16).
// Player ID zero can request shared configuration; authentication is checked
// by the session layer before this read-only optional override lookup.
// This private network has no supplemental catalog content.
// Omit the optional pair together; never supply an empty/malformed compressed
// stream, invented progression, or a successful City-session allocation.
export function cityConfig(metadata){
 let fields;try{fields=protocol.decodeRequest(metadata).fields}catch{throw new StoreError(400,'invalid_city_config_packet')}
 const types=new Map([[0xd5e5f21d,'U64'],[0x952ef509,'WSTRING'],[0x01caaee8,'U64'],[0xda780e45,'U64']]);
 const value=id=>fields.find(f=>f.id===id)?.value;
 const tag=value(0x952ef509);
 if(fields.some(f=>types.get(f.id)!==f.type))throw new StoreError(400,'city_config_descriptor');
 if(value(0xd5e5f21d)===undefined)throw new StoreError(400,'city_config_player_required');
 if(typeof tag!=='string')throw new StoreError(400,'city_config_tag_required');
 if(!tag.trim().length)throw new StoreError(400,'city_config_tag_blank');
 if(tag.length>128)throw new StoreError(400,'city_config_tag_limit');
 if(/[\u0000-\u001f\u007f]/u.test(tag))throw new StoreError(400,'city_config_tag_control');
 return {status:200,body:protocol.encodeFields([{id:0xe3920695,type:'STRINGCRC',value:0x504521a8}])};
}
