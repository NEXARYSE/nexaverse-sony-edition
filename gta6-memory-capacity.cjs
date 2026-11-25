'use strict';
const path=require('node:path'),run=require('node:util').promisify(require('node:child_process').execFile);
const G=1024**3,M=1024**2,TARGET=64*G;
const encode=s=>Buffer.from(s,'utf16le').toString('base64');
async function ps(script,timeout=30000){return run(path.join(process.env.SystemRoot||'C:\\Windows','System32','WindowsPowerShell','v1.0','powershell.exe'),['-NoProfile','-NonInteractive','-EncodedCommand',encode(script)],{timeout,windowsHide:true,maxBuffer:1048576});}
const readScript=`$ErrorActionPreference='Stop'
$c=Get-CimInstance Win32_ComputerSystem
$m=Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
$d=Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$env:SystemDrive'"
$name="$env:SystemDrive\\pagefile.sys"
$u=Get-CimInstance Win32_PageFileUsage|Where-Object {$_.Name -ieq $name}|Select-Object -First 1
$s=Get-CimInstance Win32_PageFileSetting|Where-Object {$_.Name -ieq $name}|Select-Object -First 1
@{automatic=[bool]$c.AutomaticManagedPagefile;ram=[double]$c.TotalPhysicalMemory;committed=[double]$m.CommittedBytes;limit=[double]$m.CommitLimit;freeDisk=[double]$d.FreeSpace;allocatedMB=[double]$u.AllocatedBaseSize;initialMB=[double]$s.InitialSize;maximumMB=[double]$s.MaximumSize}|ConvertTo-Json -Compress`;
async function inspect(){const {stdout}=await ps(readScript);const s=JSON.parse(stdout.trim());for(const k of ['ram','committed','limit','freeDisk','allocatedMB','initialMB','maximumMB'])if(!Number.isFinite(s[k])||s[k]<0)throw Error('Invalid Windows memory information');if(s.ram<=0||s.limit<=0||s.committed>s.limit||typeof s.automatic!=='boolean')throw Error('Invalid Windows memory information');return s;}
function plan(s){const requiredMB=Math.max(4096,Math.ceil((TARGET-s.ram+G)/G)*1024),initialMB=Math.ceil(Math.max(requiredMB,s.allocatedMB,s.initialMB));return {requiredMB,initialMB,maximumMB:Math.max(initialMB,s.maximumMB),growth:Math.max(0,initialMB-s.allocatedMB)*M,pending:!s.automatic&&s.initialMB>=requiredMB};}
// Re-query inside the elevated process. No renderer input or game paths enter
// this code. Existing paging-file sizes, including on other drives, are kept.
const setupScript=`$ErrorActionPreference='Stop'
try {
$c=Get-CimInstance Win32_ComputerSystem
$m=Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
if([double]$m.CommitLimit -ge 68719476736){exit 0}
$name="$env:SystemDrive\\pagefile.sys"
$usage=@(Get-CimInstance Win32_PageFileUsage)
$settings=@(Get-CimInstance Win32_PageFileSetting)
$current=$settings|Where-Object {$_.Name -ieq $name}|Select-Object -First 1
$allocated=[double](($usage|Where-Object {$_.Name -ieq $name}|Measure-Object AllocatedBaseSize -Sum).Sum)
$required=[Math]::Max(4096,[Math]::Ceiling((68719476736-[double]$c.TotalPhysicalMemory+1073741824)/1073741824)*1024)
$initial=[uint32][Math]::Max($required,[Math]::Max($allocated,[double]$current.InitialSize))
$maximum=[uint32][Math]::Max($initial,[double]$current.MaximumSize)
$d=Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$env:SystemDrive'"
if([double]$d.FreeSpace -lt ([Math]::Max(0,$initial-$allocated)*1048576+8589934592)){exit 3}
$automatic=[bool]$c.AutomaticManagedPagefile
Set-CimInstance -InputObject $c -Property @{AutomaticManagedPagefile=$false}
$created=@()
try {
 foreach($u in $usage|Where-Object {$_.Name -ine $name}) {
  if(-not ($settings|Where-Object {$_.Name -ieq $u.Name})) {
   $size=[uint32][Math]::Max(1,$u.AllocatedBaseSize)
   $entry=New-CimInstance Win32_PageFileSetting -Property @{Name=$u.Name;InitialSize=$size;MaximumSize=$size}
   $created+=,$entry
  }
 }
 if($current){Set-CimInstance -InputObject $current -Property @{InitialSize=$initial;MaximumSize=$maximum}}
 else {$entry=New-CimInstance Win32_PageFileSetting -Property @{Name=$name;InitialSize=$initial;MaximumSize=$maximum};$created+=,$entry}
 $verify=Get-CimInstance Win32_PageFileSetting|Where-Object {$_.Name -ieq $name}|Select-Object -First 1
 if(-not $verify -or $verify.InitialSize -lt $initial -or $verify.MaximumSize -lt $maximum){throw 'Setting verification failed'}
}catch {
 foreach($entry in $created){Remove-CimInstance -InputObject $entry -ErrorAction SilentlyContinue}
 if($current){Set-CimInstance -InputObject $current -Property @{InitialSize=[uint32]$current.InitialSize;MaximumSize=[uint32]$current.MaximumSize}}
 Set-CimInstance -InputObject $c -Property @{AutomaticManagedPagefile=$automatic}
 throw
}
exit 0
}catch {exit 1}`;
async function configure(){await ps(`$ErrorActionPreference='Stop'
$p=Start-Process -FilePath "$env:SystemRoot\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand','${encode(setupScript)}') -Verb RunAs -WindowStyle Hidden -Wait -PassThru
if($p.ExitCode -ne 0){throw 'Paging-file setup failed'}`,180000);}
function create({query=inspect,configure:apply=configure}={}){return {async beforeLaunch(){
 let s;try{s=await query();}catch{throw Error('Could not check Windows virtual memory. GTA VI was not started.');}
 if(s.limit<TARGET){const p=plan(s);
  if(p.pending)throw Error('Restart Windows to activate the configured paging file. Current commit limit: '+(s.limit/G).toFixed(1)+' GiB; required minimum: 64 GiB. GTA VI was not started.');
  if(s.freeDisk<p.growth+8*G)throw Error('Free at least '+Math.ceil((p.growth+8*G-s.freeDisk)/G)+' GiB more on the Windows drive for GTA VI paging-file setup. No settings were changed.');
  try{await apply();}catch{throw Error('Paging-file setup was cancelled or failed. Approve the Windows administrator prompt and check disk space. GTA VI was not started.');}
  s=await query();if(s.limit<TARGET)throw Error('Restart Windows to activate the larger paging file, then retry GTA VI. Current commit limit: '+(s.limit/G).toFixed(1)+' GiB; required minimum: 64 GiB.');
 }
 if(s.limit-s.committed<8*G)throw Error('Less than 8 GiB of commit capacity is available. Close memory-heavy applications and retry. GTA VI was not started.');return true;
}};}
module.exports={create,inspect,plan,setupScript,readScript};
