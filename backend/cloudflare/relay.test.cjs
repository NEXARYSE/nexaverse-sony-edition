const assert=require('node:assert/strict'),http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const {createRelay}=require('./relay.cjs');
(async()=>{
 const token='a'.repeat(64);let received;
 const upstream=http.createServer(async(req,res)=>{const chunks=[];for await(const c of req)chunks.push(c);received={url:req.url,headers:req.headers,body:Buffer.concat(chunks)};res.writeHead(201,{'Content-Type':'application/octet-stream','Set-Cookie':'never=forward'});res.end(received.body)});
 await new Promise(resolve=>upstream.listen(0,'127.0.0.1',resolve));
 const relay=createRelay({origin:'http://127.0.0.1:'+upstream.address().port,token,allowLocalTest:true});
 const a=await relay.listen(0),base='http://127.0.0.1:'+a.port;
 try{
  const input=Buffer.from([0,255,32,128]);const r=await fetch(base+'/Session/login?q=1',{method:'POST',headers:{Authorization:'Bearer wrong',Cookie:'private=discard','Content-Type':'application/octet-stream'},body:input});
  assert.equal(r.status,201);assert.deepEqual(Buffer.from(await r.arrayBuffer()),input);assert.equal(r.headers.get('set-cookie'),null);
  assert.equal(received.url,'/Session/login?q=1');assert.equal(received.headers.authorization,'Bearer '+token);assert.equal(received.headers.cookie,undefined);
  assert.equal((await fetch(base+'/admin/status')).status,404);
  assert.equal((await fetch(base+'/Session/login',{headers:{Origin:'https://other.example'}})).status,403);
  assert.throws(()=>createRelay({origin:'https://example.com',token}));assert.throws(()=>createRelay({origin:'https://x.workers.dev',token:'bad'}));
 }finally{await relay.close();await new Promise(resolve=>upstream.close(resolve))}
 if(process.env.TEST_LIVE_WORKER==='1'){
  const deployment=JSON.parse(fs.readFileSync(path.join(__dirname,'deployment.json'))),secret=fs.readFileSync(path.join(__dirname,'.private/research-token'),'utf8').trim();
  const live=createRelay({origin:deployment.origin,token:secret});const a=await live.listen(0);
  try{const r=await fetch('http://127.0.0.1:'+a.port+'/Session/login',{method:'POST',body:Buffer.alloc(16)});assert.equal(r.status,501);assert.equal((await r.json()).mycareerReady,false)}finally{await live.close()}
 }
 console.log('PASS: loopback relay preserves binary responses/status, isolates owner credentials, blocks browser origins/admin routes, validates upstream'+(process.env.TEST_LIVE_WORKER==='1'?' and connects to live Cloudflare.':'.'));
})().catch(e=>{console.error(e);process.exitCode=1});
