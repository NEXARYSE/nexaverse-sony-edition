const sonyTrophyBrowser = (() => {
  async function showGameTrophies(game,options={}) {
    const readOnly=options.readOnly===true;
    const root = modal(game.name + ' · Trophies');
    document.querySelector('#panel').classList.add('trophy-dialog');
    const data = await api.trophyList(game.id);if(readOnly)data.progress=null;
    if(data.pending){paragraph(root,'The in-app trophy list is not available yet.');if(data.source){const link=block('button','trophy-source','View PlayStation trophy list');link.onclick=()=>api.openTrophySource(game.id);root.append(link)}return;}
    if (!root.isConnected) return;
    const counts = block('div', 'profile-small-tiers');
    for (const tier of (data.nintendoAchievements?['nintendo']:['platinum', 'gold', 'silver', 'bronze', 'steam'])) {
      const item = block('div');
      item.append(image('assets/trophy-' + tier + (tier==='steam'?'.webp':'.png'), tier + ' trophy'), block('span', '', data.trophies.filter(t => t.tier === tier).length));
      counts.append(item);
    }
    root.append(counts);
    paragraph(root, data.nintendoAchievements?'Community achievements from Completionism.ca · '+data.trophies.length+' challenges. Earned status has not been verified.':'Trophies & Steam achievements · ' + data.trophies.length + ' trophies. '+(data.progress?'Steam verified matches · '+data.progress.count+' earned · '+data.progress.mapped.length+' matched. These are NEXAVERSE imports, not changes to your PlayStation account.':'Earned status has not been verified.'));
    const source = block('button', 'trophy-source', 'View original achievement source');
    source.onclick = () => api.openTrophySource(game.id);
    root.append(source);if(data.playstationSource){const ps=block('button','trophy-source','View PlayStation trophy list');ps.onclick=()=>api.openTrophySource(game.id,'playstation');root.append(ps)}
    const status = block('div', 'save-monitor-status'); root.append(status);
    async function check(force) {
      const state = await api.saveStatus(game.id, force);
      if (!status.isConnected) return;
      status.replaceChildren();
      paragraph(status, state.message);
      if (state.checkedAt) paragraph(status, 'Last checked: ' + new Date(state.checkedAt).toLocaleTimeString() + ' · ' + state.candidates + ' save candidates');
      if (state.limited) paragraph(status, 'Scan limit reached. Choose the specific save folder for a focused check.');
      if (state.unreadable) paragraph(status, 'Some files could not be read. Close the game and check again.');
    }
    const controls = block('div', 'trophy-save-controls');
    if(!readOnly&&data.steamSupported!==false)action(controls,'Sync Steam achievements',async()=>{try{await api.steamProgress(game.id,true);await reload();await showGameTrophies(games.find(g=>g.id===game.id)||game)}catch(e){toast(e.message)}});
    if(!readOnly&&!data.nintendoAchievements&&!data.progress)action(controls, 'Check saves now', () => check(true).catch(e => toast(e.message)));
    if(!readOnly&&!data.nintendoAchievements&&!data.progress)action(controls, 'Choose save folder', async () => {
      try { if (await api.selectSaveDirectory(game.id)) await check(false); } catch (e) { toast(e.message); }
    });
    root.append(controls);
    const filter = block('div', 'trophy-filters'), search = document.createElement('input'), select = document.createElement('select');
    search.placeholder = 'Search trophies'; search.setAttribute('aria-label', 'Search trophies');
    select.setAttribute('aria-label', 'Filter trophy tier');
    for (const tier of (data.nintendoAchievements?['all','nintendo']:['all', 'platinum', 'gold', 'silver', 'bronze', 'steam'])) {
      const option = document.createElement('option'); option.value = tier; option.textContent = tier === 'all' ? 'All tiers' : tier[0].toUpperCase() + tier.slice(1); select.append(option);
    }
    filter.append(search, select); root.append(filter);
    const list = block('div', 'trophy-list'); root.append(list);
    function renderList() {
      list.replaceChildren();
      const trophies = data.trophies.filter(t => (select.value === 'all' || t.tier === select.value) && t.name.toLowerCase().includes(search.value.toLowerCase()));
      for (const trophy of trophies) {
        const row = block('button', 'trophy-list-row'); row.type = 'button';
        row.append(image('assets/trophy-' + trophy.tier + (trophy.tier==='steam'?'.webp':'.png'), trophy.tier + ' trophy'));
        const copy = block('div'); copy.append(block('strong', '', trophy.name), block('small', '', trophy.tier==='nintendo'?'Nintendo community achievement':trophy.tier==='steam'?'Steam achievement':trophy.tier[0].toUpperCase() + trophy.tier.slice(1)));
        if(trophy.description)copy.append(block('small','',trophy.description));
        row.append(copy, block('span',data.progress?.earned[trophy.id]?'trophy-earned':'trophy-unverified',data.progress?.earned[trophy.id]?(data.progress.earned[trophy.id].source==='admin'?'Earned · NEXAVERSE award':'Earned · Steam'):data.progress?.mapped.includes(trophy.id)?'Not earned on Steam':'Not verified'));
        row.title = 'View original achievement requirements';
        row.onclick = () => api.openTrophySource(game.id, trophy.id);
        list.append(row);
      }
      if (!trophies.length) paragraph(list, 'No matching trophies.');
    }
    search.oninput = select.onchange = renderList; renderList();
    if(!readOnly&&!data.nintendoAchievements&&!data.progress)check(false).catch(e => paragraph(status, e.message));
    const poll = setInterval(() => {
      if (readOnly || data.nintendoAchievements || data.progress || !root.isConnected || !document.querySelector('#panel').open) return clearInterval(poll);
      check(false).catch(() => {});
    }, 30000);
  }
  document.querySelector('#gameTrophyCard').onclick = () => {
    const game = games.find(g => g.id === selected);
    if (game) showGameTrophies(game).catch(e => toast(e.message));
  };
  document.querySelector('#trophyCard').onclick = () => {
    const root = modal('Your trophies');
    paragraph(root, 'Browse trophy lists by game. Totals include verified Steam achievements and NEXAVERSE admin awards.');
    for (const game of games) action(root, game.name, () => showGameTrophies(game).catch(e => toast(e.message)));
  };
  return {showGame: showGameTrophies};
})();
