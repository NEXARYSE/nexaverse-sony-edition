const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs/promises'),os=require('node:os'),path=require('node:path');
const {create}=require('./gta6-runtime.cjs');
test('save folder follows launcher default or configured drive without modifying saves',async t=>{
 const root=await fs.mkdtemp(path.join(os.tmpdir(),'nexa-save-folder-'));t.after(async()=>{assert(path.basename(root).startsWith('nexa-save-folder-'));await fs.rm(root,{recursive:true,force:true})});
 const runtime=create({runtimeRoot:root,appData:root});assert.equal(await runtime.saveFolder(),path.join(root,'NEXA GTA','victory','data','7952987A','saves'));
 const settings=path.join(root,'NEXA GTA','7900-storage.json');await fs.mkdir(path.dirname(settings),{recursive:true});
 const dataDir=path.join(root,'secondary-drive');await fs.writeFile(settings,JSON.stringify({dataDir}));assert.equal(await runtime.saveFolder(),path.join(dataDir,'7952987A','saves'));
 await fs.writeFile(settings,JSON.stringify({dataDir:'relative/path'}));await assert.rejects(runtime.saveFolder(),/Invalid GTA storage/);
});
