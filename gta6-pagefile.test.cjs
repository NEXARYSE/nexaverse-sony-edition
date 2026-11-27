const {test}=require('node:test'),a=require('node:assert/strict'),{plan,create,setupScript}=require('./gta6-pagefile.cjs'),G=1024**3;
const low={limit:40*G,committed:20*G,automatic:true,systemDrive:'C:',drives:[{letter:'C:',free:10*G},{letter:'D:',free:100*G}],usage:[{name:'C:\\pagefile.sys',mb:8192}],settings:[]};
test('full Windows drive automatically selects a secondary local drive',()=>{const p=plan(low);a.equal(p.drive,'D:');a.equal(p.initialMB,25600);a.equal(p.growth,25*G)});
test('current pagefile on secondary drive is increased, never shrunk',()=>{const p=plan({...low,usage:[{name:'D:\\pagefile.sys',mb:16384}],settings:[{name:'D:\\pagefile.sys',initial:16384,maximum:65536}]});a.equal(p.initialMB,41984);a.equal(p.maximumMB,65536)});
test('no eligible drive produces accurate disk-space failure',()=>{a.throws(()=>plan({...low,drives:[{letter:'C:',free:G}]}),/No suitable local drive/)});
test('pending paging capacity avoids repeated elevation',async()=>{const s={...low,automatic:false,settings:[{name:'D:\\pagefile.sys',initial:25600,maximum:25600}]};const r=await create({query:async()=>s,configure:async()=>a.fail('elevation')}).setup();a.equal(r.ready,false);a.match(r.message,/Restart/)});
test('setup button returns a reboot result without launching game',async()=>{let n=0;const r=await create({query:async()=>n++?{...low,automatic:false,settings:[{name:'D:\\pagefile.sys',initial:25600,maximum:25600}]}:low,configure:async p=>a.equal(p.drive,'D:')}).setup();a.equal(r.ready,false);a.match(r.message,/D:/)});
test('Play can proceed with sufficient active headroom despite pending capacity',async()=>{const s={...low,automatic:false,settings:[{name:'D:\\pagefile.sys',initial:25600,maximum:25600}]};a.equal(await create({query:async()=>s,configure:async()=>a.fail('Play must never configure paging files')}).beforeLaunch(),true)});
test('real Windows setup error is not replaced by generic cancellation',async()=>{await a.rejects(create({query:async()=>low,configure:async()=>{throw Error('Windows failure 0x80041003')}}).setup(),/0x80041003/)});
test('adequate capacity never alters paging files',async()=>{await create({query:async()=>({...low,limit:70*G}),configure:async()=>a.fail('elevation')}).beforeLaunch()});
test('elevated script uses privileged WMI and rejects injected drive names',()=>{a.match(setupScript(plan(low)),/EnablePrivileges=\$true/);a.throws(()=>setupScript({drive:"D:';bad",initialMB:100}),/Invalid/)});
test('generated WMI scope is the local CIMV2 path',()=>{const text=setupScript(plan(low)),line=text.split('\n').find(l=>l.includes('ManagementScope'));const expected=String.fromCharCode(92).repeat(2)+'.'+String.fromCharCode(92)+'root'+String.fromCharCode(92)+'cimv2';a.equal(line.split("'")[1],expected)});

test('Play checks active headroom and never configures memory automatically',async()=>{
 await a.rejects(create({query:async()=>({...low,limit:70*G,committed:63*G}),configure:async()=>a.fail('elevation')}).beforeLaunch(),/8 GiB/);
 a.equal(await create({query:async()=>({...low,limit:32*G,committed:24*G}),configure:async()=>a.fail('elevation')}).beforeLaunch(),true);
});
test('Play fails closed when Windows memory cannot be verified',async()=>{
 for(const state of [{limit:NaN,committed:0},{limit:40*G,committed:-1},{limit:40*G,committed:41*G}])await a.rejects(create({query:async()=>state}).beforeLaunch(),/Could not verify/);
 await a.rejects(create({query:async()=>{throw Error('CIM unavailable')}}).beforeLaunch(),/CIM unavailable/);
});
test('explicit drive moves all paging capacity to D while preserving the commit target',()=>{
 const s={...low,automatic:false,limit:47*G,usage:[{name:'C:\\pagefile.sys',mb:15360}],settings:[{name:'C:\\pagefile.sys',initial:15360,maximum:15360},{name:'D:\\pagefile.sys',initial:19456,maximum:19456}]};
 const p=plan(s,'D:');a.equal(p.drive,'D:');a.equal(p.moveToDrive,true);a.equal(p.initialMB,34816);a.equal(p.growth,p.initialMB*1024**2);
 a.equal(plan({...s,settings:[{name:'D:\\pagefile.sys',initial:p.initialMB,maximum:p.initialMB}]},'D:').pending,true);
 for(const value of ['E:','D:;bad',{},null])a.throws(()=>plan(s,value),/drive|Select/);
 a.throws(()=>plan({...s,drives:[{letter:'D:',free:G}]},'D:'),/Not enough free space/);
 const script=setupScript(p);a(script.indexOf('did not retain the paging-file settings')<script.indexOf('[void]$other.Delete()'));a.match(script,/foreach\(\$record in \$removed\)/);a.match(script,/Explicit drive selection/);
});
test('explicit drive setup preserves other-drive capacity until target configuration verifies',async()=>{
 let applied;const before={...low,automatic:false,settings:[{name:'C:\\pagefile.sys',initial:8192,maximum:8192}]};let n=0;
 const result=await create({query:async()=>n++?{...before,settings:[{name:'D:\\pagefile.sys',initial:33792,maximum:33792}]}:before,configure:async p=>applied=p}).setup('D:');
 a.equal(applied.moveToDrive,true);a.equal(result.ready,false);a.match(result.message,/Restart Windows/);a.match(result.message,/other drives/);
});


test('D activating before restart does not count retired C twice or grow D again',async()=>{
 const s={...low,automatic:false,limit:80.725*G,usage:[{name:'C:\\pagefile.sys',mb:15360},{name:'D:\\pagefile.sys',mb:34816}],settings:[{name:'D:\\pagefile.sys',initial:34816,maximum:34816}]};
 a.equal(plan(s,'D:').pending,true);const result=await create({query:async()=>s,configure:async()=>a.fail('must not grow again')}).setup('D:');a.equal(result.ready,false);a.match(result.message,/Restart/);
});
