const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const {sessionCalendar}=require('./nba23-calendar.cjs');
const LIMIT=4*1024*1024;
function createRelay({origin="https://nexaverse-nba23-backend.tratreejordant.workers.dev",token,allowLocalTest=false,onTiming}){
 const remote=new URL(origin);
 const local=allowLocalTest&&remote.protocol==='http:'&&remote.hostname==='127.0.0.1';
 if((!local&&(remote.protocol!=='https:'||remote.hostname!=='nexaverse-nba23-backend.tratreejordant.workers.dev'))||remote.username||remote.password||remote.pathname!=='/'||remote.search||remote.hash)throw Error('Invalid Worker origin');
 if(typeof token!=='string'||!token.length||token.length>2048||/[\r\n]/.test(token))throw Error('Invalid Discord token');
 const server=http.createServer(async(req,res)=>{
  const reply=(status,error)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify({error,mycareerReady:false}))};
  try{
   if(req.headers['sec-fetch-site'])return reply(403,'local_access_only');
   if(!/^(127\.0\.0\.1|localhost):\d+$/.test(req.headers.host||'')||req.headers.origin)return reply(403,'local_access_only');
   if(!req.url.startsWith('/'))return reply(400,'invalid_route');
   const target=new URL(req.url,'http://127.0.0.1');
   if(target.origin!=='http://127.0.0.1'||(target.pathname!=='/health'&&target.pathname!=='/Session'&&!/^\/Session\/[A-Za-z0-9_/-]{1,120}$/.test(target.pathname)))return reply(404,'unknown_route');
   if(!['GET','POST'].includes(req.method))return reply(405,'unsupported_method');
   if(typeof onTiming==='function'){
    const start=performance.now();
    res.once('finish',()=>{try{onTiming({route:target.pathname,status:res.statusCode,elapsedMs:Math.round(performance.now()-start)})}catch{}});
   }
   if(Number(req.headers['content-length'])>LIMIT){req.resume();return reply(413,'request_too_large')}
   const chunks=[];let bytes=0;
   for await(const chunk of req){bytes+=chunk.length;if(bytes>LIMIT){reply(413,'request_too_large');req.destroy();return}chunks.push(chunk)}
   const headers={authorization:'Bearer '+token};
   for(const key of ['content-type','accept','vcfieldlist_size','vc-request-id'])if(typeof req.headers[key]==='string')headers[key]=req.headers[key];
   const result=await fetch(new URL('/client'+target.pathname+target.search,remote),{method:req.method,headers,body:req.method==='POST'?Buffer.concat(chunks):undefined,redirect:'manual',signal:AbortSignal.timeout(10000)});
   const output=[];let size=0;
   for await(const chunk of result.body||[]){size+=chunk.length;if(size>LIMIT)throw Error('response_too_large');output.push(chunk)}
   const raw=Buffer.concat(output),body=result.status===200?sessionCalendar(raw,target.pathname):raw,outHeaders={'Cache-Control':'no-store','Content-Length':body.length};
   for(const key of ['content-type','vcfieldlist_size','vc-request-id'])if(result.headers.has(key))outHeaders[key]=result.headers.get(key);
   res.writeHead(result.status,outHeaders);res.end(body);
  }catch{if(!res.headersSent)reply(502,'worker_unavailable');else res.destroy()}
 });
 server.requestTimeout=15000;server.headersTimeout=4000;server.maxHeadersCount=64;
 return {server,listen:port=>new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',()=>resolve(server.address()))}),close:()=>new Promise(resolve=>{server.close(resolve);server.closeAllConnections()})};
}
module.exports={createRelay};
