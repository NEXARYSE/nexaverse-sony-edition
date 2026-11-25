const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),{EventEmitter}=require('node:events');
function fixture({rejectBrowser=false,portError=false}={}){
  let callback,server,timeout,stored=null,copied='',opened=[],requests=[];
  const api={exports:{}};
  const electron={app:{getPath:()=>'/private'},safeStorage:{isEncryptionAvailable:()=>true,encryptString:s=>Buffer.from(s),decryptString:b=>b.toString()},clipboard:{writeText:s=>{copied=s}},shell:{openExternal:async url=>{opened.push(url);if(rejectBrowser)throw Error('No browser handler')}}};
  const http={createServer:fn=>{callback=fn;server=new EventEmitter();server.close=()=>{};server.listen=(_port,_host,fn)=>queueMicrotask(()=>portError?server.emit('error',Error('busy')):fn());return server}};
  vm.runInNewContext(fs.readFileSync(__dirname+'/discord-auth.cjs','utf8'),{module:api,require:name=>name==='electron'?electron:name==='node:http'?http:name==='./discord-config.json'?{clientId:'1570039716093632684'}:name==='node:fs/promises'?{readFile:async()=>{if(!stored)throw Error('missing');return stored},mkdir:async()=>{},writeFile:async(_p,b)=>{stored=b},unlink:async()=>{stored=null}}:require(name),URL,URLSearchParams,Buffer,AbortSignal,setTimeout:fn=>{timeout=fn;return 1},clearTimeout:()=>{},fetch:async(url,options)=>{requests.push({url,options});return {ok:true,json:async()=>url.endsWith('/token')?{access_token:'private-access',refresh_token:'private-refresh',expires_in:3600}:{id:'123456789012345678',username:'tester'}}}});
  return {auth:api.exports,get opened(){return opened},get copied(){return copied},get stored(){return stored},get requests(){return requests},expire:()=>timeout(),async ready(){await new Promise(setImmediate)},async callback(query){let status=200,body='';await callback({method:'GET',url:'/discord/callback?'+query},{setHeader(){},writeHead:n=>{status=n},end:s=>{body=s}});return {status,body}}};
}
test('failed browser opening keeps PKCE callback alive for copy and manual authorization',async()=>{
  const f=fixture({rejectBrowser:true});const result=f.auth.connect();await f.ready();
  assert.equal(f.auth.linkStatus().pending,true);assert.match(f.auth.linkStatus().browserError,/Copy/);
  f.auth.copyAuthorization();const url=new URL(f.copied);assert.equal(url.hostname,'discord.com');assert.equal(url.searchParams.get('code_challenge_method'),'S256');assert(!f.copied.includes('private-access'));
  assert.equal((await f.callback('state=wrong&code=invalid')).status,400);assert.equal(f.requests.length,0);
  await f.callback(new URLSearchParams({state:url.searchParams.get('state'),code:'authorized-code'}));
  assert.equal((await result).ok,true);assert.equal(f.auth.linkStatus().pending,false);assert(f.stored);
  assert.equal(f.requests.length,2);const exchange=f.requests[0].options.body;assert.equal(exchange.get('redirect_uri'),url.searchParams.get('redirect_uri'));assert(exchange.get('code_verifier'));assert(!exchange.has('client_secret'));
  assert.throws(()=>f.auth.copyAuthorization(),/Start Discord/);
});
test('browser retry reuses the active state and cancel prevents late authorization',async()=>{
  const f=fixture();const result=f.auth.connect();await f.ready();await f.auth.openAuthorization();assert.equal(f.opened.length,2);assert.equal(f.opened[0],f.opened[1]);
  const state=new URL(f.opened[0]).searchParams.get('state');f.auth.cancelLink();assert.equal((await result).ok,false);assert.equal(f.auth.linkStatus().pending,false);
  assert.equal((await f.callback(new URLSearchParams({state,code:'late'}))).status,400);assert.equal(f.requests.length,0);
});
test('port conflict resolves promptly and never opens an unusable authorization URL',async()=>{
  const f=fixture({portError:true});const result=await f.auth.connect();assert.equal(result.ok,false);assert.match(result.error,/port/);assert.equal(f.opened.length,0);assert.equal(f.auth.linkStatus().pending,false);
});
test('timeout clears the copy link and allows a fresh attempt',async()=>{
  const f=fixture();const first=f.auth.connect();await f.ready();f.expire();assert.equal((await first).ok,false);assert.equal(f.auth.linkStatus().pending,false);
  const second=f.auth.connect();await f.ready();assert.equal(f.auth.linkStatus().pending,true);assert.notEqual(f.opened[0],f.opened[1]);f.auth.cancelLink();await second;
});
test('browser recovery IPC remains available before Discord entry',async()=>{
  const gate=require('./discord-required.cjs').create({identity:{status:async()=>({linked:false})}});const handlers=new Map();
  for(const name of ['discord-link-status','discord-link-open','discord-link-copy','discord-link-cancel']){gate.register({handle:(n,fn)=>handlers.set(n,fn)},name,()=>name);assert.equal(await handlers.get(name)(),name);}
});
