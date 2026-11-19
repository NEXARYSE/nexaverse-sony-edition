const fs = require('node:fs/promises');
const path = require('node:path');

const GAME_ID = 'infamous-second-son';
const TITLE_IDS = new Set(['CUSA00004', 'CUSA00223']);

function readTitleId(data) {
  if (data.length < 20 || data.readUInt32LE(0) !== 0x46535000) return null;
  const keyStart = data.readUInt32LE(8);
  const valueStart = data.readUInt32LE(12);
  const count = data.readUInt32LE(16);
  if (20 + count * 16 > keyStart || keyStart > valueStart || valueStart > data.length) return null;
  for (let i = 0; i < count; i++) {
    const entry = 20 + i * 16;
    const key = keyStart + data.readUInt16LE(entry);
    if (key + 9 > valueStart || data.toString('ascii', key, key + 9) !== 'TITLE_ID\0') continue;
    const len = data.readUInt32LE(entry + 4);
    const offset = valueStart + data.readUInt32LE(entry + 12);
    if (data.readUInt16LE(entry + 2) !== 0x0204 || len !== 10 || offset + len > data.length) return null;
    if (data[offset + 9] !== 0) return null;
    return data.toString('latin1', offset, offset + 9);
  }
  return null;
}

function create({runtimeRoot, userData}) {
  const executable = () => path.join(runtimeRoot, 'shadPS4.exe');
  async function validateDirectory(directory) {
    try {
      const root = await fs.realpath(directory);
      const eboot = await fs.stat(path.join(root, 'eboot.bin'));
      const param = path.join(root, 'sce_sys', 'param.sfo');
      const info = await fs.stat(param);
      if (eboot.isFile() && eboot.size > 0 && info.isFile() && info.size <= 65536 &&
          TITLE_IDS.has(readTitleId(await fs.readFile(param)))) return root;
    } catch {}
    throw Error('Choose an inFAMOUS Second Son PS4 folder (CUSA00004 or CUSA00223) containing eboot.bin and sce_sys/param.sfo.');
  }
  async function launchPlan(game) {
    if (game.id !== GAME_ID) throw Error('This emulator is reserved for inFAMOUS Second Son.');
    const directory = await validateDirectory(game.gameDirectory);
    const exe = executable();
    try {
      if (!(await fs.stat(exe)).isFile()) throw Error('Invalid runtime');
    } catch {
      throw Error('The bundled inFAMOUS Second Son emulator is missing.');
    }
    // The Windows fork resolves its portable profile and defaults beside the EXE.
    const cwd = path.join(userData, 'shadps4-second-son');
    const localExe = path.join(cwd, 'shadPS4.exe');
    const ready = path.join(cwd, '.nexaverse-bundle-1.1.1');
    const staged = await fs.access(ready).then(() => true, () => false);
    if (!staged) {
      await fs.cp(runtimeRoot, cwd, {recursive: true, force: false});
      await fs.mkdir(path.join(cwd, 'user'), {recursive: true});
      await fs.writeFile(ready, '');
    }
    // Refresh the executable from the bundle while preserving existing portable saves.
    await fs.copyFile(exe, localExe);
    return {exe: localExe, cwd, args: ['--game', path.join(directory, 'eboot.bin')]};
  }
  return {executable, validateDirectory, launchPlan};
}

module.exports = {GAME_ID, create, readTitleId};
