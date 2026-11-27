const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const {parseHTML}=require('C:/Users/tratr/Documents/Codex/2026-11-09/i-x20/work/nexaverse-console/node_modules/linkedom');
const dir=__dirname;
test('GTA VI IPC validates, persists, and blocks settings changes during launch or play',async()=>{
 const handlers={},games=[{id:'gta-vi'}],sessions=new Set(),launchingGames=new Set();let writes=0,notices=0,race=false;
 const context={handleIpc:(name,fn)=>handlers[name]=fn,gta6Module:require(dir+'/gta6-runtime.cjs'),sessions,launchingGames,edit:async fn=>{if(race)launchingGames.add('gta-vi');await fn(games);writes++},win:{webContents:{send:()=>notices++}}};
 vm.runInNewContext(fs.readFileSync(dir+'/main.cjs','utf8').split('\n').find(line=>line.includes("handleIpc('gta6-render-resolution'")),context);
 const save=handlers['gta6-render-resolution'];assert.equal(await save(null,'1080p'),'1080p');assert.equal(games[0].gtaRenderResolution,'1080p');assert.equal(writes,1);assert.equal(notices,1);
 for(const value of ['4k','__proto__','',null,{},720,['720p']])await assert.rejects(save(null,value),/Select 720p or 1080p/);
 sessions.add('gta-vi');await assert.rejects(save(null,'720p'),/Close GTA VI/);sessions.clear();
 launchingGames.add('gta-vi');await assert.rejects(save(null,'720p'),/Close GTA VI/);launchingGames.clear();
 race=true;await assert.rejects(save(null,'720p'),/Close GTA VI/);assert.equal(writes,1);assert.equal(games[0].gtaRenderResolution,'1080p');
});
test('resolution selector is beside memory setup, saves choices, rolls back errors, and disables while running',async()=>{
 const {document}=parseHTML('<html><body><div id="root"></div></body></html>');const root=document.querySelector('#root');let saved;
 const block=(tag,cls='',text='')=>{const el=document.createElement(tag);el.className=cls;el.textContent=text;if(tag==='select')Object.defineProperty(el,'value',{value:'',writable:true});return el};
 const context={document,block,paragraph:(p,t,c)=>{const e=block('p',c,t);p.append(e);return e},action:(p,t,f)=>{const e=block('button','pill',t);e.onclick=f;p.append(e);return e},api:{gta6RenderResolution:async value=>{saved=value}},toast:()=>{},reload:async()=>{}};
 const source=fs.readFileSync(dir+'/app.js','utf8');vm.createContext(context);vm.runInContext(source.slice(source.indexOf('function renderGta6Options(root,game)'),source.indexOf('const renderBeforeGta6=')),context);
 const game={id:'gta-vi'};context.renderGta6Options(root,game);const select=root.querySelector('select');assert.equal(select.value,'720p');assert.equal(select.options.length,2);assert.equal(select.getAttribute('aria-label'),'GTA VI render resolution');assert(select.parentElement.parentElement.textContent.includes('Set up GTA VI memory'));
 select.value='1080p';await select.onchange();assert.equal(saved,'1080p');assert.equal(game.gtaRenderResolution,'1080p');
 context.api.gta6RenderResolution=async()=>{throw Error('Save failed')};select.value='720p';await select.onchange();assert.equal(select.value,'1080p');assert.equal(select.disabled,false);
 root.replaceChildren();context.renderGta6Options(root,{id:'gta-vi',running:true,gtaRenderResolution:'1080p'});assert.equal(root.querySelector('select').disabled,true);assert.equal(root.querySelector('button').disabled,true);
});
test('paging drive selector persists selection, applies it on setup, and rolls back errors',async()=>{
 const {document}=parseHTML('<html><body><div id="root"></div></body></html>'),root=document.querySelector('#root');let selected,applied;
 const block=(tag,cls='',text='')=>{const el=document.createElement(tag);el.className=cls;el.textContent=text;if(tag==='select')Object.defineProperty(el,'value',{value:'',writable:true});return el};
 const context={document,block,paragraph:(p,t,c)=>{const e=block('p',c,t);p.append(e);return e},action:(p,t,f)=>{const e=block('button','pill',t);e.onclick=f;p.append(e);return e},api:{gta6PagefileDrives:async()=>[{drive:'C:',free:100*1073741824,activeMB:15360},{drive:'D:',free:1000*1073741824,activeMB:0}],gta6PagefileDrive:async value=>selected=value,gta6MemorySetup:async value=>{applied=value;return {message:'Restart Windows'}}},toast:()=>{}};
 const source=fs.readFileSync(dir+'/app.js','utf8');vm.createContext(context);vm.runInContext(source.slice(source.indexOf('function renderGta6Options(root,game)'),source.indexOf('const renderBeforeGta6=')),context);
 const game={id:'gta-vi'};context.renderGta6Options(root,game);await new Promise(setImmediate);const drive=root.querySelector('[aria-label="GTA VI paging-file drive"]');assert.equal(drive.options.length,3);assert.equal(drive.value,'auto');drive.value='D:';await drive.onchange();assert.equal(selected,'D:');assert.equal(game.gtaPagefileDrive,'D:');await root.querySelector('button').onclick();assert.equal(applied,'D:');
 context.api.gta6PagefileDrive=async()=>{throw Error('Drive unavailable')};drive.value='C:';await drive.onchange();assert.equal(drive.value,'D:');assert.equal(game.gtaPagefileDrive,'D:');
 root.replaceChildren();context.renderGta6Options(root,{id:'gta-vi',running:true});await new Promise(setImmediate);assert.equal(root.querySelector('[aria-label="GTA VI paging-file drive"]').disabled,true);
});
