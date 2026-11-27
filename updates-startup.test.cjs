const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
test('a missing updater dependency cannot prevent launcher startup',()=>{
 const context={module:{exports:{}},require:name=>{if(name==='electron')return {app:{isPackaged:true}};if(name==='./package.json')return {version:'0.1.92'};throw Error("Cannot find module 'fs-extra'");},setTimeout:()=>{throw Error('failed updater must not schedule a check')},setInterval:()=>{throw Error('failed updater must not schedule checks')}};
 vm.runInNewContext(fs.readFileSync(__dirname+'/updates.cjs','utf8'),context);
 assert.doesNotThrow(()=>context.module.exports.start());
 assert.equal(context.module.exports.status().status,'error');
 assert.match(context.module.exports.status().message,/update service could not start/i);
});
