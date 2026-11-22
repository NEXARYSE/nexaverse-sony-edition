const fs = require('node:fs/promises');
const path = require('node:path');

const titles = new Set([
  'super-smash-bros-ultimate',
  'zelda-breath-of-the-wild',
  'zelda-tears-of-the-kingdom',
  'kirby-and-the-forgotten-land',
  'super-mario-odyssey',
  'super-mario-galaxy'
]);
const extensions = new Set(['.nsp', '.xci']);

async function validateFile(file) {
  try {
    const resolved = await fs.realpath(file);
    const info = await fs.stat(resolved);
    if (info.isFile() && info.size > 0 && extensions.has(path.extname(resolved).toLowerCase())) return resolved;
  } catch {}
  throw Error('Choose an existing Nintendo Switch game file (.nsp or .xci).');
}

function create({runtimeRoot, userData}) {
  const executable = () => path.join(runtimeRoot, 'yuzu.exe');
  const cwd = path.join(userData, 'yuzu');
  let staging;
  async function stage() {
    await fs.access(executable());
    const marker = path.join(cwd, '.nexaverse-yuzu-v1');
    if (!await fs.access(marker).then(() => true, () => false)) {
      await fs.cp(runtimeRoot, cwd, {recursive:true, force:false});
      await fs.mkdir(path.join(cwd, 'user'), {recursive:true});
      await fs.writeFile(marker, '');
    }
  }
  async function launchPlan(game) {
    if (!titles.has(game.id)) throw Error('This tile does not use Yuzu.');
    const file = await validateFile(game.gameFile);
    staging ||= stage().finally(() => {staging = undefined;});
    await staging;
    return {exe:path.join(cwd, 'yuzu.exe'), cwd, args:['-g', file]};
  }
  return {executable, launchPlan};
}

module.exports = {supports: id => titles.has(id), validateFile, create};
