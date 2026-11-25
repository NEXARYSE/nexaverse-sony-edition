const fs=require('node:fs/promises'),path=require('node:path');
function create({BrowserWindow,register,home}){
  let active=null;
  function owned(event){if(!active||event.sender!==active.window.webContents)throw Error('This file picker is no longer open.');return active;}
  function extensions(options){return new Set((options.filters||[]).flatMap(f=>f.extensions||[]).map(e=>e.toLowerCase()));}
  async function list(directory,options){
    const absolute=path.resolve(directory);
    const entries=await fs.readdir(absolute,{withFileTypes:true}),allowed=extensions(options),folders=options.properties?.includes('openDirectory');
    const items=[];
    for(const entry of entries){
      if(entry.isDirectory())items.push({name:entry.name,path:path.join(absolute,entry.name),folder:true});
      else if(!folders&&entry.isFile()&&(!allowed.size||allowed.has('*')||allowed.has(path.extname(entry.name).slice(1).toLowerCase())))items.push({name:entry.name,path:path.join(absolute,entry.name),folder:false});
    }
    items.sort((a,b)=>Number(b.folder)-Number(a.folder)||a.name.localeCompare(b.name,undefined,{numeric:true}));
    const roots=[];
    if(process.platform==='win32')for(const letter of 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'){
      const root=letter+':\\';try{if((await fs.stat(root)).isDirectory())roots.push(root);}catch{}
    }
    else roots.push('/');
    return {directory:absolute,parent:path.dirname(absolute),entries:items,roots,folderMode:Boolean(folders),title:options.title||'Choose a game location'};
  }
  register('controller-picker-list',async(event,directory)=>{const state=owned(event);return list(typeof directory==='string'&&directory?directory:state.directory,state.options);});
  register('controller-picker-choose',async(event,selection)=>{
    const state=owned(event);if(typeof selection!=='string'||!selection)throw Error('Choose a file or folder.');
    const absolute=path.resolve(selection),stat=await fs.stat(absolute),folders=state.options.properties?.includes('openDirectory'),allowed=extensions(state.options);
    if(folders?!stat.isDirectory():!stat.isFile())throw Error(folders?'Choose a folder.':'Choose a file.');
    if(!folders&&allowed.size&&!allowed.has('*')&&!allowed.has(path.extname(absolute).slice(1).toLowerCase()))throw Error('Choose one of the supported file types.');
    state.resolve({canceled:false,filePaths:[absolute]});state.window.close();return true;
  });
  register('controller-picker-cancel',event=>{const state=owned(event);state.window.close();return true;});
  async function open(parent,options={}){
    if(active)throw Error('Finish the open file picker first.');
    let directory=options.defaultPath||home;
    try{if(!(await fs.stat(directory)).isDirectory())directory=path.dirname(directory);}catch{directory=home;}
    return new Promise((resolve,reject)=>{
      const window=new BrowserWindow({parent,modal:true,width:1100,height:760,minWidth:720,minHeight:520,show:false,title:options.title||'Choose a game location',autoHideMenuBar:true,webPreferences:{preload:path.join(__dirname,'controller-picker-preload.cjs'),contextIsolation:true,nodeIntegration:false,sandbox:true}});
      const state={window,options,directory,resolve};active=state;
      window.on('closed',()=>{if(active===state)active=null;resolve({canceled:true,filePaths:[]});});
      window.once('ready-to-show',()=>window.show());
      window.loadFile(path.join(__dirname,'controller-picker.html')).catch(error=>{window.close();reject(error);});
    });
  }
  return {open,cancel(){active?.window.close();},owns(sender){return active?.window.webContents===sender;},list};
}
module.exports={create};
