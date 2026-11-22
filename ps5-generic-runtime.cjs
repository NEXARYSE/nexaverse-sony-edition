const fs = require('node:fs/promises');
const path = require('node:path');
const { checkHost } = require('./runtime-compatibility.cjs');

const GAMES = new Map([
  ['ufc-6', { label: 'EA SPORTS UFC 6', titlePattern: /UFC\s*6/i }],
  ['marvels-wolverine', { label: 'Marvel’s Wolverine', acceptAnyPs5Title: true }],
  ['ghost-of-yotei', { label: 'Ghost of Yōtei', acceptAnyPs5Title: true }],
  ['mlb-the-show-26', { label: 'MLB The Show 26', acceptAnyPs5Title: true }],
  ['nhl-27', { label: 'NHL 27', acceptAnyPs5Title: true }],
]);

async function validateDirectory(directory, id = 'ufc-6') {
  const game = GAMES.get(id);
  if (!game) throw Error('This game does not use the bundled PS5 runtime.');
  try {
    if (typeof directory !== 'string' || !path.isAbsolute(directory)) throw Error();
    const root = await fs.realpath(directory);
    const eboot = await fs.stat(path.join(root, 'eboot.bin'));
    const metadata = path.join(root, 'sce_sys', 'param.json');
    const info = await fs.stat(metadata);
    if (!eboot.isFile() || !info.isFile() || info.size > 1048576) throw Error();
    const param = JSON.parse(await fs.readFile(metadata, 'utf8'));
    const titles = Object.values(param.localizedParameters || {}).filter(value => value && typeof value === 'object').map(value => String(value.titleName || ''));
    if (!/^PPSA\d{5}$/.test(param.titleId || '') || (!game.acceptAnyPs5Title && !titles.some(title => game.titlePattern.test(title)))) throw Error();
    return root;
  } catch {
    throw Error(game.acceptAnyPs5Title
      ? 'Choose a PS5 game folder containing eboot.bin and sce_sys/param.json.'
      : `Choose a ${game.label} PS5 game folder containing eboot.bin and sce_sys/param.json.`);
  }
}

function create({ runtimeRoot, userData, verifyHost = checkHost }) {
  const executable = () => path.join(runtimeRoot, 'kyty_emulator.exe');
  async function launchPlan(game) {
    if (!GAMES.has(game.id)) throw Error('This game does not use the bundled PS5 runtime.');
    verifyHost();
    const directory = await validateDirectory(game.gameDirectory, game.id);
    const exe = executable();
    await fs.access(exe);
    const cwd = path.join(userData, 'ps5-kyty', game.id);
    await fs.mkdir(cwd, { recursive: true });
    return { exe, cwd, windowsHide: true, args: ['--game', directory] };
  }
  return { executable, validateDirectory, launchPlan };
}

module.exports = { GAME_ID: 'ufc-6', supports: id => GAMES.has(id), create, validateDirectory };
