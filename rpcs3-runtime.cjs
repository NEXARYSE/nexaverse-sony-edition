const fs = require('node:fs/promises');
const {createReadStream} = require('node:fs');
const path = require('node:path');
const {spawn} = require('node:child_process');
const {createHash} = require('node:crypto');
const {readTitleId} = require('./second-son-runtime.cjs');

const titles = new Map([
  ['infamous', new Set(['BCUS98119', 'BCES00220', 'BCJS30035', 'NPUA80026', 'NPEA00376'])],
  ['infamous-2', new Set(['BCUS98125', 'BCES00900', 'BCJS30054', 'NPUA80698', 'NPEA00375'])],
  ['the-sly-collection', new Set(['BCUS98246', 'BCES00968', 'BCJS30061'])],
  ['jak-3', new Set(['BCUS98281', 'BCES01325', 'BCAS20222'])],
  ['littlebigplanet-3', new Set(['BCUS98362', 'BCES01663', 'BCES02068', 'BCJS30095', 'NPUA81116', 'NPEA00515'])],
  ['fight-night-champion', new Set(['BLUS30608', 'BLES01039', 'BLJM60267', 'NPUB30595', 'NPEB00730', 'BLAS50313'])],
  ['killzone-3', new Set(['BCUS98234', 'BCES01007'])],
  ['naughty-bear', new Set(['BLUS30439', 'BLES00945'])],
]);
const firmwareBytes = 206197916;
const firmwareHash = '158471fd834f8ea8036136b6aab43cd86c7ba73d79ca30e0af3c0fe0001cf365';

async function verifyFirmware(file) {
  if ((await fs.stat(file)).size !== firmwareBytes) throw Error('The bundled PS3 firmware has an unexpected size.');
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(file)) hash.update(chunk);
  if (hash.digest('hex') !== firmwareHash) {
    throw Error('The bundled PS3 firmware failed its integrity check.');
  }
}

function create({runtimeRoot, userData}) {
  const cwd = path.join(userData, 'rpcs3');
  const executable = () => path.join(runtimeRoot, 'rpcs3.exe');
  let preparing;
  async function validateDirectory(directory, id) {
    const allowed = titles.get(id);
    if (!allowed) throw Error('This game does not use the bundled PS3 emulator.');
    try {
      const root = await fs.realpath(directory);
      for (const folder of [root, path.join(root, 'PS3_GAME')]) {
        const param = path.join(folder, 'PARAM.SFO');
        const boot = path.join(folder, 'USRDIR', 'EBOOT.BIN');
        const info = await fs.stat(param).catch(() => null);
        if (!info?.isFile() || info.size > 65536) continue;
        const eboot = await fs.stat(boot).catch(() => null);
        if (eboot?.isFile() && eboot.size > 0 && allowed.has(readTitleId(await fs.readFile(param)))) return folder;
      }
    } catch {}
    throw Error('Choose the matching PS3 game folder containing PARAM.SFO and USRDIR/EBOOT.BIN.');
  }
  async function firmwareInstalled() {
    try {
      const version = await fs.readFile(path.join(cwd, 'dev_flash', 'vsh', 'etc', 'version.txt'), 'utf8');
      await fs.access(path.join(cwd, 'dev_flash', 'sys', 'external', 'liblv2.sprx'));
      return /^release:\d+\.\d+/m.test(version);
    } catch { return false; }
  }
  async function prepare() {
    await fs.access(executable());
    const ready = path.join(cwd, '.nexaverse-rpcs3-337fa1c5');
    if (!await fs.access(ready).then(() => true, () => false)) {
      await fs.cp(runtimeRoot, cwd, {recursive:true, force:false, filter:src=>path.basename(src)!=='PS3UPDAT.PUP'});
      await fs.writeFile(ready, '');
    }
    if (await firmwareInstalled()) return;
    const firmware = path.join(runtimeRoot, 'PS3UPDAT.PUP');
    await verifyFirmware(firmware);
    await new Promise((resolve, reject) => {
      const child = spawn(path.join(cwd, 'rpcs3.exe'), ['--headless', '--installfw', firmware], {
        cwd, stdio:'ignore', windowsHide:true, timeout:300000,
      });
      child.once('error', reject);
      child.once('close', code => code === 0 ? resolve() : reject(Error('RPCS3 firmware installation failed.')));
    });
    if (!await firmwareInstalled()) throw Error('RPCS3 did not finish installing PS3 firmware. Try again.');
  }
  async function launchPlan(game) {
    const folder = await validateDirectory(game.gameDirectory, game.id);
    preparing ||= prepare().finally(() => { preparing = undefined; });
    await preparing;
    return {exe:path.join(cwd, 'rpcs3.exe'), cwd, args:['--no-gui', path.join(folder, 'USRDIR', 'EBOOT.BIN')]};
  }
  return {executable, validateDirectory, launchPlan};
}

module.exports = {supports: id => titles.has(id), create};
