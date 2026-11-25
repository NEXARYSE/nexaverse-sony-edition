const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const {parseHTML}=require('linkedom');
const {create:createPolicy}=require('./discord-required.cjs');
const create=options=>createPolicy({membership:{check:async()=>({status:'member'})},...options});
const linked=id=>({configured:true,linked:true,membership:{status:'member'},user:{id,name:'Test Player',username:'test'}});

test('all game launches and mutations require a linked, selected Discord identity',async()=>{
  let state={configured:true,linked:false,user:null},calls=0;
  const gate=create({identity:{status:async()=>state}}),handlers=new Map();
  const ipc={handle:(name,handler)=>handlers.set(name,handler)};
  for(const channel of ['launch','link','rename','gta6-memory-setup','gta6-backend','steam-connect','wallpaper','future-mutation'])gate.register(ipc,channel,()=>++calls);
  for(const handler of handlers.values())await assert.rejects(handler(null,'game'),/Link your Discord/);
  await assert.rejects(gate.enter(),/Link your Discord/);
  assert.equal(calls,0);
  state=linked('123456789012345678');
  await assert.rejects(handlers.get('launch')(),/Select your Discord profile/);
  await gate.enter();
  assert.equal(await handlers.get('launch')(),1);
  assert.equal(await handlers.get('future-mutation')(),2);
  state=linked('987654321098765432');
  await assert.rejects(handlers.get('launch')(),/Select your Discord profile/);
  await gate.enter();await gate.lock();
  await assert.rejects(handlers.get('launch')(),/Select your Discord profile/);
  await gate.enter();state={linked:false};
  await assert.rejects(handlers.get('launch')(),/Link your Discord/);
  assert.equal(calls,2);
});

test('sign-in, closing and update recovery stay available before linking',async()=>{
  const gate=create({identity:{status:async()=>({linked:false})}}),handlers=new Map();
  const ipc={handle:(name,handler)=>handlers.set(name,handler)};
  for(const channel of ['discord-status','discord-connect','discord-lock','discord-enter','window-action','check-updates','download-update','install-update','open-discord-server']){
    gate.register(ipc,channel,()=>channel);
    assert.equal(await handlers.get(channel)(),channel);
  }
});

test('invalid or unreadable identity fails closed; a renderer cannot submit a user ID to enter',async()=>{
  let state={linked:true,user:{id:'invalid'}};
  const gate=create({identity:{status:async()=>state}});
  await assert.rejects(gate.enter('123456789012345678'),/Link your Discord/);
  state=linked('123456789012345678');
  assert.equal((await gate.enter('987654321098765432')).user.id,state.user.id);
});

test('a cached display profile without OAuth credentials does not count as linked',async()=>{
  let saved={user:{id:'123456789012345678',name:'Cached profile'}};
  const module={exports:{}};
  const context={module,require:name=>{
    if(name==='electron')return {app:{getPath:()=>'/fixture'},shell:{},safeStorage:{decryptString:buffer=>buffer.toString()}};
    if(name==='node:fs/promises')return {readFile:async()=>Buffer.from(JSON.stringify(saved))};
    if(name==='./discord-config.json')return {clientId:'123456789012345678'};
    return require(name);
  }};
  vm.runInNewContext(fs.readFileSync('discord-auth.cjs','utf8'),context);
  assert.equal((await module.exports.status()).linked,false);
  saved.token={access_token:'fixture-oauth-token'};
  assert.equal((await module.exports.status()).linked,true);
  saved.user.id='bad';
  assert.equal((await module.exports.status()).linked,false);
});

function welcomeFixture(initial){
  const html=fs.readFileSync('index.html','utf8');
  const {document,window}=parseHTML(html);
  let state=initial,connect=async()=>({ok:false,error:'Discord authorization cancelled.'}),entered=0,closed=0,onState;
  const powerActions=[],browserCalls=[];
  const intervals=[],frames=[];
  const root=document.querySelector('#nexaWelcome');
  const context2d={setTransform(){},clearRect(){},beginPath(){},arc(){},fill(){}};
  root.querySelector('canvas').getContext=()=>context2d;
  const api={discordStatus:async()=>state,refreshDiscordMembership:async()=>state,openDiscordServer:async()=>{},lockDiscordProfile:async()=>({...state,entered:false}),
    discordLinkStatus:async()=>({pending:true,browserError:'Windows could not open your browser.'}),openDiscordAuthorization:async()=>browserCalls.push('open'),copyDiscordAuthorization:async()=>browserCalls.push('copy'),cancelDiscordLink:async()=>browserCalls.push('cancel'),
    connectDiscord:()=>connect(),enterDiscordProfile:async()=>{entered++;state={...state,entered:true};return state},
    windowAction:async action=>{powerActions.push(action);closed++},onDiscordState:fn=>{onState=fn}};
  window.consoleLauncher=api;window.innerWidth=1280;window.innerHeight=720;
  window.matchMedia=()=>({matches:false,addEventListener(){}});
  const context={window,document,CustomEvent:window.CustomEvent,Date,Math,Map,
    setInterval:fn=>{intervals.push(fn);return intervals.length},clearInterval(){},
    requestAnimationFrame:fn=>{frames.push(fn);return frames.length},cancelAnimationFrame(){}};
  vm.createContext(context);vm.runInContext(fs.readFileSync('discord-welcome.js','utf8'),context);
  return {document,root,frames,intervals,powerActions,browserCalls,settle:()=>new Promise(resolve=>setImmediate(resolve)),
    setState:async next=>{state=next;await onState()},setConnect:fn=>{connect=fn},get entered(){return entered},get closed(){return closed}};
}

test('linked users see their profile at startup and must click it; logout returns to welcome',async()=>{
  const f=welcomeFixture({...linked('123456789012345678'),entered:true});await f.settle();
  assert.equal(f.root.hidden,false);
  assert.equal(f.document.querySelector('main').inert,true);
  assert.equal(f.root.querySelector('[data-welcome-profile]').hidden,false);
  assert.equal(f.root.querySelector('[data-welcome-link-label]').textContent,'Add account');
  assert.match(f.root.textContent,/Welcome Back to Nexa Verse/);
  assert.match(f.root.textContent,/Who's using this launcher\?/);
  assert.equal(f.entered,0);
  f.root.querySelector('[data-welcome-profile]').click();await f.settle();
  assert.equal(f.entered,1);assert.equal(f.root.hidden,true);
  assert.equal(Boolean(f.document.querySelector('main').inert),false);
  await f.setState({configured:true,linked:false,user:null});await f.settle();
  assert.equal(f.root.hidden,false);
  assert.equal(f.root.querySelector('[data-welcome-profile]').hidden,true);
  assert.equal(f.document.querySelector('main').inert,true);
});

test('cancelled authorization stays locked and can retry without dismissing the welcome screen',async()=>{
  const f=welcomeFixture({configured:true,linked:false,user:null});await f.settle();
  const button=f.root.querySelector('[data-welcome-link]');
  button.click();await f.settle();
  assert.equal(f.root.hidden,false);assert.equal(button.disabled,false);
  assert.match(f.root.querySelector('[data-welcome-message]').textContent,/cancelled/);
  f.setConnect(async()=>{await f.setState(linked('123456789012345678'));return {ok:true}});
  button.click();await f.settle();
  assert.equal(f.root.hidden,false);assert.equal(f.entered,0);
  assert.equal(f.root.querySelector('[data-welcome-profile]').hidden,false);
  f.root.querySelector('[data-welcome-quit]').click();await f.settle();assert.equal(f.closed,0);
  assert.equal(f.root.querySelector('[data-welcome-power-menu]').hidden,false);
  f.root.querySelector('[data-welcome-power-action="close"]').click();await f.settle();assert.equal(f.closed,1);
});

test('power menu offers close and restart; Escape dismisses without closing the launcher',async()=>{
  const f=welcomeFixture({linked:false});await f.settle();
  const power=f.root.querySelector('[data-welcome-quit]'),menu=f.root.querySelector('[data-welcome-power-menu]');
  power.click();assert.equal(menu.hidden,false);assert.match(menu.textContent,/Close Launcher/);assert.match(menu.textContent,/Restart Launcher/);
  const escape=new f.document.defaultView.Event('keydown',{bubbles:true,cancelable:true});escape.key='Escape';menu.dispatchEvent(escape);
  assert.equal(menu.hidden,true);assert.equal(f.closed,0);
  power.click();f.root.querySelector('[data-welcome-power-action="restart"]').click();await f.settle();
  assert.deepEqual(f.powerActions,['restart']);assert.equal(menu.hidden,true);
});

test('server timeouts lock the welcome screen until membership is restored and the profile is selected again',async()=>{
  const state=linked('123456789012345678');state.entered=true;
  const f=welcomeFixture(state);await f.settle();
  f.root.querySelector('[data-welcome-profile]').click();await f.settle();assert.equal(f.root.hidden,true);
  await f.setState({...state,entered:false,membership:{status:'timed-out',message:'Your Discord server timeout also applies to NEXAVERSE.',until:'2026-11-25T16:05:00Z'}});await f.settle();
  assert.equal(f.root.hidden,false);assert.equal(f.root.querySelector('[data-welcome-profile]').disabled,true);
  assert.match(f.root.querySelector('[data-welcome-message]').textContent,/timeout/);
  assert.equal(f.root.querySelector('[data-welcome-access]').hidden,false);
  f.root.querySelector('[data-welcome-profile]').click();await f.settle();assert.equal(f.entered,1);
  await f.setState({...state,entered:false});await f.settle();
  assert.equal(f.root.hidden,false);assert.equal(f.root.querySelector('[data-welcome-profile]').disabled,false);
  f.root.querySelector('[data-welcome-profile]').click();await f.settle();assert.equal(f.root.hidden,true);
});

test('welcome uses the linked Discord avatar and falls back if the image cannot load',async()=>{
  const state=linked('123456789012345678');state.user.avatar='https://cdn.discordapp.com/avatars/123456789012345678/fixture.png';
  const f=welcomeFixture(state);await f.settle();
  const image=f.root.querySelector('[data-welcome-avatar] img');assert.equal(image.src,state.user.avatar);
  image.dispatchEvent(new f.document.defaultView.Event('load'));
  assert.equal(f.root.querySelector('.nexa-welcome-initial').hidden,true);
  image.dispatchEvent(new f.document.defaultView.Event('error'));
  assert.equal(f.root.querySelector('[data-welcome-avatar] img'),null);
  assert.equal(f.root.querySelector('.nexa-welcome-initial').hidden,false);
});

test('restart uses normal shutdown and rejects an active game or transfer',()=>{
  const source=fs.readFileSync('main.cjs','utf8');
  const body=source.slice(source.indexOf("handleIpc('window-action'"),source.indexOf("handleIpc('discord-status'"));
  let handler,relaunches=0,quits=0;
  const c={handleIpc:(_,fn)=>{handler=fn},sessions:new Map(),pendingLaunches:0,activeUploads:0,
    app:{relaunch:()=>relaunches++,quit:()=>quits++},require:()=>({status:()=>({status:'idle'})})};
  vm.runInNewContext(body,c);handler(null,'restart');assert.equal(relaunches,1);assert.equal(quits,1);
  c.sessions.set('running',true);assert.throws(()=>handler(null,'restart'),/Close running games/);assert.equal(relaunches,1);
});

test('welcome screen runs particle frames and required files ship in the package',async()=>{
  const f=welcomeFixture({linked:false});await f.settle();
  assert.ok(f.frames.length>0);
  const before=f.frames.length;f.frames[0](1000);assert.ok(f.frames.length>before);
  const pkg=require('./package.json');
  for(const file of ['discord-required.cjs','discord-welcome.js','discord-welcome.css'])assert.ok(pkg.build.files.includes(file));
  const main=fs.readFileSync('main.cjs','utf8');
  assert.ok(!main.includes('ipcMain.handle('),'all handlers must use the central access policy');
  assert.match(main,/handleIpc\('launch'/);
});


test('browser recovery controls appear during linking and support copy, retry and cancel',async()=>{
  const f=welcomeFixture({linked:false});await f.settle();let finish;
  f.setConnect(()=>new Promise(resolve=>{finish=resolve}));
  f.root.querySelector('[data-welcome-link]').click();await f.settle();
  for(const tick of [...f.intervals])await tick();await f.settle();
  const controls=f.root.querySelector('.nexa-welcome-browser');assert.equal(controls.hidden,false);
  const buttons=[...controls.querySelectorAll('button')];assert.equal(buttons.length,3);
  buttons[1].click();await f.settle();assert.match(f.root.querySelector('[data-welcome-message]').textContent,/Paste it into your browser/);
  buttons[0].click();await f.settle();buttons[2].click();await f.settle();assert.deepEqual(f.browserCalls,['copy','open','cancel']);
  finish({ok:false,error:'Discord linking cancelled.'});await f.settle();assert.equal(controls.hidden,true);assert.equal(f.root.hidden,false);
});
