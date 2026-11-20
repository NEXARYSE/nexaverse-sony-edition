const fs=require('node:fs/promises'),path=require('node:path');
const VERSION='7.3';
const defaults={configured:'0',quality:'0',present:'0',fullscreen:'1',dof_off:'1',motion_blur_off:'1',film_grain_off:'1',film_fx_off:'1',parsec:'0',actions_menu:'0',unlock_all:'0',infinite_stamina:'0',no_damage:'0',particles_off:'0'};
function create({runtimeRoot,userData}){
 const installRoot=path.join(userData,'portheon');
 const executable=()=>path.join(installRoot,'PORTHEON.exe');
 const gameExecutable=()=>path.join(installRoot,'games','UFC6','UFC6.exe');
 async function exists(file){try{await fs.access(file);return true}catch{return false}}
 async function validGameDirectory(directory){return (await exists(path.join(directory,'eboot.bin')))&&(await exists(path.join(directory,'Data')))&&(await exists(path.join(directory,'sce_sys','param.json')))}
 async function validateGameDirectory(directory){
  const queue=[[path.resolve(directory),0]];
  while(queue.length){const [candidate,depth]=queue.shift();if(await validGameDirectory(candidate))return candidate;if(depth===2)continue;let entries=[];try{entries=await fs.readdir(candidate,{withFileTypes:true})}catch{}for(const entry of entries)if(entry.isDirectory()&&!entry.isSymbolicLink())queue.push([path.join(candidate,entry.name),depth+1]);}
  throw Error('Choose the UFC 6 game folder containing eboot.bin, Data, and sce_sys\\param.json.');
 }
 async function copyRuntime(){if(await exists(executable()))return;await fs.mkdir(path.dirname(installRoot),{recursive:true});await fs.cp(runtimeRoot,installRoot,{recursive:true,force:true,errorOnExist:false});if(!await exists(executable()))throw Error('PORTHEON runtime could not be prepared.');}
 async function configure(gameDirectory){
  const marker=path.join(installRoot,'.nexaverse-ufc6-defaults-'+VERSION),file=path.join(installRoot,'games','UFC6','UFC6.ini'),raw=await fs.readFile(file),utf16=raw[0]===0xff&&raw[1]===0xfe;
  let text=(utf16?raw.subarray(2).toString('utf16le'):raw.toString('utf8'));
  for(const [key,value] of Object.entries({...defaults,game:gameDirectory})){const expression=new RegExp('^'+key.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'=.*$','m');text=expression.test(text)?text.replace(expression,key+'='+value):text+'\r\n'+key+'='+value;}
  if(!await exists(marker))text=text.replace(/^hw_sig=.*$(\r?\n)?/m,'').replace(/^auto_summary=.*$(\r?\n)?/m,'').replace(/^user=.*$(\r?\n)?/m,'');
  await fs.writeFile(file,utf16?Buffer.concat([Buffer.from([0xff,0xfe]),Buffer.from(text,'utf16le')]):text);
  if(!await exists(marker))await fs.writeFile(marker,VERSION);
 }
 return {executable,gameExecutable,validateGameDirectory,async launchPlan(game){if(!game?.gameDirectory)throw Error('Use the three dots to link your UFC 6 game folder first.');const directory=await validateGameDirectory(game.gameDirectory);await copyRuntime();await configure(directory);return {exe:gameExecutable(),args:['--play'],cwd:path.dirname(gameExecutable()),windowsHide:true}}};
}
module.exports={create,VERSION};
