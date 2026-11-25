const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs/promises'),path=require('node:path'),os=require('node:os'),{EventEmitter}=require('node:events');
const {create}=require('./controller-picker.cjs');
test('controller file picker browses folders, filters supported files and validates the selected path',async()=>{
  const root=await fs.mkdtemp(path.join(os.tmpdir(),'nexa-controller-'));
  assert(path.resolve(root).startsWith(path.resolve(os.tmpdir())+path.sep+'nexa-controller-'));
  const handlers=new Map();let window;
  class TestWindow extends EventEmitter{constructor(){super();window=this;this.webContents={};}async loadFile(){}show(){}close(){this.emit('closed');}}
  async function opened(){for(let i=0;i<100&&!window;i++)await new Promise(r=>setTimeout(r,5));assert(window);}
  try{
    await fs.mkdir(path.join(root,'Game'));await fs.writeFile(path.join(root,'game.exe'),'fixture');await fs.writeFile(path.join(root,'readme.txt'),'fixture');
    const picker=create({BrowserWindow:TestWindow,register:(key,handler)=>handlers.set(key,handler),home:root});
    const listing=await picker.list(root,{filters:[{extensions:['exe']}]});assert.deepEqual(listing.entries.map(e=>e.name),['Game','game.exe']);
    const pending=picker.open(null,{properties:['openFile'],filters:[{extensions:['exe']}]});await opened();
    const event={sender:window.webContents};await assert.rejects(handlers.get('controller-picker-list')({sender:{}},root),/no longer open/);
    await assert.rejects(handlers.get('controller-picker-choose')(event,path.join(root,'readme.txt')),/supported file types/);
    await handlers.get('controller-picker-choose')(event,path.join(root,'game.exe'));assert.deepEqual(await pending,{canceled:false,filePaths:[path.join(root,'game.exe')]});
    window=null;const folder=picker.open(null,{properties:['openDirectory']});await opened();
    await handlers.get('controller-picker-choose')({sender:window.webContents},path.join(root,'Game'));assert.equal((await folder).canceled,false);
  }finally{await fs.rm(root,{recursive:true,force:true});}
});
