const fs = require('node:fs/promises');
const path = require('node:path');
const { execFile } = require('node:child_process');

function create({ runtimeRoot, userData, runFile = execFile }) {
  const logRoot = path.join(userData, 'gta-toolkit', 'logs');
  const outputRoot = path.join(userData, 'gta-toolkit');
  let busy = false;
  const exists = async file => { try { return (await fs.stat(file)).isFile(); } catch { return false; } };
  const executable = () => path.join(runtimeRoot, 'runtime', 'gta6rt.exe');
  async function validateDirectory(directory) {
    if (typeof directory !== 'string' || !path.isAbsolute(directory)) throw Error('Link your GTA VI Xbox package folder first.');
    const real = await fs.realpath(directory);
    if (!(await fs.stat(real)).isDirectory()) throw Error('Choose a GTA VI Xbox package folder.');
    return real;
  }
  async function launchPlan(game) {
    const directory = await validateDirectory(game.gameDirectory);
    const image = path.join(directory, 'game_scarlett_master.exe');
    if (!await exists(image)) throw Error('The linked GTA VI folder is missing game_scarlett_master.exe. Game startup is experimental.');
    await fs.access(executable());
    await fs.mkdir(outputRoot, { recursive: true });
    return { exe: executable(), args: ['--run', image, '--game-dir', directory, '--overlay', path.join(outputRoot, 'overlay')], cwd: outputRoot, windowsHide: true };
  }
  async function status(game = {}) {
    return { directory: game.gameDirectory || '', available: await exists(path.join(runtimeRoot, 'runtime', 'gta6rt.exe')),
      executablePresent: game.gameDirectory ? await exists(path.join(game.gameDirectory, 'game_scarlett_master.exe')) : false,
      busy, logs: logRoot };
  }
  async function run(action) {
    const plans = {
      demo: ['runtime/gta6rt.exe', ['--run', path.join(runtimeRoot, 'demo', 'tiny_game.exe'), '--overlay', path.join(outputRoot, 'overlay'), '--', 'hello', '42']],
      warp: ['xbox2windows/offscreen_check.exe', ['warp', path.join(outputRoot, 'triangle-warp.bmp')]],
      gpu: ['xbox2windows/offscreen_check.exe', ['hardware', path.join(outputRoot, 'triangle-hardware.bmp')]],
    };
    if (!Object.hasOwn(plans, action)) throw Error('Unknown toolkit action.');
    if (busy) throw Error('A toolkit test is already running.');
    busy = true;
    try {
      await fs.mkdir(logRoot, { recursive: true });
      const [relative, args] = plans[action];
      const exe = path.join(runtimeRoot, relative);
      await fs.access(exe);
      const result = await new Promise(resolve => runFile(exe, args,
        { cwd: outputRoot, windowsHide: true, timeout: 60000, maxBuffer: 256 * 1024 },
        (error, stdout, stderr) => resolve({ ok: !error, exitCode: error ? error.code ?? 'error' : 0,
          output: String(stdout || '') + String(stderr || '') + (error ? '\n' + error.message : '') })));
      // Fixed filenames bound log storage; synthetic tests contain no game data.
      await fs.writeFile(path.join(logRoot, action + '.txt'), new Date().toISOString() + '\n' + result.output, 'utf8');
      return result;
    } finally { busy = false; }
  }
  return { status, run, executable, validateDirectory, launchPlan, runtimeRoot, logRoot };
}

function register({ app, ipcMain, shell, dialog, getWindow, read, edit }) {
  const runtimeRoot = app.isPackaged ? path.join(process.resourcesPath, 'gta-toolkit') : path.join(__dirname, 'runtime', 'gta-toolkit');
  const toolkit = create({ runtimeRoot, userData: app.getPath('userData') });
  ipcMain.handle('gta-toolkit-status', async () => toolkit.status((await read()).find(g => g.id === 'gta-vi')));
  ipcMain.handle('gta-toolkit-run', (_, action) => toolkit.run(action));
  ipcMain.handle('gta-toolkit-link', async () => {
    const current = (await read()).find(g => g.id === 'gta-vi');
    const result = await dialog.showOpenDialog(getWindow(), { title: 'Link GTA VI Xbox package folder',
      ...(current?.gameDirectory ? { defaultPath: current.gameDirectory } : {}), properties: ['openDirectory'] });
    if (result.canceled) return false;
    const directory = await fs.realpath(result.filePaths[0]);
    if (!(await fs.stat(directory)).isDirectory()) throw Error('Choose a folder.');
    await edit(items => { const game = items.find(g => g.id === 'gta-vi'); if (!game) throw Error('Game not found.'); game.gameDirectory = directory; });
    getWindow()?.webContents.send('library-changed');
    return true;
  });
  ipcMain.handle('gta-toolkit-open', async (_, target) => {
    if (target !== 'logs' && target !== 'tools') throw Error('Unknown toolkit location.');
    const directory = target === 'logs' ? toolkit.logRoot : toolkit.runtimeRoot;
    if (target === 'logs') await fs.mkdir(directory, { recursive: true });
    const error = await shell.openPath(directory);
    if (error) throw Error(error);
  });
}
module.exports = { create, register };
