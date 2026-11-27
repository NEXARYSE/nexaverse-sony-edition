const test=require('node:test'),a=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const {parseHTML}=require('C:/Users/tratr/Documents/Codex/2026-11-09/i-x20/work/nexaverse-console/node_modules/linkedom');
test('logs IPC opens only known folders, creates them, and reports Explorer errors',async()=>{
 const handlers={},events=[];let failure='';
 const context={handleIpc:(n,f)=>handlers[n]=f,path,app:{getPath:key=>{a.equal(key,'appData');return 'C:/Test/Roaming'}},crashLogger:{directory:'C:/Test/Roaming/nexaverse-sony-edition/crash-reports'},fs:{mkdir:async(p,opts)=>events.push(['mkdir',p,opts.recursive])},shell:{openPath:async p=>{events.push(['open',p]);return failure}},Error};
 const source=fs.readFileSync(__dirname+'/main.cjs','utf8');vm.runInNewContext(source.slice(source.indexOf(" handleIpc('open-logs-folder'"),source.indexOf(" handleIpc('gta6-memory-setup'")),context);
 const open=handlers['open-logs-folder'];
 for(const kind of ['gta6','launcher']){events.length=0;a.equal((await open(null,kind)).opened,true);a.equal(events[0][0],'mkdir');a.equal(events[1][0],'open');a.equal(events[0][1],events[1][1]);a.equal(events[0][2],true);a.equal(events[1][1],kind==='gta6'?path.join('C:/Test/Roaming','NEXA GTA','logs'):context.crashLogger.directory)}
 events.length=0;for(const kind of ['C:/Windows','../','__proto__',null,{}])await a.rejects(open(null,kind),/Unknown logs folder/);a.equal(events.length,0);
 failure='Access denied';await a.rejects(open(null,'gta6'),/Access denied/);
});
test('Settings logs buttons invoke matching folders and display failures',async()=>{
 const {document}=parseHTML('<html><body><dialog id="panel"></dialog><div id="content"></div></body></html>');const root=document.querySelector('#content'),panel=document.querySelector('#panel');let opened=[],messages=[];
 const block=(tag,cls='',text='')=>{const e=document.createElement(tag);e.className=cls;e.textContent=text;return e};
 const context={document,$:sel=>document.querySelector(sel),modal:()=>root,block,paragraph:(p,t)=>p.append(block('p','',t)),action:(p,t,fn)=>{const e=block('button','',t);e.onclick=fn;p.append(e);return e},discordAccount:null,api:{version:'0.1.90',openLogsFolder:async kind=>opened.push(kind)},toast:text=>messages.push(text)};
 vm.createContext(context);const source=fs.readFileSync(__dirname+'/app.js','utf8');vm.runInContext(source.slice(source.indexOf('async function showSettings()'),source.indexOf("$('#settings').onclick=showSettings;")),context);await context.showSettings();
 const entry=[...root.querySelectorAll('button')].find(e=>e.textContent.includes('Open GTA VI logs or launcher crash reports'));a(entry);entry.onclick();
 const buttons=[...root.querySelectorAll('button')];await buttons.find(e=>e.textContent==='Open GTA VI logs folder').onclick();await buttons.find(e=>e.textContent==='Open launcher crash reports').onclick();a.deepEqual(opened,['gta6','launcher']);
 context.api.openLogsFolder=async()=>{throw Error('Explorer unavailable')};await buttons.find(e=>e.textContent==='Open GTA VI logs folder').onclick();a.deepEqual(messages,['Explorer unavailable']);
});
