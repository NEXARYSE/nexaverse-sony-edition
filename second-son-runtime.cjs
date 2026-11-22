const fs = require('node:fs/promises');
const path = require('node:path');

const GAMES = new Map([
  ['infamous-second-son', { label: 'inFAMOUS Second Son', titleIds: new Set(['CUSA00004', 'CUSA00223']), titlePattern: /inFAMOUS\s+Second\s+Son/i }],
  ['infamous-first-light', { label: 'inFAMOUS First Light', acceptAnyPs4Title: true }],
  ['killzone-shadow-fall', { label: 'Killzone: Shadow Fall', titleIds: new Set(['CUSA00002', 'CUSA00129']), titlePattern: /Killzone\s*:\s*Shadow\s+Fall/i }],
  ['nba-street-vol-2', { label: 'NBA Street Vol. 2', acceptAnyPs4Title: true }],
]);

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
  async function validateDirectory(directory, id = 'infamous-second-son') {
    const game = GAMES.get(id);
    if (!game) throw Error('This game does not use the bundled shadPS4 emulator.');
    try {
      const root = await fs.realpath(directory);
      const eboot = await fs.stat(path.join(root, 'eboot.bin'));
      const param = path.join(root, 'sce_sys', 'param.sfo');
      const info = await fs.stat(param);
      const titleId = readTitleId(await fs.readFile(param));
      if (eboot.isFile() && eboot.size > 0 && info.isFile() && info.size <= 65536 &&
          (game.acceptAnyPs4Title ? /^[A-Z]{4}\d{5}$/.test(titleId || '') : game.titleIds.has(titleId))) return root;
    } catch {}
    throw Error(game.acceptAnyPs4Title
      ? 'Choose a PS4 game folder containing eboot.bin and sce_sys/param.sfo.'
      : `Choose an ${game.label} PS4 folder containing eboot.bin and sce_sys/param.sfo.`);
  }
  async function launchPlan(game) {
    if (!GAMES.has(game.id)) throw Error('This game does not use the bundled shadPS4 emulator.');
    const directory = await validateDirectory(game.gameDirectory, game.id);
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

module.exports = {GAME_ID: 'infamous-second-son', supports: id => GAMES.has(id), create, readTitleId};
