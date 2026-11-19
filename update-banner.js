(() => {
  const banner = block('aside', 'update-banner');
  banner.id = 'updateBanner';
  banner.setAttribute('popover', 'manual');
  banner.setAttribute('aria-label', 'Launcher update');
  const heading = block('strong', '', 'Launcher update');
  const message = block('p'); message.setAttribute('role', 'status');
  const progress = document.createElement('progress'); progress.max = 100;
  progress.setAttribute('aria-label', 'Launcher update download progress');
  const retry = block('button', 'pill', 'Retry update');
  retry.onclick = () => api.checkUpdates().then(show).catch(() => {});
  banner.append(heading, message, progress, retry); document.body.append(banner);
  let visible = false;
  function bringToFront() {
    if (!visible || !banner.showPopover) return;
    try { if (banner.matches(':popover-open')) banner.hidePopover(); banner.showPopover(); } catch {}
  }
  function show(state) {
    visible = ['available', 'downloading', 'ready', 'waiting', 'restarting', 'installing', 'error'].includes(state.status);
    banner.hidden = !visible;
    if (!visible) { try { banner.hidePopover?.(); } catch {} return; }
    heading.textContent = state.status === 'error' ? 'Update interrupted' : state.status === 'downloading' || state.status === 'available' ? 'Downloading launcher update' : 'Installing launcher update';
    if (state.availableVersion && state.status !== 'error') heading.textContent += ' · ' + state.availableVersion;
    message.textContent = state.message;
    progress.hidden = !['available', 'downloading'].includes(state.status);
    progress.value = Math.max(0, Math.min(100, state.progress || 0));
    retry.hidden = state.status !== 'error';
    if (!banner.matches(':popover-open')) bringToFront();
  }
  // Keep the notice above controller/profile dialogs without taking keyboard focus.
  new MutationObserver(bringToFront).observe(document.querySelector('#panel'), {attributes: true, attributeFilter: ['open']});
  api.onUpdate(show);
  api.updateStatus().then(show).catch(() => {});
})();
