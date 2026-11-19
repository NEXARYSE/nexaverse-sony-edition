const fs = require('node:fs/promises');
const path = require('node:path');
const { checkHost } = require('./runtime-compatibility.cjs');

const GAME_ID = 'ufc-6';

async function validateDirectory(directory) {
  try {
    if (typeof directory !== 'string' || !path.isAbsolute(directory)) throw Error();
    const root = await fs.realpath(directory);
    const eboot = await fs.stat(path.join(root, 'eboot.bin'));
    const metadata = path.join(root, 'sce_sys', 'param.json');
    const info = await fs.stat(metadata);
    if (!eboot.isFile() || !info.isFile() || info.size > 1048576) throw Error();
    const param = JSON.parse(await fs.readFile(metadata, 'utf8'));
    const titles = Object.values(param.localizedParameters || {}).filter(value => value && typeof value === 'object').map(value => String(value.titleName || ''));
    if (!/^PPSA\d{5}$/.test(param.titleId || '') || !titles.some(title => /UFC\s*6/i.test(title))) throw Error();
    return root;
  } catch {
    throw Error('Choose an EA SPORTS UFC 6 PS5 game folder containing eboot.bin and sce_sys/param.json.');
  }
}

function create({ runtimeRoot, userData, verifyHost = checkHost }) {
  const executable = () => path.join(runtimeRoot, 'kyty_emulator.exe');
  async function launchPlan(game) {
    if (game.id !== GAME_ID) throw Error('This PS5 integration only accepts EA SPORTS UFC 6.');
    verifyHost();
    const directory = await validateDirectory(game.gameDirectory);
    const exe = executable();
    await fs.access(exe);
    const cwd = path.join(userData, 'ps5-kyty', GAME_ID);
    await fs.mkdir(cwd, { recursive: true });
    return { exe, cwd, windowsHide: true, args: ['--game', directory, '--screen-width', '1920', '--screen-height', '1080', '--vblank-frequency', '60', '--present-mode', 'Mailbox', '--frame-rate-limit', '0', '--no-splash', '--shader-cache'] };
  }
  return { executable, validateDirectory, launchPlan };
}

module.exports = { GAME_ID, create, validateDirectory };
