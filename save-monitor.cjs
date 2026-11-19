const fs=require('node:fs/promises'),path=require('node:path'),os=require('node:os');
// Detection is separate from verification: unidentified binary saves never award trophies.
const folders={
 'last-of-us':['The Last of Us Part I'],'last-of-us-2':['The Last of Us Part II'],
 'god-of-war':['God of War'],'god-of-war-ragnarok':['God of War Ragnarok','God of War Ragnarök'],
 'spider-man':['Marvel\'s Spider-Man Remastered'],'miles-morales':['Marvel\'s Spider-Man Miles Morales'],
 'spider-man-2':['Marvel\'s Spider-Man 2'],'ratchet-clank':['Ratchet & Clank - Rift Apart'],
 'uncharted':['Uncharted Legacy of Thieves Collection'],'returnal':['Returnal'],
 'horizon-zero-dawn':['Horizon Zero Dawn'],'horizon-forbidden-west':['Horizon Forbidden West Complete Edition'],
 'until-dawn':['Until Dawn'],'death-stranding':['DeathStranding'],'death-stranding-2':['DeathStranding2']
};
const snapshots=new Map(),pending=new Map();
async function inspect(game,force=false){
 if(!game.exe)return {status:'unlinked',message:'Link this game to start checking for saves.',checkedAt:null,candidates:0,verificationSupported:false};
 const old=snapshots.get(game.id);
 if(!force&&old&&Date.now()-old.checkedAt<30000)return old;
 if(pending.has(game.id))return pending.get(game.id);
 const job=(async()=>{
  const home=os.homedir(),names=folders[game.id]||[],roots=[{folder:path.dirname(game.exe),depth:3}];
  if(game.saveDirectory)roots.unshift({folder:game.saveDirectory,depth:5});
  for(const name of names)for(const base of [path.join(home,'Saved Games'),path.join(home,'Documents'),path.join(home,'Documents','My Games'),process.env.LOCALAPPDATA,process.env.APPDATA].filter(Boolean))roots.push({folder:path.join(base,name),depth:5});
  const seen=new Set(),files=[];let entriesVisited=0,limited=false,unreadable=false;
  async function walk(folder,depth){
   const key=path.resolve(folder).toLowerCase();if(seen.has(key))return;seen.add(key);
   let entries;try{entries=await fs.readdir(folder,{withFileTypes:true})}catch(e){if(!['ENOENT','ENOTDIR'].includes(e.code))unreadable=true;return}
   for(const entry of entries){
    if(++entriesVisited>5000){limited=true;return}
    if(entry.isSymbolicLink())continue;
    const file=path.join(folder,entry.name);
    if(entry.isDirectory()){
     if(depth>0&&!/^(node_modules|assets|cache|shadercache|logs|redist|_commonredist)$/i.test(entry.name))await walk(file,depth-1);
    }else if(entry.isFile()&&(/\.(sav|save|savegame|sdata)$/i.test(entry.name)||/^(USR-DATA|SAVEFILE\d*|profile\.dat|slot\d+\.dat)$/i.test(entry.name))){
     try{const stat=await fs.stat(file);files.push({path:file,size:stat.size,modified:stat.mtimeMs})}catch{unreadable=true}
    }
   }
  }
  for(const root of roots){if(entriesVisited>5000)break;await walk(root.folder,root.depth)}
  const fingerprint=JSON.stringify(files.sort((a,b)=>a.path.localeCompare(b.path)));
  const changed=old?old.fingerprint!==fingerprint:false;
  const snapshot={status:files.length?'reader-required':'no-saves',checkedAt:Date.now(),candidates:files.length,verificationSupported:false,changed,lastChanged:changed?Date.now():old?.lastChanged||null,limited,unreadable,fingerprint,
   message:files.length?'Save candidates detected. Trophy verification needs a validated reader for this game.':'No supported gameplay save candidates found yet.'};
  snapshots.set(game.id,snapshot);return snapshot;
 })();
 pending.set(game.id,job);try{return await job}finally{pending.delete(game.id)}
}
async function status(game,force){const {fingerprint,...publicState}=await inspect(game,force);return publicState}
function start(read){const timer=setInterval(async()=>{try{for(const game of await read())if(game.exe)await status(game)}catch{}},30000);timer.unref()}
module.exports={status,start};
