// Convert only the two known session-clock fields. Preserve every other byte,
// including credentials, manifests, heap offsets and unknown descriptors.
function sessionCalendar(body, route) {
 if(!['/Session/login','/Session/update'].includes(route)||body.length%8||body.length>4*1024*1024)return body;
 const slots=[];let terminated=false;
 for(let at=0;at+16<=body.length&&at<4096*16;at+=16){
  const id=body.readUInt32BE(at),tag=body.readUInt32BE(at+4);
  if(!id&&!tag&&body.readBigUInt64BE(at+8)===0n){terminated=true;break;}
  if([0xe9824107,0x329a965f].includes(id)){
   if(tag!==0x55c05a86||slots.some(s=>s.id===id))return body;
   slots.push({id,at:at+8,value:body.readBigInt64BE(at+8)});
  }
 }
 if(!terminated||slots.length!==2)return body;
 // Legacy Worker replies use Unix milliseconds; updated replies are already
 // Julian milliseconds and must never receive the offset a second time.
 if(!slots.every(s=>s.value>=946684800000n&&s.value<4102444800000n))return body;
 const result=Buffer.from(body);
 for(const s of slots)result.writeBigInt64BE(s.value+210866760000000n,s.at);
 return result;
}
module.exports={sessionCalendar};
