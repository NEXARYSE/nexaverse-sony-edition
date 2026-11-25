// Local launcher access policy. Discord OAuth credentials stay in discord-auth.
const PUBLIC_CHANNELS = new Set([
  'discord-link-status', 'discord-link-open', 'discord-link-copy', 'discord-link-cancel',
  'discord-status', 'discord-connect', 'discord-logout', 'discord-enter', 'discord-lock', 'discord-membership-refresh',
  'controller-picker-cancel', 'open-discord-server', 'window-drag-start', 'window-drag-end', 'window-state', 'window-action',
  'check-updates', 'update-status', 'download-update', 'install-update',
  // Read-only data needed to initialize the renderer behind the welcome screen.
  'library', 'storage', 'launcher-activity', 'steam-status', 'discord-friends',
  'community-online-count',
]);

function validIdentity(state) {
  return state?.linked === true && /^\d{17,20}$/.test(state.user?.id || '');
}

function create({identity, membership, notify = () => {}}) {
  let selectedId = null;
  let accessSignature = '';
  async function status({forceMembership=false} = {}) {
    const account = await identity.status();
    const linked = validIdentity(account);
    if (!linked || account.user.id !== selectedId) selectedId = null;
    let access = {status:'unlinked',message:'Link your Discord account to use NEXAVERSE.'};
    if (linked) {
      try { access = await membership.check(account.user.id,{force:forceMembership}); }
      catch { access = {status:'unavailable',message:'Could not verify Discord server access. Please retry.'}; }
      const after = await identity.status();
      if (!after.linked || after.user?.id !== account.user.id) {
        selectedId = null;
        return {...after,entered:false,membership:{status:'unavailable',message:'Discord account changed. Select your profile again.'}};
      }
    }
    if (access.status !== 'member') selectedId = null;
    return {...account, linked, membership:access, entered: linked && access.status === 'member' && selectedId === account.user.id};
  }
  async function lock() {
    selectedId = null;
    const state = await status({forceMembership:true});
    notify(state);
    return state;
  }
  async function enter() {
    const state = await status({forceMembership:true});
    if (!state.linked) throw Error('Link your Discord account to use NEXAVERSE.');
    if (state.membership.status !== 'member') {notify(state);throw Error(state.membership.message);}
    selectedId = state.user.id;
    const entered = {...state, entered: true};
    notify(entered);
    return entered;
  }
  async function requireEntry({freshMembership=false,expectedUserId} = {}) {
    const state = await status({forceMembership:freshMembership});
    if (expectedUserId && state.user?.id !== expectedUserId) throw Error('Discord account changed while preparing the game. Please launch again.');
    if (state.entered) return state.user;
    notify(state);
    if (state.linked && state.membership.status !== 'member') throw Error(state.membership.message);
    throw Error(state.linked
      ? 'Select your Discord profile on the welcome screen to continue.'
      : 'Link your Discord account to use NEXAVERSE.');
  }
  async function refreshAccess() {
    const state = await status({forceMembership:true});
    const signature = JSON.stringify([state.linked,state.user?.id,state.entered,state.membership?.status,state.membership?.until]);
    if (signature !== accessSignature) {accessSignature = signature;notify(state);}
    return state;
  }
  function register(ipcMain, channel, handler) {
    ipcMain.handle(channel, async (...args) => {
      if (!PUBLIC_CHANNELS.has(channel)) await requireEntry({freshMembership:channel==='launch'});
      return handler(...args);
    });
  }
  return {status, lock, enter, requireEntry, register, refreshAccess};
}

module.exports = {create, validIdentity};
