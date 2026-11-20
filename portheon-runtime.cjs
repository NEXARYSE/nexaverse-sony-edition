const fs=require('node:fs/promises'),path=require('node:path');
const VERSION='7.3';
const defaults={configured:'0',quality:'0',present:'0',fullscreen:'1',dof_off:'1',motion_blur_off:'1',film_grain_off:'1',film_fx_off:'1',parsec:'0',actions_menu:'0',unlock_all:'0',infinite_stamina:'0',no_damage:'0',particles_off:'0'};
function create({runtimeRoot,userData}){
 const installRoot=path.join(userData,'portheon');
 const executable=()=>path.join(installRoot,'PORTHEON.exe');
 async function exists(file){try{await fs.access(file);return true}catch{return false}}
 async function copyRuntime(){if(await exists(executable()))return;await fs.mkdir(path.dirname(installRoot),{recursive:true});await fs.cp(runtimeRoot,installRoot,{recursive:true,force:true,errorOnExist:false});if(!await exists(executable()))throw Error('PORTHEON runtime could not be prepared.');}
 async function configure(){const marker=path.join(installRoot,'.nexaverse-ufc6-defaults-'+VERSION);if(await exists(marker))return;const file=path.join(installRoot,'games','UFC6','UFC6.ini');let text=await fs.readFile(file,'utf8');for(const [key,value] of Object.entries(defaults)){const expression=new RegExp('^'+key.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'=.*$','m');text=expression.test(text)?text.replace(expression,key+'='+value):text+'\n'+key+'='+value;}text=text.replace(/^hw_sig=.*$(\r?\n)?/m,'').replace(/^auto_summary=.*$(\r?\n)?/m,'').replace(/^user=.*$(\r?\n)?/m,'');await fs.writeFile(file,text);await fs.writeFile(marker,VERSION);}
 return {executable,async launchPlan(){await copyRuntime();await configure();return {exe:executable(),args:[],cwd:installRoot}}};
}
module.exports={create,VERSION};
