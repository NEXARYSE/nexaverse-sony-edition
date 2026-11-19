const assert=require('node:assert/strict'),http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const {createRelay}=require('./nba23-cloud.cjs');
(async()=>{
 const token='a'.repeat(64);let received;
 const upstream=http.createServer(async(req,res)=>{const chunks=[];for await(const c of req)chunks.push(c);received={url:req.url,headers:req.headers,body:Buffer.concat(chunks)};res.writeHead(201,{'Content-Type':'application/octet-stream','Set-Cookie':'never=forward'});res.end(received.body)});
 await new Promise(resolve=>upstream.listen(0,'127.0.0.1',resolve));
 const relay=createRelay({origin:'http://127.0.0.1:'+upstream.address().port,token,allowLocalTest:true});
 const a=await relay.listen(0),base='http://127.0.0.1:'+a.port;
 try{
  const input=Buffer.from([0,255,32,128]);const r=await fetch(base+'/Session/login?q=1',{method:'POST',headers:{Authorization:'Bearer wrong',Cookie:'private=discard','Content-Type':'application/octet-stream'},body:input});
  assert.equal(r.status,201);assert.deepEqual(Buffer.from(await r.arrayBuffer()),input);assert.equal(r.headers.get('set-cookie'),null);
  assert.equal(received.url,'/client/Session/login?q=1');assert.equal(received.headers.authorization,'Bearer '+token);assert.equal(received.headers.cookie,undefined);
  assert.equal((await fetch(base+'/admin/status')).status,404);
  assert.equal((await fetch(base+'/Session/login',{headers:{Origin:'https://other.example'}})).status,403);
  assert.throws(()=>createRelay({origin:'https://example.com',token}));assert.throws(()=>createRelay({origin:'https://nexaverse-nba23-backend.tratreejordant.workers.dev',token:''}));
 }finally{await relay.close();await new Promise(resolve=>upstream.close(resolve))}
 console.log('PASS: cloud bridge binary forwarding, player authentication isolation, browser/admin rejection and fixed upstream validation.');
})().catch(e=>{console.error(e);process.exitCode=1});
