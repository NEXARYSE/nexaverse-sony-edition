const {test}=require('node:test'),assert=require('node:assert/strict');
const {sessionCalendar}=require('./nba23-calendar.cjs');
const {encodeFields,decodeRequest}=require('./backend23/protocol.cjs');
test('2K23 session calendar converts legacy clocks once without changing other bytes',()=>{
 const now=Date.UTC(2026,10,17),fields=[{id:0xda780e45,type:'U64',value:'123456'},{id:0xe9824107,type:'DATETIME',value:now},{id:0x329a965f,type:'DATETIME',value:now},{id:0xb5017d25,type:'BINARY',value:Buffer.from('manifest fixture')}];
 const body=encodeFields(fields),fixed=sessionCalendar(body,'/Session/login');assert.notDeepEqual(fixed,body);assert.equal(fixed.length,body.length);
 const parsed=decodeRequest(fixed).fields;assert.equal(parsed[1].value,(BigInt(now)+210866760000000n).toString());assert.equal(parsed[0].value,'123456');assert.deepEqual(parsed[3].value,fields[3].value);
 assert.deepEqual(sessionCalendar(fixed,'/Session/update'),fixed);assert.strictEqual(sessionCalendar(body,'/Session/MyCareer/upload'),body);
 const expected=Buffer.from(fixed);body.copy(expected,24,24,32);body.copy(expected,40,40,48);assert.deepEqual(expected,body);
 assert.strictEqual(sessionCalendar(Buffer.alloc(17),'/Session/login').length,17);
 assert.strictEqual(sessionCalendar(encodeFields(fields.slice(0,2)),'/Session/login').length,encodeFields(fields.slice(0,2)).length);
});
test('authenticated relay returns the corrected clock to the native session',async()=>{
 const http=require('node:http'),{createRelay}=require('./nba23-cloud.cjs');
 const now=Date.UTC(2026,10,17),packet=encodeFields([{id:0xe9824107,type:'DATETIME',value:now},{id:0x329a965f,type:'DATETIME',value:now}]);
 const upstream=http.createServer((req,res)=>{req.resume();res.writeHead(200,{'Content-Type':'application/octet-stream'});res.end(packet)});
 await new Promise(resolve=>upstream.listen(0,'127.0.0.1',resolve));
 const relay=createRelay({origin:'http://127.0.0.1:'+upstream.address().port,token:'fixture',allowLocalTest:true});
 try{
  const address=await relay.listen(0),r=await fetch('http://127.0.0.1:'+address.port+'/Session/update?nx_session=fixture',{method:'POST',body:Buffer.alloc(16)});
  assert.equal(r.status,200);const bytes=Buffer.from(await r.arrayBuffer());assert.equal(bytes.length,packet.length);assert.equal(decodeRequest(bytes).fields[0].value,(BigInt(now)+210866760000000n).toString());
 }finally{await relay.close();await new Promise(resolve=>upstream.close(resolve))}
});
