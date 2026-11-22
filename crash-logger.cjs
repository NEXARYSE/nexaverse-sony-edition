const fs=require('node:fs/promises');
const path=require('node:path');

function serialise(value){
  if(value instanceof Error)return {name:value.name,message:value.message,stack:value.stack};
  if(Array.isArray(value))return value.map(serialise);
  if(value&&typeof value==='object'){
    try{return Object.fromEntries(Object.entries(value).map(([key,item])=>[key,serialise(item)]));}catch{return String(value);}
  }
  return value==null?null:typeof value==='string'||typeof value==='number'||typeof value==='boolean'?value:String(value);
}

function redact(value){
  return JSON.parse(JSON.stringify(value).replace(/[A-Za-z]:\\\\Users\\\\[^\\\\"\r\n]+/g,'%USERPROFILE%'));
}

function create({userData,version}){
  const directory=path.join(userData,'crash-reports');
  async function trim(){
    const entries=await fs.readdir(directory,{withFileTypes:true});
    const reports=(await Promise.all(entries.filter(entry=>entry.isFile()&&entry.name.endsWith('.json')).map(async entry=>({name:entry.name,stat:await fs.stat(path.join(directory,entry.name))})))).sort((a,b)=>b.stat.mtimeMs-a.stat.mtimeMs);
    await Promise.all(reports.slice(20).map(report=>fs.unlink(path.join(directory,report.name)).catch(()=>{})));
  }
  async function write(event,details={}){
    await fs.mkdir(directory,{recursive:true});
    const timestamp=new Date().toISOString();
    const report=redact({format:1,timestamp,event,launcherVersion:version,platform:process.platform,arch:process.arch,details:serialise(details)});
    const filename=`NEXAVERSE-crash-${timestamp.replace(/[:.]/g,'-')}.json`;
    await fs.writeFile(path.join(directory,filename),JSON.stringify(report,null,2)+'\n','utf8');
    trim().catch(()=>{});
    return path.join(directory,filename);
  }
  return {directory,write};
}

module.exports={create};
