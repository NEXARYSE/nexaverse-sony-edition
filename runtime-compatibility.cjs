const os=require('node:os');
function checkHost({platform=process.platform,arch=process.arch,machine=os.machine(),release=os.release()}={}){
 if(platform!=='win32')throw Error('NBA 2K22 NEXT-GEN requires Windows 10 or 11 on an x64 PC.');
 if(arch!=='x64'||/arm/i.test(machine))throw Error('This runtime currently requires an x64 PC. Windows on ARM is not verified.');
 const version=/^(\d+)\.(\d+)\.(\d+)/.exec(release);
 if(!version||Number(version[1])<10)throw Error('Update to Windows 10 or 11 before launching NBA 2K22 NEXT-GEN.');
 return {platform,arch,windowsBuild:Number(version[3]),graphicsVerified:false};
}
module.exports={checkHost};
