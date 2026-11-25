'use strict';
const path=require('node:path');
const {execFile}=require('node:child_process');
const {promisify}=require('node:util');
const run=promisify(execFile);
const GIB=1024**3;
const recommendedHeadroom=8*GIB;
const powershell=()=>path.join(process.env.SystemRoot||'C:\\Windows','System32','WindowsPowerShell','v1.0','powershell.exe');
const encoded=text=>Buffer.from(text,'utf16le').toString('base64');
async function ps(script,timeout=30000){
 return run(powershell(),['-NoProfile','-NonInteractive','-EncodedCommand',encoded(script)],{windowsHide:true,timeout,maxBuffer:1024*1024});
}
async function inspect(){
 const {stdout}=await ps(`$ErrorActionPreference='Stop'
$computer=Get-CimInstance Win32_ComputerSystem
$memory=Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
$disk=Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$env:SystemDrive'"
@{automatic=[bool]$computer.AutomaticManagedPagefile;committed=[double]$memory.CommittedBytes;limit=[double]$memory.CommitLimit;freeDisk=[double]$disk.FreeSpace}|ConvertTo-Json -Compress`);
 const state=JSON.parse(stdout.trim());
 for(const key of ['committed','limit','freeDisk'])if(!Number.isFinite(state[key])||state[key]<0)throw Error('Windows memory information is unavailable.');
 if(typeof state.automatic!=='boolean'||state.limit<=0||state.committed>state.limit)throw Error('Windows returned invalid memory information.');
 return state;
}
async function enableAutomatic(){
 // Fixed encoded code, no game paths or renderer input interpolated into an
 // elevated command. Windows owns the UAC approval; cancellation fails closed.
 const child=encoded(`$ErrorActionPreference='Stop'
try {$computer=Get-CimInstance Win32_ComputerSystem;Set-CimInstance -InputObject $computer -Property @{AutomaticManagedPagefile=$true};if(-not (Get-CimInstance Win32_ComputerSystem).AutomaticManagedPagefile){exit 2};exit 0}catch{exit 1}`);
 await ps(`$ErrorActionPreference='Stop'
$process=Start-Process -FilePath "$env:SystemRoot\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand','${child}') -Verb RunAs -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0){throw 'Windows could not enable automatic paging-file management.'}`,180000);
}
function create({query=inspect,configure=enableAutomatic,notice=async()=>{}}={}){
 return {async beforeLaunch(){
  let state;
  try{state=await query();}catch{throw Error('Could not check Windows virtual memory. GTA VI was not started. Please retry.');}
  let changed=false;
  if(!state.automatic){
   if(state.freeDisk<8*GIB)throw Error('GTA VI paging-file setup needs at least 8 GB of free space on the Windows drive. Free disk space and retry; Windows may need more as the paging file grows.');
   try{await configure();}catch{throw Error('Windows paging-file setup was cancelled or failed. GTA VI was not started. Approve the Windows administrator prompt when you retry.');}
   state=await query();
   if(!state.automatic)throw Error('Windows did not enable automatic paging-file management. GTA VI was not started.');
   changed=true;
  }
  if(state.limit-state.committed<recommendedHeadroom)throw Error('Windows has less than 8 GB of available commit capacity. GTA VI was not started to avoid the reported allocation crash. Close memory-heavy apps and check free disk space. If paging-file settings just changed, restart Windows and retry. More memory may still be required.');
  if(changed)await notice('Windows automatic paging-file management is enabled. Windows may require a restart to apply its full capacity. GTA VI can start with the current available memory; this does not fix experimental graphics or cutscene issues.');
  return true;
 }};
}
module.exports={create,inspect};
