const fs=require('node:fs/promises'),path=require('node:path'),{performance}=require('node:perf_hooks');
let file,startedAt,previousSeconds=0,loading=Promise.resolve(),writes=Promise.resolve(),timer;
function start(directory){
 if(file)return loading;
 file=path.join(directory,'activity.json');startedAt=performance.now();
 loading=fs.readFile(file,'utf8').then(raw=>{const saved=JSON.parse(raw);if(Number.isFinite(saved.seconds)&&saved.seconds>=0)previousSeconds=saved.seconds}).catch(()=>{});
 timer=setInterval(()=>flush().catch(()=>{}),30000);timer.unref();return loading;
}
async function status(){await loading;return {seconds:previousSeconds+(file?Math.max(0,(performance.now()-startedAt)/1000):0)}}
function flush(){
 if(!file)return Promise.resolve();
 const write=writes.then(async()=>{const current=await status();await fs.mkdir(path.dirname(file),{recursive:true});await fs.writeFile(file+'.tmp',JSON.stringify(current));await fs.rename(file+'.tmp',file)});
 writes=write.catch(()=>{});return write;
}
module.exports={start,status,flush};
