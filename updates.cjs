const {app}=require('electron');
const version=require('./package.json').version;
let updater,checking=null,downloading=null,installing=null,started=false,restartAt=null,restartTimer=null;
let options={canInstall:()=>true,beforeInstall:async()=>{},onState:()=>{}};
let state={status:'idle',message:'Updates download and install automatically.',version};
function set(patch){state={...state,...patch};try{options.onState({...state})}catch{}return {...state}}
function failure(message){restartAt=null;set({status:'error',message,restartIn:null})}
function client(){
 if(updater)return updater;
 updater=require('electron-updater').autoUpdater;
 updater.logger=null;updater.autoDownload=false;updater.autoInstallOnAppQuit=false;updater.allowDowngrade=false;
 updater.on('checking-for-update',()=>set({status:'checking',message:'Checking for updates…',progress:0}));
 updater.on('update-available',info=>{
  set({status:'available',availableVersion:info.version,message:'New update found. Starting download…'});
  download().catch(()=>failure('Download failed. The launcher will retry automatically.'));
 });
 updater.on('update-not-available',()=>set({status:'current',message:'You’re up to date · '+version}));
 updater.on('download-progress',p=>{const percent=Math.max(0,Math.min(100,Number(p.percent)||0));set({status:'downloading',progress:percent,message:'Downloading update · '+Math.floor(percent)+'%'})});
 updater.on('update-downloaded',()=>{
  set({status:'ready',progress:100,message:'Update downloaded. Preparing automatic restart…'});
  if(!restartTimer){restartTimer=setInterval(pulse,1000);restartTimer.unref()}
  pulse();
 });
 updater.on('error',()=>failure('Could not complete the update. The launcher will retry automatically; you can keep using it.'));
 return updater;
}
function blockedReason(){try{return options.canInstall()?'':(options.waitingReason?.()||'Waiting for your running game to close before restarting.')}catch{return 'Waiting until the launcher is ready to restart.'}}
function pulse(){
 if(!['ready','waiting','restarting'].includes(state.status))return;
 const reason=blockedReason();
 if(reason){restartAt=null;if(state.status!=='waiting'||state.message!=='Update downloaded. '+reason)set({status:'waiting',restartIn:null,message:'Update downloaded. '+reason});return}
 if(restartAt===null)restartAt=Date.now()+10000;
 const seconds=Math.max(0,Math.ceil((restartAt-Date.now())/1000));
 if(seconds===0){install().catch(()=>{});return}
 if(state.status!=='restarting'||state.restartIn!==seconds)set({status:'restarting',restartIn:seconds,message:'Update downloaded. Restarting to install in '+seconds+' seconds…'});
}
async function check(){
 if(!app.isPackaged)return set({status:'development',message:'Update checks are available in the installed launcher. Version '+version});
 if(['ready','waiting','restarting','installing'].includes(state.status)||downloading)return {...state};
 if(checking)return checking;
 checking=Promise.resolve().then(async()=>{try{await client().checkForUpdates()}catch{failure('Could not check for updates. The launcher will retry automatically.')}return {...state}});
 try{return await checking}finally{checking=null}
}
async function download(){
 if(downloading)return downloading;
 if(state.status!=='available')throw Error('Check for an available update first.');
 set({status:'downloading',progress:0,message:'Downloading update · 0%'});
 downloading=Promise.resolve().then(()=>client().downloadUpdate());
 try{await downloading;return {...state}}catch{failure('Download failed. The launcher will retry automatically.');return {...state}}finally{downloading=null}
}
async function install(){
 if(installing)return installing;
 if(!['ready','waiting','restarting'].includes(state.status))throw Error('No update is ready to install.');
 const reason=blockedReason();if(reason){pulse();throw Error(reason)}
 set({status:'installing',message:'Installing update… The launcher will restart automatically.',restartIn:0});
 installing=Promise.resolve().then(async()=>{
  await options.beforeInstall();
  const reason=blockedReason();if(reason){restartAt=null;set({status:'waiting',restartIn:null,message:'Update downloaded. '+reason});return}
  client().quitAndInstall(true,true);
 }).catch(()=>failure('The update could not be installed. The launcher will retry automatically.'));
 try{await installing}finally{installing=null}
}
function start(config={}){
 options={...options,...config};if(started||!app.isPackaged)return;started=true;client();
 const first=setTimeout(()=>check(),10000);first.unref();
 const repeat=setInterval(()=>check(),300000);repeat.unref();
}
module.exports={check,download,install,start,status:()=>({...state})};
