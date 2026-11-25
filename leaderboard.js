(() => {
  let generation = 0;
  function row(user, rank) {
    const entry = block('button', 'leaderboard-row'); entry.type = 'button';
    entry.setAttribute('aria-label', 'View ' + user.name + ' profile, rank ' + rank);
    const place = block('span', 'leaderboard-rank');
    if (rank <= 4) place.append(image('assets/trophy-' + ['platinum', 'gold', 'silver', 'bronze'][rank - 1] + '.png', 'Rank ' + rank));
    else place.textContent = String(rank);
    const avatar = block('span', 'leaderboard-avatar');
    if (user.avatar) avatar.append(image(user.avatar, '')); else avatar.textContent = (user.name || 'N')[0];
    const identity = block('span', 'leaderboard-identity'); identity.append(block('strong', '', user.name), block('small', '', '@' + user.username));
    const time = block('span', 'leaderboard-time', timeText(user.seconds));
    const tiers = block('span', 'leaderboard-tiers');
    for (const tier of ['platinum', 'gold', 'silver', 'bronze', 'steam', 'xbox', 'nintendo']) {
      const extension = tier === 'steam' ? '.webp' : (tier === 'xbox' || tier === 'nintendo' ? '.svg' : '.png');
      const label = tier === 'xbox' ? 'Xbox achievements' : (tier === 'nintendo' ? 'Nintendo achievements' : tier);
      const count = block('span', 'leaderboard-tier'); count.append(image('assets/trophy-' + tier + extension, label), block('span', '', String(user.tiers?.[tier] || 0))); tiers.append(count);
    }
    entry.append(place, avatar, identity, time, tiers);
    entry.onclick = () => {
      if (user.id === discordAccount?.id) { viewedFriend = null; visitorCommunity = null; profileTab = 'overview'; changePage('profile'); loadOwnCommunity(); }
      else showFriendProfile({ ...user, source: 'leaderboard' });
    };
    return entry;
  }
  async function show() {
    const request = ++generation, content = $('#leaderboardContent'); content.replaceChildren();
    const top = block('div', 'leaderboard-heading'); top.append(block('h1', '', 'Leaderboard'));
    const refresh = block('button', 'gallery-text-button', '↻ Refresh'); refresh.setAttribute('aria-label', 'Refresh leaderboard'); refresh.onclick = show;
    top.append(refresh); content.append(top, block('p', 'leaderboard-description', 'Top 10 · Total game playtime, then trophies and achievements earned'));
    const body = block('div', 'leaderboard-body'); content.append(body);
    if (!discordAccount) { paragraph(body, 'Link Discord to view the NEXAVERSE leaderboard.'); action(body, 'Link Discord', linkDiscord); return; }
    refresh.disabled = true; paragraph(body, 'Loading leaderboard…');
    try {
      await syncOwnProfile();
      const data = await api.leaderboard();
      if (request !== generation || activePage !== 'leaderboard') return;
      body.replaceChildren();
      const labels = block('div', 'leaderboard-columns'); labels.append(block('span', '', 'Rank'), block('span', '', 'Player'), block('span', '', 'Total playtime'), block('span', '', 'Trophies / platform achievements')); body.append(labels);
      if (!data.users?.length) paragraph(body, 'No rankings yet. Play a linked game to start recording your activity.');
      for (const [index, user] of (data.users || []).slice(0, 10).entries()) body.append(row(user, index + 1));
      const date = Number.isFinite(data.updatedAt) ? new Date(data.updatedAt).toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' }) : '';
      content.append(block('p', 'leaderboard-footnote', 'Game time recorded by NEXAVERSE · Trophy and achievement slots include PlayStation, Steam, Xbox, and Nintendo' + (date ? ' · Updated ' + date : '')));
    } catch (error) { if (request === generation && activePage === 'leaderboard') { body.replaceChildren(); paragraph(body, 'Leaderboard unavailable. ' + error.message); } }
    finally { refresh.disabled = false; }
  }
  window.nexaverseLeaderboard = { show };
})();
