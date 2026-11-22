window.openGtaToolkit = async function () {
  const root = modal('Grand Theft Auto VI · Toolkit');
  paragraph(root, 'Experimental GTA VI toolkit. Full-game startup and gameplay have not been verified.');
  const linked = paragraph(root, 'Loading linked folder…', 'path');
  const stateLine = paragraph(root, '');
  const controls = document.createElement('div');
  controls.className = 'gta-toolkit-actions';
  root.append(controls);
  const output = document.createElement('pre');
  output.className = 'gta-toolkit-output';
  output.setAttribute('aria-live', 'polite');
  root.append(output);
  const buttons = [];
  let available = false;
  async function refresh() {
    const state = await window.gtaToolkit.status();
    available = state.available;
    linked.textContent = state.directory || 'No Xbox package folder linked.';
    stateLine.textContent = !state.available ? 'Toolkit files are missing. Reapply the toolkit update.' : state.directory
      ? (state.executablePresent ? 'Game executable present. Game startup remains unverified.' : 'Package folder linked. The expected game executable is absent.')
      : 'Link a folder to remember its location. Tests work without game files.';
    buttons.forEach(button => { button.disabled = !available || state.busy; });
    launch.disabled = !available || !state.directory || state.busy;
  }
  action(controls, 'Link Xbox folder', async () => { try { if (await window.gtaToolkit.link()) { await reload(); await refresh(); } } catch (error) { toast(error.message); } });
  const launch = action(controls, 'Attempt experimental launch', async () => {
    launch.disabled = true;
    try { await api.launch('gta-vi'); output.textContent = 'Runtime started. This does not confirm successful game startup.'; await reload(); }
    catch (error) { output.textContent = error.message; }
    finally { await refresh(); }
  });
  launch.disabled = true;
  async function run(name) {
    buttons.forEach(button => { button.disabled = true; });
    output.textContent = 'Running synthetic test…';
    try {
      const result = await window.gtaToolkit.run(name);
      output.textContent = (result.ok ? 'Test passed' : 'Test failed') + ' · exit ' + result.exitCode + '\n\n' + result.output;
    } catch (error) { output.textContent = error.message; }
    finally { await refresh().catch(error => { output.textContent += '\n' + error.message; }); }
  }
  buttons.push(action(controls, 'Loader demo', () => run('demo')));
  buttons.push(action(controls, 'Software GPU test', () => run('warp')));
  buttons.push(action(controls, 'Hardware GPU test', () => run('gpu')));
  action(controls, 'Open logs', () => window.gtaToolkit.open('logs').catch(error => toast(error.message)));
  action(controls, 'Open all tools', () => window.gtaToolkit.open('tools').catch(error => toast(error.message)));
  paragraph(root, 'Uses the existing game folder without copying it. Tests overwrite their small output files and logs. Asset extraction and shader generation stay manual.');
  await refresh().catch(error => { output.textContent = error.message; });
};

const renderBeforeGtaToolkit = render;
render = function () {
  renderBeforeGtaToolkit();
  const game = games.find(item => item.id === selected);
  if (game?.id !== 'gta-vi') return;
  $('#heroDescription').textContent = game.gameDirectory ? 'Experimental toolkit · Xbox folder linked' : 'Link your GTA VI Xbox folder to enable launch attempts.';
  $('#heroAction').textContent = 'Open toolkit';
  $('#heroAction').disabled = false;
  $('#gameLinkStatus').textContent = game.gameDirectory ? 'Xbox package folder linked · Startup unverified' : 'Link your GTA VI Xbox folder in the toolkit.';
};
const heroBeforeGtaToolkit = $('#heroAction').onclick;
$('#heroAction').onclick = function (...args) { return selected === 'gta-vi' ? window.openGtaToolkit() : heroBeforeGtaToolkit.apply(this, args); };
const detailsBeforeGtaToolkit = $('#details').onclick;
$('#details').onclick = function (...args) { return selected === 'gta-vi' ? window.openGtaToolkit() : detailsBeforeGtaToolkit.apply(this, args); };
const libraryBeforeGtaToolkit = openLibraryGame;
openLibraryGame = function (game) { return game.id === 'gta-vi' ? window.openGtaToolkit() : libraryBeforeGtaToolkit(game); };
const launchBeforeGtaToolkit = launchLibraryGame;
launchLibraryGame = function (game) { return game.id === 'gta-vi' ? window.openGtaToolkit() : launchBeforeGtaToolkit(game); };
