'use strict';
const path=require('node:path'),run=require('node:util').promisify(require('node:child_process').execFile);
const G=1024**3,M=1024**2,TARGET=64*G;
const encode=s=>Buffer.from(s,'utf16le').toString('base64');
async function ps(script,timeout=30000){return run(path.join(process.env.SystemRoot||'C:\\Windows','System32','WindowsPowerShell','v1.0','powershell.exe'),['-NoProfile','-NonInteractive','-EncodedCommand',encode(script)],{timeout,windowsHide:true,maxBuffer:1048576});}
const readScript=`$ErrorActionPreference='Stop'
$c=Get-CimInstance Win32_ComputerSystem
$m=Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
$drives=@(Get-CimInstance Win32_LogicalDisk -Filter 'DriveType=3'|Where-Object {$_.FileSystem -eq 'NTFS' -and $null -ne $_.FreeSpace}|ForEach-Object {@{letter=$_.DeviceID;free=[double]$_.FreeSpace}})
$usage=@(Get-CimInstance Win32_PageFileUsage|ForEach-Object {@{name=$_.Name;mb=[double]$_.AllocatedBaseSize}})
$settings=@(Get-CimInstance Win32_PageFileSetting|ForEach-Object {@{name=$_.Name;initial=[double]$_.InitialSize;maximum=[double]$_.MaximumSize}})
@{automatic=[bool]$c.AutomaticManagedPagefile;limit=[double]$m.CommitLimit;committed=[double]$m.CommittedBytes;systemDrive=$env:SystemDrive;drives=$drives;usage=$usage;settings=$settings}|ConvertTo-Json -Depth 5 -Compress`;
async function inspect(){const {stdout}=await ps(readScript);const s=JSON.parse(stdout.trim());if(!Number.isFinite(s.limit)||s.limit<=0||!Number.isFinite(s.committed)||s.committed<0||s.committed>s.limit||!Array.isArray(s.drives)||!Array.isArray(s.usage)||!Array.isArray(s.settings))throw Error('Invalid Windows memory information');return s;}
function plan(s,preferredDrive='auto'){
 if(preferredDrive!=='auto'&&(typeof preferredDrive!=='string'||! /^[A-Z]:$/i.test(preferredDrive)))throw Error('Select an available local NTFS drive.');
 if(preferredDrive!=='auto'){
  preferredDrive=preferredDrive.toUpperCase();
  const disk=s.drives.find(d=>d.letter.toUpperCase()===preferredDrive);
  if(!disk)throw Error('Selected paging-file drive is unavailable.');
  const name=preferredDrive+'\\pagefile.sys',active=s.usage.reduce((n,u)=>n+u.mb,0),current=s.usage.find(u=>u.name.toLowerCase()===name.toLowerCase())?.mb||0;
  // Subtract capacity already supplied by files awaiting removal: Windows may
  // activate D immediately while C remains active until restart.
  const additional=Math.ceil((TARGET-s.limit+G)/G)*1024;
  const initialMB=Math.ceil(Math.max(1024,current,active+additional,s.settings.reduce((n,p)=>n+p.initial,0)));
  const chosen=s.settings.find(p=>p.name.toLowerCase()===name.toLowerCase());
  if(!s.automatic&&s.settings.length===1&&chosen?.initial>=initialMB)return {pending:true,drive:preferredDrive};
  const reserve=preferredDrive.toLowerCase()===s.systemDrive.toLowerCase()?8*G:2*G,growth=Math.max(0,initialMB-current)*M;
  if(!Number.isFinite(disk.free)||disk.free<growth+reserve)throw Error('Not enough free space on '+preferredDrive+' for the paging file. Free at least '+Math.ceil((growth+reserve-(disk.free||0))/G)+' GiB and retry.');
  return {drive:preferredDrive,initialMB,maximumMB:Math.max(initialMB,chosen?.maximum||0),growth,reserve,moveToDrive:true};
 }

 const additionalMB=Math.max(1024,Math.ceil((TARGET-s.limit+G)/G)*1024);
 const allocated=name=>s.usage.find(u=>u.name.toLowerCase()===name.toLowerCase())?.mb||0;
 const pendingMB=s.settings.reduce((n,p)=>n+Math.max(0,p.initial-allocated(p.name)),0);
 if(!s.automatic&&pendingMB>=additionalMB)return {pending:true};
 const candidates=s.drives.filter(d=>/^[A-Z]:$/i.test(d.letter)&&Number.isFinite(d.free)&&d.free>=0).map(d=>{
  const name=d.letter+'\\pagefile.sys',old=s.settings.find(p=>p.name.toLowerCase()===name.toLowerCase());
  const active=allocated(name),initialMB=Math.ceil(Math.max(active+additionalMB,old?.initial||0));
  // Keep 8 GiB free on Windows; 2 GiB on a secondary local NTFS volume.
  const reserve=d.letter.toLowerCase()===s.systemDrive.toLowerCase()?8*G:2*G;
  const growth=Math.max(0,initialMB-active)*M;
  return {drive:d.letter,initialMB,maximumMB:Math.max(initialMB,old?.maximum||0),growth,reserve,free:d.free};
 }).sort((a,b)=>(b.free-b.growth-b.reserve)-(a.free-a.growth-a.reserve));
 const selected=candidates.find(p=>p.free>=p.growth+p.reserve);
 if(selected)return selected;
 const best=candidates[0];throw Error(best?'No suitable local drive has enough paging-file space. Free at least '+Math.ceil((best.growth+best.reserve-best.free)/G)+' GiB on '+best.drive+' and retry. No settings were changed.':'No writable local NTFS drive was found for a paging file. No settings were changed.');
}
function setupScript(p){
 if(!/^[A-Z]:$/i.test(p.drive)||!Number.isInteger(p.initialMB)||p.initialMB<1||p.initialMB>1048576)throw Error('Invalid paging-file plan');
 return `$ErrorActionPreference='Stop'
try {
$memory=Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
${p.moveToDrive?'# Explicit drive selection relocates paging files even when total capacity is sufficient.':'if([double]$memory.CommitLimit -ge 68719476736){exit 0}'}
$drive='${p.drive}'
$name="$drive\\pagefile.sys"
$disk=Get-CimInstance Win32_LogicalDisk|Where-Object {$_.DeviceID -eq $drive -and $_.DriveType -eq 3 -and $_.FileSystem -eq 'NTFS'}|Select-Object -First 1
if(-not $disk){throw 'Selected local NTFS drive is no longer available'}
# This privilege is required by Win32_PageFileSetting. Enable it only on the
# elevated WMI operation scope, rather than changing Windows security policy.
$scope=[System.Management.ManagementScope]::new('\\\\.\\root\\cimv2')
$scope.Options.EnablePrivileges=$true
$scope.Connect()
$class=[System.Management.ManagementClass]::new($scope,[System.Management.ManagementPath]::new('Win32_PageFileSetting'),$null)
$settings=@($class.GetInstances())
$usage=@(Get-CimInstance Win32_PageFileUsage)
$current=$settings|Where-Object {$_.Name -ieq $name}|Select-Object -First 1
$active=[double](($usage|Where-Object {$_.Name -ieq $name}|Measure-Object AllocatedBaseSize -Sum).Sum)
$additional=[Math]::Max(1024,[Math]::Ceiling((68719476736-[double]$memory.CommitLimit+1073741824)/1073741824)*1024)
$initial=[uint32][Math]::Max(${p.initialMB},[Math]::Max($active+$additional,[double]$current.InitialSize))
$maximum=[uint32][Math]::Max($initial,[double]$current.MaximumSize)
if([double]$disk.FreeSpace -lt ([Math]::Max(0,$initial-$active)*1048576+${p.reserve})){throw 'Free disk space changed; free more space and retry'}
$computer=Get-CimInstance Win32_ComputerSystem
$automatic=[bool]$computer.AutomaticManagedPagefile
$oldInitial=[uint32]$current.InitialSize;$oldMaximum=[uint32]$current.MaximumSize
$created=@()
$removed=@()
Set-CimInstance -InputObject $computer -Property @{AutomaticManagedPagefile=$false}
try {
 foreach($u in $usage){if(${p.moveToDrive?'$false':'$true'} -and $u.Name -ine $name -and -not ($settings|Where-Object {$_.Name -ieq $u.Name})){
  $entry=$class.CreateInstance();$entry.Name=$u.Name;$entry.InitialSize=[uint32]$u.AllocatedBaseSize;$entry.MaximumSize=[uint32]$u.AllocatedBaseSize;[void]$entry.Put();$created+=,$entry
 }}
 $entry=$current
 if(-not $entry){$entry=$class.CreateInstance();$entry.Name=$name;$created+=,$entry}
 $entry.InitialSize=$initial;$entry.MaximumSize=$maximum;[void]$entry.Put()
 $verify=@($class.GetInstances())|Where-Object {$_.Name -ieq $name}|Select-Object -First 1
 if(-not $verify -or $verify.InitialSize -lt $initial -or $verify.MaximumSize -lt $maximum){throw 'Windows did not retain the paging-file settings'}
${p.moveToDrive?"foreach($other in $settings){if($other.Name -ine $name){$record=@{name=[string]$other.Name;initial=[uint32]$other.InitialSize;maximum=[uint32]$other.MaximumSize};[void]$other.Delete();$removed+=,$record}}":''}
}catch {
 $failure=$_
 foreach($record in $removed){$restore=$class.CreateInstance();$restore.Name=$record.name;$restore.InitialSize=$record.initial;$restore.MaximumSize=$record.maximum;[void]$restore.Put()}
 foreach($entry in $created){try{[void]$entry.Delete()}catch{}}
 if($current){$current.InitialSize=$oldInitial;$current.MaximumSize=$oldMaximum;[void]$current.Put()}
 Set-CimInstance -InputObject $computer -Property @{AutomaticManagedPagefile=$automatic}
 throw $failure
}
exit 0
}catch {if($_.Exception.Message -like '*Free disk space changed*'){exit 3};if($_.Exception.Message -like '*Selected local NTFS drive*'){exit 4};if($_.Exception.Message -like '*did not retain*'){exit 5};exit ([int]$_.Exception.HResult)}`;
}
async function configure(p){
 const child=encode(setupScript(p));
 const {stdout}=await ps(`$ErrorActionPreference='Stop'
+try{$p=Start-Process -FilePath "$env:SystemRoot\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand','${child}') -Verb RunAs -WindowStyle Hidden -Wait -PassThru;Write-Output ('NEXA_EXIT:'+$p.ExitCode)}catch{$failure=$_.Exception;while($failure){if($failure -is [ComponentModel.Win32Exception] -and $failure.NativeErrorCode -eq 1223){Write-Output 'UAC_CANCELLED';exit 0};$failure=$failure.InnerException};throw}`.replace(/^\+/gm,''),180000);
 if(stdout.includes('UAC_CANCELLED'))throw Error('Windows administrator approval was cancelled. Approve the UAC prompt to apply the paging file.');
 const match=stdout.match(/NEXA_EXIT:(-?\d+)/);if(!match)throw Error('Windows did not report a paging-file setup result.');
 const code=Number(match[1])>>>0;
 if(code===3)throw Error('Free disk space changed during setup. Free more space and retry; another local drive will be checked automatically.');
 if(code===4)throw Error('The selected local drive became unavailable. Reconnect it and retry memory setup.');
 if(code===5)throw Error('Windows did not retain the paging-file settings. Setup was not confirmed.');
 if(code!==0)throw Error('Windows paging-file setup failed (0x'+code.toString(16).padStart(8,'0')+'). Administrator paging-file permission, disk access, or configuration was refused. No successful setup was confirmed.');
}
function create({query=inspect,configure:apply=configure}={}){
 async function setup(preferredDrive='auto'){
  let s=await query();
  if(preferredDrive!=='auto'&&(typeof preferredDrive!=='string'||! /^[A-Z]:$/i.test(preferredDrive)))throw Error('Select an available local NTFS drive.');
  const selected=preferredDrive==='auto'?null:preferredDrive.toUpperCase()+'\\pagefile.sys';
  if(s.limit>=TARGET&&(!selected||(!s.automatic&&s.settings.length===1&&s.settings[0].name.toLowerCase()===selected.toLowerCase()&&s.usage.every(u=>u.name.toLowerCase()===selected.toLowerCase()))))return {ready:true,message:'Windows commit capacity is '+(s.limit/G).toFixed(1)+' GiB. No paging-file changes are needed.'};
  const p=plan(s,preferredDrive);
  if(p.pending)return {ready:false,message:'Paging capacity is already configured'+(p.drive?' on '+p.drive:'')+'. Restart Windows to finish applying paging-file changes.'};
  await apply(p);s=await query();
  if(preferredDrive==='auto'&&s.limit>=TARGET)return {ready:true,message:'Paging-file setup completed. Active commit capacity is '+(s.limit/G).toFixed(1)+' GiB.'};
  const retained=preferredDrive!=='auto'&&!s.automatic&&s.settings.length===1&&s.settings[0].name.toLowerCase()===selected.toLowerCase()&&s.settings[0].initial>=p.initialMB;
  const after=retained?{pending:true}:plan(s,preferredDrive);
  if(!after.pending)throw Error('Windows did not retain sufficient paging-file capacity. No success was confirmed.');
  return {ready:false,message:'Paging-file capacity was configured on '+p.drive+'. Restart Windows to activate it'+(p.moveToDrive?' and remove the paging files on other drives':'')+'.'};
 }

 return {setup,async beforeLaunch(){
  // The 64 GiB setup target is a recommendation, not an active launch requirement.
  // Play reads current capacity only: never elevate or alter paging files here.
  const s=await query();
  if(!Number.isFinite(s.limit)||!Number.isFinite(s.committed)||s.limit<=0||s.committed<0||s.committed>s.limit)throw Error('Could not verify active Windows commit capacity. GTA VI was not started.');
  if(s.limit-s.committed<8*G)throw Error('Close memory-heavy applications to leave at least 8 GiB of available commit capacity. GTA VI was not started. If you configured a larger paging file, restart Windows to activate it.');
  return true;
 }};
}
module.exports={create,inspect,plan,setupScript,readScript};
