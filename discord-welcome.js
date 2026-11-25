(() => {
  const api = window.consoleLauncher;
  const root = document.getElementById('nexaWelcome');
  if (!api || !root) return;
  const profile = root.querySelector('[data-welcome-profile]');
  const avatar = root.querySelector('[data-welcome-avatar]');
  const name = root.querySelector('[data-welcome-name]');
  const link = root.querySelector('[data-welcome-link]');
  const label = root.querySelector('[data-welcome-link-label]');
  const message = root.querySelector('[data-welcome-message]');
  const browserActions = document.createElement('div');
  browserActions.className = 'nexa-welcome-browser'; browserActions.hidden = true;
  for (const [text, action] of [['Open browser again', 'openDiscordAuthorization'], ['Copy sign-in link', 'copyDiscordAuthorization'], ['Cancel linking', 'cancelDiscordLink']]) {
    const button = document.createElement('button'); button.type = 'button'; button.textContent = text;
    button.addEventListener('click', async () => {
      button.disabled = true;
      try { await api[action](); if (action === 'copyDiscordAuthorization') say('Sign-in link copied. Paste it into your browser on this PC and finish Discord authorization.'); }
      catch (error) { say(error.message || 'Could not open Discord. Try Copy sign-in link.', true); }
      finally { button.disabled = false; }
    });
    browserActions.append(button);
  }
  root.append(browserActions);
  async function refreshBrowserActions() {
    try { const state = await api.discordLinkStatus(); browserActions.hidden = !connecting || !state.pending; if (connecting && state.browserError) say(state.browserError, true); }
    catch { browserActions.hidden = true; }
  }

  const clock = root.querySelector('time');
  const powerButton = root.querySelector('[data-welcome-quit]');
  const powerMenu = root.querySelector('[data-welcome-power-menu]');
  const accessActions = root.querySelector('[data-welcome-access]');
  const checkAccess = root.querySelector('[data-welcome-check]');
  const background = [...document.body.children].filter(el => el !== root && el.tagName !== 'SCRIPT');
  const inertBefore = new Map(background.map(el => [el, el.inert]));
  let account = null, connecting = false, entering = false, epoch = 0, lastAccess = '';
  const focusEntry = () => (account ? (profile.disabled ? checkAccess : profile) : link).focus();
  function say(text, error = false) { message.textContent = text; message.dataset.error = String(error); }
  function show(state) {
    account = state?.linked && state.user?.id ? state.user : null;
    root.dataset.linked = String(Boolean(account));
    const access = state?.membership;
    const allowed = access?.status === 'member';
    const wasDenied = lastAccess && lastAccess !== 'member';
    lastAccess = access?.status || '';
    const locked = !account || !state.entered || !allowed;
    const wasHidden = root.hidden;
    root.hidden = !locked;
    for (const el of background) el.inert = locked ? true : inertBefore.get(el);
    if (locked) {
      const panel = document.getElementById('panel');
      if (panel?.open && typeof panel.close === 'function') panel.close();
    }
    profile.hidden = !account;
    label.textContent = account ? 'Add account' : 'Link Discord';
    link.setAttribute('aria-label', account ? 'Link another Discord account' : 'Link your Discord account');
    profile.disabled = !account || entering || !allowed;
    accessActions.hidden = !account || allowed;
    if (account && !allowed) {
      const until = access?.until ? ' Access resumes '+new Date(access.until).toLocaleString()+'.' : '';
      say((access?.message || 'Discord server membership must be verified to continue.')+until,true);
    } else if ((message.dataset.error !== 'true' || wasDenied) && !connecting && !entering) say(account
      ? 'Select your Discord profile to continue.'
      : 'Link Discord and join the NEXAVERSE server to use the launcher.');
    name.textContent = account?.name || account?.username || '';
    profile.setAttribute('aria-label', account ? 'Continue as ' + name.textContent : 'Select your Discord profile');
    const signature = account ? account.id + ':' + (account.avatar || '') : '';
    if (avatar.dataset.account !== signature) {
      avatar.dataset.account = signature;
      avatar.replaceChildren();
      const initial = document.createElement('span');
      initial.className = 'nexa-welcome-initial';
      initial.textContent = Array.from(name.textContent || 'N')[0];
      avatar.append(initial);
      if (account?.avatar && /^https:\/\/cdn\.discordapp\.com\/avatars\//.test(account.avatar)) {
        initial.hidden = true;
        const image = document.createElement('img');
        image.alt = '';
        image.src = account.avatar;
        image.addEventListener('load', () => { initial.hidden = true; });
        image.addEventListener('error', () => {image.remove(); initial.hidden = false;});
        avatar.append(image);
      }
    }
    if (locked && (!wasHidden && !root.contains(document.activeElement) || wasHidden)) focusEntry();
    animate();
  }
  async function refresh() {
    const request = ++epoch;
    try { const state = await api.discordStatus(); if (request === epoch) show(state); }
    catch { if (request === epoch) { show(null); say('Could not check your Discord account. Select Link Discord to retry.', true); } }
  }
  link.addEventListener('click', async () => {
    if (connecting) return;
    connecting = true; link.disabled = true;
    say('Complete Discord authorization in your browser.');
    const poll = setInterval(() => refresh(), 2000);
    const browserPoll = setInterval(refreshBrowserActions, 500);
    try {
      const result = await api.connectDiscord();
      if (!result.ok) throw Error(result.error || 'Discord linking did not complete.');
      say('Discord linked. Select your profile to continue.');
    } catch (error) { say(error.message || 'Discord linking did not complete. Please retry.', true); }
    finally { connecting = false; link.disabled = false; clearInterval(poll); clearInterval(browserPoll); browserActions.hidden = true; await refresh(); }
  });
  profile.addEventListener('click', async () => {
    if (!account || entering || lastAccess !== 'member') return;
    entering = true; profile.disabled = true; say('');
    ++epoch;
    try {
      const state = await api.enterDiscordProfile();
      if (!state.linked || !state.entered) throw Error('Link Discord and select your profile to continue.');
      show(state);
      window.dispatchEvent(new CustomEvent('nexaverse-profile-entered', {detail:state.user}));
    } catch (error) { say(error.message || 'Could not open your profile. Please retry.', true); await refresh(); }
    finally { entering = false; profile.disabled = !account || lastAccess !== 'member'; }
  });
  root.querySelector('[data-welcome-join]').addEventListener('click', async () => {
    try {await api.openDiscordServer();}
    catch {say('Could not open the Discord server. Please retry.',true);}
  });
  checkAccess.addEventListener('click', async () => {
    checkAccess.disabled = true;
    const request = ++epoch;
    say('Checking Discord server access…');
    try {const state=await api.refreshDiscordMembership();if(request===epoch)show(state);}
    catch {say('Could not verify server access. Please retry.',true);}
    finally {checkAccess.disabled = false;}
  });
  function closePowerMenu() { powerMenu.hidden = true; powerButton.setAttribute('aria-expanded','false'); }
  powerButton.addEventListener('click', () => {
    powerMenu.hidden = !powerMenu.hidden;
    powerButton.setAttribute('aria-expanded',String(!powerMenu.hidden));
    if (!powerMenu.hidden) powerMenu.querySelector('button').focus();
  });
  for (const button of powerMenu.querySelectorAll('[data-welcome-power-action]')) button.addEventListener('click', async () => {
    button.disabled = true;
    try { await api.windowAction(button.dataset.welcomePowerAction); }
    catch (error) { say(error.message || 'Could not complete this action. Please retry.',true); }
    finally { button.disabled = false; closePowerMenu(); }
  });
  root.addEventListener('click', event => {
    if (!powerMenu.hidden && !powerMenu.contains(event.target) && !powerButton.contains(event.target)) closePowerMenu();
  });
  root.addEventListener('keydown', event => {
    if (event.key === 'Escape' && !powerMenu.hidden) {event.preventDefault(); closePowerMenu(); powerButton.focus();}
  });
  // Prevent keyboard shortcuts and programmatic background clicks from skipping selection.
  for (const type of ['click', 'keydown', 'pointerdown', 'focusin']) document.addEventListener(type, event => {
    if (root.hidden || root.contains(event.target)) return;
    event.preventDefault(); event.stopImmediatePropagation();
    if (type === 'focusin') focusEntry();
  }, true);
  if (api.onDiscordState) api.onDiscordState(() => refresh());
  window.addEventListener('focus', refresh);
  setInterval(() => { if (!entering) refresh(); }, 15000);
  function updateClock() { clock.textContent = new Date().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'}); }
  updateClock(); setInterval(updateClock, 30000);

  const canvas = root.querySelector('canvas');
  const ctx = canvas.getContext?.('2d');
  const motion = window.matchMedia('(prefers-reduced-motion: reduce)');
  let particles = [], frame = 0, previous = 0, width = 0, height = 0;
  function resize() {
    width = window.innerWidth; height = window.innerHeight;
    const scale = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = width * scale; canvas.height = height * scale;
    ctx?.setTransform(scale, 0, 0, scale, 0, 0);
    particles = Array.from({length:Math.min(250, Math.max(90, Math.round(width / 6)))}, () => ({
      x:Math.random() * width, phase:Math.random() * 6.28, spread:(Math.random()-.5)*height*.36,
      radius:.5+Math.random()**3*4, speed:9+Math.random()*28, alpha:.12+Math.random()*.5,
    }));
    paint(0); animate();
  }
  function paint(dt) {
    if (!ctx) return;
    ctx.clearRect(0, 0, width, height);
    for (const p of particles) {
      p.x += p.speed * dt;
      if (p.x > width + 12) p.x = -12;
      const y = height*.85 - p.x/width*height*.34 + Math.sin(p.x/width*6+p.phase)*height*.055 + p.spread;
      ctx.globalAlpha = p.alpha; ctx.fillStyle = p.radius > 2.5 ? '#c19453' : '#e7c587';
      ctx.beginPath(); ctx.arc(p.x, y, p.radius, 0, Math.PI*2); ctx.fill();
    }
    ctx.globalAlpha = 1;
  }
  function tick(now) {
    frame = 0;
    if (root.hidden || document.hidden || motion.matches) {previous = 0; return;}
    paint(previous ? Math.min((now-previous)/1000,.05) : 0);
    previous = now; frame = requestAnimationFrame(tick);
  }
  function animate() {
    if (!ctx || root.hidden || document.hidden || motion.matches) {
      if (frame) cancelAnimationFrame(frame);
      frame = 0; previous = 0; return;
    }
    if (!frame) frame = requestAnimationFrame(tick);
  }
  window.addEventListener('resize', resize);
  document.addEventListener('visibilitychange', animate);
  motion.addEventListener('change', () => {paint(0); animate();});
  resize();
  api.lockDiscordProfile().then(show).catch(() => {show(null); say('Could not check your Discord account. Please retry.', true);});
})();
