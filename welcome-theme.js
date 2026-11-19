(() => {
  const expires = new Date(2027, 0, 1).getTime();
  const icon = (now = Date.now()) => now < expires
    ? 'assets/welcome-holiday-icon.png'
    : 'assets/launcher-icon.png';
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { icon, expires };
    return;
  }
  window.welcomeTheme = { icon };
  const snow = document.querySelector('.welcome-snow');
  for (let i = 0; snow && i < 120; i++) {
    const flake = document.createElement('i');
    flake.style.setProperty('--x', ((i * 61.8034) % 100).toFixed(2) + '%');
    flake.style.setProperty('--size', (1 + i % 6 * .5) + 'px');
    flake.style.setProperty('--duration', (12 + i % 17) + 's');
    flake.style.setProperty('--delay', (-i * 1.73) + 's');
    flake.style.setProperty('--drift', ((i * 29 % 90) - 45) + 'px');
    flake.style.setProperty('--alpha', .3 + i % 6 * .1);
    snow.append(flake);
  }
  function pauseWhenInactive() {
    document.body.classList.toggle('welcome-snow-paused', document.hidden || !document.hasFocus());
  }
  document.addEventListener('visibilitychange', pauseWhenInactive);
  window.addEventListener('focus', pauseWhenInactive);
  window.addEventListener('blur', pauseWhenInactive);
  pauseWhenInactive();
  function refreshIcon() {
    const tileIcon = document.querySelector('.tile.welcome img');
    if (tileIcon) tileIcon.setAttribute('src', icon());
    const remaining = expires - Date.now();
    if (remaining > 0) setTimeout(refreshIcon, Math.min(remaining, 86400000));
  }
  window.addEventListener('focus', refreshIconOnWake);
  document.addEventListener('visibilitychange', refreshIconOnWake);
  function refreshIconOnWake() {
    const tileIcon = document.querySelector('.tile.welcome img');
    if (tileIcon) tileIcon.setAttribute('src', icon());
  }
  refreshIcon();
})();
