(() => {
  const choices = new Map();
  let viewerRequest = 0;
  function preferences(owner) {
    try { return JSON.parse(localStorage.getItem('nexaverse-gallery:' + owner)) || {}; } catch { return {}; }
  }
  function favorite(owner, id) {
    const prefs = preferences(owner), ids = new Set(prefs.favorites || []);
    ids.has(id) ? ids.delete(id) : ids.add(id);
    localStorage.setItem('nexaverse-gallery:' + owner, JSON.stringify({ ...prefs, favorites: [...ids] }));
  }
  async function view(owner, item) {
    const request = ++viewerRequest, content = modal('Shared screenshot');
    $('#panel').classList.add('gallery-dialog');
    paragraph(content, 'Loading screenshot…');
    try {
      const src = await api.communityImage(owner, item.id, true);
      if (request !== viewerRequest || !$('#panel').open || !content.isConnected || $('#panelTitle').textContent !== 'Shared screenshot') return;
      content.replaceChildren(image(src, item.caption || 'Shared screenshot'));
    } catch (error) { if (request === viewerRequest && $('#panel').open) content.replaceChildren(block('p', '', error.message)); }
  }
  function render(parent, { own = true, mediaPage = false } = {}) {
    const state = own ? ownCommunity : visitorCommunity, owner = own ? discordAccount?.id : viewedFriend?.id;
    const key = (owner || 'local') + ':' + (mediaPage ? 'media' : 'shared');
    const choice = choices.get(key) || { tab: 'all', sort: 'newest' };
    choices.set(key, choice);
    const section = block('section', 'capture-gallery');
    parent.append(section);
    const top = block('div', 'capture-heading');
    top.append(block('h2', '', mediaPage ? 'Media Gallery' : 'Shared'));
    const actions = block('div', 'capture-actions');
    const refresh = block('button', 'gallery-text-button', '↻ Refresh');
    refresh.setAttribute('aria-label', 'Refresh shared screenshots');
    refresh.onclick = () => own ? loadOwnCommunity() : showFriendProfileShared();
    actions.append(refresh);
    if (own && discordAccount) {
      const upload = block('button', 'gallery-text-button', mediaPage ? 'Manage in Shared' : 'Upload screenshot');
      upload.onclick = mediaPage ? () => { viewedFriend = null; profileTab = 'shared'; changePage('profile'); loadOwnCommunity(); } : async () => {
        upload.disabled = true;
        try { await syncOwnProfile(); const result = await api.uploadScreenshot(); if (result?.ok) { toast('Screenshot shared'); await loadOwnCommunity(); } }
        catch (error) { toast(error.message); }
        finally { upload.disabled = false; }
      };
      actions.append(upload);
    }
    top.append(actions); section.append(top);
    const tabs = block('nav', 'capture-tabs'); tabs.setAttribute('aria-label', 'Screenshot collection');
    const redraw = () => { parent.replaceChildren(); render(parent, { own, mediaPage }); };
    for (const [id, label] of [['all', 'All'], ['favorites', 'Favourites']]) {
      const button = block('button', choice.tab === id ? 'selected' : '', label);
      button.setAttribute('aria-pressed', String(choice.tab === id));
      button.onclick = () => { choice.tab = id; redraw(); }; tabs.append(button);
    }
    section.append(tabs);
    const toolbar = block('div', 'capture-toolbar');
    const items = [...(state?.media || [])], stars = new Set(preferences(owner).favorites || []);
    toolbar.append(block('span', 'capture-count', items.length + (items.length === 1 ? ' screenshot' : ' screenshots')));
    const sortLabel = block('label', '', 'Sort by: '), sort = document.createElement('select');
    sort.setAttribute('aria-label', 'Sort screenshots');
    for (const [value, label] of [['newest', 'Date Added (New – Old)'], ['oldest', 'Date Added (Old – New)']]) {
      const option = block('option', '', label); option.value = value; sort.append(option);
    }
    sort.value = choice.sort; sort.onchange = () => { choice.sort = sort.value; redraw(); };
    sortLabel.append(sort); toolbar.append(sortLabel, block('span', 'capture-filter', 'Filter by: Screenshots')); section.append(toolbar);
    const empty = (title, detail) => { const area = block('div', 'capture-empty'); area.append(block('h3', '', title), block('p', '', detail)); section.append(area); return area; };
    if (own && !discordAccount) {
      const area = empty('Your moments, all together.', 'Link Discord to view screenshots shared on your profile.'); action(area, 'Link Discord', linkDiscord); return;
    }
    if (state?.loading) { empty('Loading screenshots…', ''); return; }
    if (state?.error) { empty('Screenshots unavailable', state.error); return; }
    const list = items.filter(item => choice.tab === 'all' || stars.has(item.id)).sort((a, b) => {
      const delta = (Date.parse(a.createdAt) || 0) - (Date.parse(b.createdAt) || 0);
      return choice.sort === 'oldest' ? delta : -delta;
    });
    if (!list.length) { empty(choice.tab === 'favorites' ? 'No favourites yet' : 'Nothing shared yet', choice.tab === 'favorites' ? 'Select the star on a screenshot to keep it here.' : own ? 'Screenshots uploaded in your profile’s Shared section appear here and in Media.' : 'This friend has not shared any screenshots.'); return; }
    const grid = block('div', 'capture-grid'); section.append(grid);
    let shown = 0;
    const moreButton = block('button', 'pill capture-more', 'Show more screenshots');
    function more() {
      for (const item of list.slice(shown, shown + 12)) {
        const card = block('article', 'capture-tile');
        const button = block('button', 'capture-open'); button.setAttribute('aria-label', 'View shared screenshot');
        const photo = image('', item.caption || 'Shared screenshot'); photo.loading = 'lazy'; button.append(photo); button.onclick = () => view(owner, item);
        const caption = block('span', 'capture-date', Number.isFinite(Date.parse(item.createdAt)) ? new Date(item.createdAt).toLocaleDateString() : 'Screenshot');
        button.append(caption); card.append(button);
        const star = block('button', 'capture-favorite', stars.has(item.id) ? '★' : '☆');
        star.setAttribute('aria-label', stars.has(item.id) ? 'Remove from favourites' : 'Add to favourites'); star.setAttribute('aria-pressed', String(stars.has(item.id)));
        star.onclick = () => { favorite(owner, item.id); redraw(); }; card.append(star);
        if (own && !mediaPage) {
          const remove = block('button', 'capture-delete', 'Delete'); remove.setAttribute('aria-label', 'Delete screenshot');
          remove.onclick = async () => { remove.disabled = true; try { const result = await api.deleteScreenshot(item.id); if (!result.cancelled) { mediaCache.delete(owner + '/' + item.id); await loadOwnCommunity(); } } catch (error) { toast(error.message); } finally { remove.disabled = false; } }; card.append(remove);
        }
        grid.append(card);
        const cacheKey = owner + '/' + item.id, cached = mediaCache.get(cacheKey);
        if (cached) photo.src = cached;
        else api.communityImage(owner, item.id, false).then(src => { mediaCache.set(cacheKey, src); if (photo.isConnected) photo.src = src; }).catch(() => { photo.alt = 'Screenshot could not load'; card.classList.add('capture-unavailable'); });
      }
      shown = Math.min(shown + 12, list.length); moreButton.hidden = shown >= list.length;
    }
    moreButton.onclick = more; section.append(moreButton); more();
  }
  async function showFriendProfileShared() {
    const friend = viewedFriend; if (!friend) return;
    visitorCommunity = { loading: true }; renderProfile();
    try { const result = await api.communityProfile(friend.id); if (viewedFriend?.id !== friend.id) return; visitorCommunity = result; }
    catch (error) { if (viewedFriend?.id !== friend.id) return; visitorCommunity = { error: error.message }; }
    if (activePage === 'profile' && profileTab === 'shared') renderProfile();
  }
  window.nexaverseGallery = { render };
})();
