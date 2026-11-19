const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const LIMIT=4*1024*1024;
function createRelay({origin,token,allowLocalTest=false}){
 const remote=new URL(origin);
 const local=allowLocalTest&&remote.protocol==='http:'&&remote.hostname==='127.0.0.1';
 if((!local&&(remote.protocol!=='https:'||!remote.hostname.endsWith('.workers.dev')))||remote.username||remote.password||remote.pathname!=='/'||remote.search||remote.hash)throw Error('Invalid Worker origin');
 if(!/^[a-f0-9]{64}$/.test(token))throw Error('Invalid research token');
 const server=http.createServer(async(req,res)=>{
  const reply=(status,error)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify({error,mycareerReady:false}))};
  try{
   if(!/^(127\.0\.0\.1|localhost):\d+$/.test(req.headers.host||'')||req.headers.origin)return reply(403,'local_access_only');
   if(!req.url.startsWith('/'))return reply(400,'invalid_route');
   const target=new URL(req.url,'http://127.0.0.1');
   if(target.origin!=='http://127.0.0.1'||(target.pathname!=='/health'&&target.pathname!=='/Session'&&!/^\/Session\/[A-Za-z0-9_/-]{1,120}$/.test(target.pathname)))return reply(404,'unknown_route');
   if(!['GET','POST'].includes(req.method))return reply(405,'unsupported_method');
   if(Number(req.headers['content-length'])>LIMIT){req.resume();return reply(413,'request_too_large')}
   const chunks=[];let bytes=0;
   for await(const chunk of req){bytes+=chunk.length;if(bytes>LIMIT){reply(413,'request_too_large');req.destroy();return}chunks.push(chunk)}
   const headers={authorization:'Bearer '+token};
   for(const key of ['content-type','accept','vcfieldlist_size','vc-request-id'])if(typeof req.headers[key]==='string')headers[key]=req.headers[key];
   const result=await fetch(new URL(target.pathname+target.search,remote),{method:req.method,headers,body:req.method==='POST'?Buffer.concat(chunks):undefined,redirect:'manual',signal:AbortSignal.timeout(2500)});
   const output=[];let size=0;
   for await(const chunk of result.body||[]){size+=chunk.length;if(size>LIMIT)throw Error('response_too_large');output.push(chunk)}
   const body=Buffer.concat(output),outHeaders={'Cache-Control':'no-store','Content-Length':body.length};
   for(const key of ['content-type','vcfieldlist_size','vc-request-id'])if(result.headers.has(key))outHeaders[key]=result.headers.get(key);
   res.writeHead(result.status,outHeaders);res.end(body);
  }catch{if(!res.headersSent)reply(502,'worker_unavailable');else res.destroy()}
 });
 server.requestTimeout=5000;server.headersTimeout=4000;server.maxHeadersCount=64;
 return {server,listen:port=>new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',()=>resolve(server.address()))}),close:()=>new Promise(resolve=>{server.close(resolve);server.closeAllConnections()})};
}
if(require.main===module){
 const deployment=JSON.parse(fs.readFileSync(path.join(__dirname,'deployment.json'),'utf8'));
 const token=fs.readFileSync(path.join(__dirname,'.private/research-token'),'utf8').trim();
 const relay=createRelay({origin:deployment.origin,token});
 relay.listen(32122).then(()=>console.log('Private Cloudflare research relay listening on 127.0.0.1:32122; MyCAREER unavailable.')).catch(()=>{console.error('Research relay could not bind port 32122');process.exitCode=1});
}
module.exports={createRelay};
