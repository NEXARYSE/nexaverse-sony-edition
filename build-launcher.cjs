const config=require('./package.json');
require('C:/Users/tratr/Documents/Codex/2026-11-07/i-x20/work/launcher-source-2/node_modules/electron-builder').build({projectDir:process.cwd(),publish:'never'}).then(()=>console.log('BUILD COMPLETE '+config.version)).catch(e=>{console.error(e);process.exitCode=1});
