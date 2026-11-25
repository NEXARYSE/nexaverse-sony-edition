const {test}=require('node:test');
const assert=require('node:assert/strict');
const {create}=require('./discord-membership.cjs');
const {create:createGate}=require('./discord-required.cjs');
const USER='123456789012345678',GUILD='1563364055543185470';
const response=(status,body)=>({status,ok:status>=200&&status<300,json:async()=>body});
function fixture(){
  let time=Date.parse('2026-11-24T16:00:00Z'),account={linked:true,user:{id:USER}},next=response(200,{user:{id:USER},pending:false,communication_disabled_until:null}),calls=0;
  const forces=[];
  const identity={status:async()=>account,accessToken:async options=>{forces.push(options);return options?.force?'renewed-token':'fixture-token'}};
  const membership=create({identity,guildId:GUILD,now:()=>time,request:async(url,options)=>{
    calls++;assert.equal(url,`https://discord.com/api/v10/users/@me/guilds/${GUILD}/member`);
    assert.match(options.headers.Authorization,/^Bearer (fixture-token|renewed-token)$/);
    if(next instanceof Error)throw next;
    return typeof next==='function'?next():next;
  }});
  return {identity,membership,forces,setResponse:state=>{next=state;time+=6000},setAccount:state=>{account=state},advance:ms=>{time+=ms},get calls(){return calls}};
}

test('membership is checked against the configured server and linked account; polls coalesce',async()=>{
  const f=fixture();
  assert.equal((await f.membership.check(USER)).status,'member');
  await f.membership.check(USER);assert.equal(f.calls,1);
  await f.membership.check(USER,{force:true});assert.equal(f.calls,1);
  f.advance(5001);await f.membership.check(USER,{force:true});assert.equal(f.calls,2);
  f.advance(30001);await f.membership.check(USER);assert.equal(f.calls,3);
  f.setResponse(response(200,{user:{id:'987654321098765432'}}));
  assert.equal((await f.membership.check(USER,{force:true})).status,'unavailable');
});

test('banned/non-member, pending rules and timed-out accounts are denied',async()=>{
  const f=fixture();
  f.setResponse(response(404,{}));assert.equal((await f.membership.check(USER)).status,'not-member');
  f.setResponse(response(200,{user:{id:USER},pending:true}));assert.equal((await f.membership.check(USER,{force:true})).status,'pending');
  const until='2026-11-24T16:05:00.000Z';
  f.setResponse(response(200,{user:{id:USER},communication_disabled_until:until}));
  const denied=await f.membership.check(USER,{force:true});assert.equal(denied.status,'timed-out');assert.equal(denied.until,until);
  f.advance(300001);assert.equal((await f.membership.check(USER,{force:true})).status,'member');
});

test('transient failures retain a recent verification for at most five minutes; missing scope fails closed',async()=>{
  const f=fixture();await f.membership.check(USER);
  f.setResponse(new Error('offline'));const grace=await f.membership.check(USER,{force:true});assert.equal(grace.status,'member');assert.equal(grace.verificationDeferred,true);
  f.advance(300001);assert.equal((await f.membership.check(USER,{force:true})).status,'unavailable');
  f.advance(31000);
  f.setResponse(response(403,{}));assert.equal((await f.membership.check(USER,{force:true})).status,'reconnect');
  f.setResponse(response(429,{retry_after:30}));await f.membership.check(USER,{force:true});
  const before=f.calls;assert.equal((await f.membership.check(USER,{force:true})).status,'unavailable');assert.equal(f.calls,before);
  f.advance(31000);f.setResponse(response(200,{user:{id:USER}}));assert.equal((await f.membership.check(USER,{force:true})).status,'member');
});

test('401 refreshes the OAuth token once and revalidates the member identity',async()=>{
  const f=fixture();let attempts=0;
  f.setResponse(()=>++attempts===1?response(401,{}):response(200,{user:{id:USER}}));
  assert.equal((await f.membership.check(USER)).status,'member');assert.equal(f.calls,2);
  assert.equal(f.forces[1].force,true);assert.equal(f.forces[1].previousToken,'fixture-token');
});

test('a ban or timeout revokes a selected profile and blocks a direct launch IPC call',async()=>{
  const f=fixture(),gate=createGate({identity:f.identity,membership:f.membership}),handlers=new Map();let launches=0;
  gate.register({handle:(name,handler)=>handlers.set(name,handler)},'launch',()=>++launches);
  await gate.enter();await handlers.get('launch')();assert.equal(launches,1);
  f.setResponse(response(404,{}));await assert.rejects(handlers.get('launch')(),/Join the NEXAVERSE Discord/);assert.equal(launches,1);
  f.setResponse(response(200,{user:{id:USER},communication_disabled_until:'2026-11-24T16:05:00Z'}));
  await assert.rejects(gate.enter(),/timeout/);
  f.advance(300001);await gate.enter();await handlers.get('launch')();assert.equal(launches,2);
});

test('membership verification is mandatory even when the provider is missing',async()=>{
  const f=fixture(),gate=createGate({identity:f.identity});
  await assert.rejects(gate.enter(),/Could not verify/);
});

test('background checks notify the renderer when a selected account is banned or timed out',async()=>{
  const f=fixture(),notices=[],gate=createGate({identity:f.identity,membership:f.membership,notify:state=>notices.push(state)});
  await gate.enter();notices.length=0;
  f.setResponse(response(200,{user:{id:USER},communication_disabled_until:'2026-11-24T16:05:00Z'}));
  await gate.refreshAccess();assert.equal(notices.length,1);assert.equal(notices[0].entered,false);assert.equal(notices[0].membership.status,'timed-out');
  await gate.refreshAccess();assert.equal(notices.length,1,'unchanged checks should not repeatedly notify');
  f.setResponse(response(404,{}));await gate.refreshAccess();assert.equal(notices.at(-1).membership.status,'not-member');
});

test('Discord server time prevents a fast PC clock from ending a timeout early',async()=>{
  const f=fixture();f.advance(86400000);
  const timed=response(200,{user:{id:USER},communication_disabled_until:'2026-11-24T16:05:00Z'});
  timed.headers={get:()=> 'Tue, 24 Nov 2026 16:00:00 GMT'};f.setResponse(timed);
  assert.equal((await f.membership.check(USER)).status,'timed-out');
  f.setResponse(response(200,{user:{id:USER},communication_disabled_until:'invalid'}));
  assert.equal((await f.membership.check(USER,{force:true})).status,'unavailable');
});

test('a prepared launch cannot continue after the linked account changes',async()=>{
  const f=fixture(),gate=createGate({identity:f.identity,membership:f.membership});
  await gate.enter();const initial=await gate.requireEntry();
  const other='987654321098765432';f.setAccount({linked:true,user:{id:other}});f.setResponse(response(200,{user:{id:other}}));
  await gate.enter();await assert.rejects(gate.requireEntry({freshMembership:true,expectedUserId:initial.id}),/account changed while preparing/);
});

test('a 277-second header overrides a shorter 37-second body without kicking a verified session',async()=>{
  const f=fixture(),gate=createGate({identity:f.identity,membership:f.membership});
  await gate.enter();
  const limited=response(429,{retry_after:37});
  limited.headers={get:name=>({'retry-after':'277','x-ratelimit-remaining':'0','x-ratelimit-reset-after':'276.738'}[name]??null)};
  f.setResponse(limited);
  let state=await gate.refreshAccess();assert.equal(state.entered,true);assert.equal(state.membership.verificationDeferred,true);
  const calls=f.calls;
  for(let i=0;i<20;i++){f.advance(10000);state=await gate.refreshAccess();assert.equal(state.entered,true);}
  assert.equal(f.calls,calls,'polls and force checks must respect the full cooldown');
  f.advance(78000);f.setResponse(response(200,{user:{id:USER}}));state=await gate.refreshAccess();assert.equal(state.entered,true);assert.equal(state.membership.verificationDeferred,undefined);
});

test('a new or previously denied account cannot gain access through transient-error grace',async()=>{
  const f=fixture();f.setResponse(response(429,{retry_after:37}));assert.equal((await f.membership.check(USER)).status,'unavailable');
  f.advance(38000);f.setResponse(response(200,{user:{id:USER}}));await f.membership.check(USER,{force:true});
  f.setResponse(response(404,{}));assert.equal((await f.membership.check(USER,{force:true})).status,'not-member');
  f.setResponse(new Error('offline'));assert.equal((await f.membership.check(USER,{force:true})).status,'unavailable');
});
