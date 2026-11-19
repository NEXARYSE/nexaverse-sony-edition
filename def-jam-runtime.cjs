const fs = require('node:fs/promises');
const path = require('node:path');

const GAME_ID = 'def-jam-fight-for-ny';

async function validateIso(file) {
  try {
    const resolved = await fs.realpath(file);
    const info = await fs.stat(resolved);
    if (info.isFile() && info.size > 0 && path.extname(resolved).toLowerCase() === '.iso') return resolved;
  } catch {}
  throw Error('Choose an existing Def Jam: Fight for NY .iso file.');
}

function create({runtimeRoot}) {
  const executable = () => path.join(runtimeRoot, 'DefJamSetup.exe');
  async function launchPlan(game) {
    if (game.id !== GAME_ID) throw Error('This launcher integration only accepts Def Jam: Fight for NY.');
    const iso = await validateIso(game.isoFile);
    const exe = executable();
    const info = await fs.stat(exe).catch(() => null);
    if (!info?.isFile()) throw Error('The bundled Def Jam launcher is missing.');
    return {exe, cwd:path.dirname(exe), args:[], env:{...process.env, NEXAVERSE_DEF_JAM_ISO:iso}};
  }
  return {executable, validateIso, launchPlan};
}

module.exports = {GAME_ID, create, validateIso};
