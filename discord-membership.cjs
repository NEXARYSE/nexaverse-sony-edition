// Checks only the linked user's own server membership. No bot credentials are bundled.
// Discord: GET /users/@me/guilds/{guild.id}/member (guilds.members.read).
function create({identity, guildId, request = globalThis.fetch, now = Date.now}) {
  let cached = null;
  let lastMember = null;
  const pending = new Map();
  let retryAfter = 0;
  let retryUser = null;
  const GRACE_MS = 5 * 60 * 1000;
  const result = (status, message, extra = {}) => ({status, message, ...extra});
  function defer(userId, milliseconds) {
    if (retryUser !== userId) retryAfter = 0;
    retryUser = userId;
    retryAfter = Math.max(retryAfter, now()+milliseconds+250);
  }
  function seconds(header) {
    if (header === null || header === undefined || header === '') return 0;
    const value = Number(header);
    if (Number.isFinite(value)) return Math.max(0,value*1000);
    const date = Date.parse(header);
    return Number.isFinite(date)?Math.max(0,date-now()):0;
  }
  function grace(userId, value) {
    const age = lastMember ? now()-lastMember.at : Infinity;
    if (value.transient && lastMember?.userId === userId && age >= 0 && age < GRACE_MS) {
      return {...lastMember.value,verificationDeferred:true,verifiedAt:new Date(lastMember.at).toISOString(),retryAt:new Date(retryAfter).toISOString(),message:'Discord verification is temporarily delayed. Retrying automatically.'};
    }
    return value;
  }
  async function verify(userId) {
    try {
      if (!/^\d{17,20}$/.test(guildId || '')) return result('unavailable','Server access is not configured. Contact NEXAVERSE support.');
      let token = await identity.accessToken();
      let response;
      for (let attempt=0; attempt<2; attempt++) {
        try {
          response = await request(`https://discord.com/api/v10/users/@me/guilds/${guildId}/member`, {
            headers:{Authorization:'Bearer '+token}, signal:AbortSignal.timeout(10000),
          });
        } catch {
          defer(userId,30000);
          return result('unavailable','Could not reach Discord. Retrying automatically.',{transient:true});
        }
        if (response.status !== 401 || attempt) break;
        token = await identity.accessToken({force:true,previousToken:token});
      }
      const header = name => response.headers?.get?.(name);
      if (header('x-ratelimit-remaining') === '0') defer(userId,Math.max(seconds(header('x-ratelimit-reset-after')),seconds(header('retry-after'))));
      if (response.status === 404) return result('not-member','Join the NEXAVERSE Discord server to use the launcher. Banned accounts cannot enter.');
      if (response.status === 401 || response.status === 403) return result('reconnect','Reconnect Discord to verify server access.');
      if (response.status === 429) {
        const body = await response.json().catch(()=>({}));
        // Discord can return different route/global cooldowns. Respect the longest.
        defer(userId,Math.max(15000,seconds(body.retry_after),seconds(header('retry-after')),seconds(header('x-ratelimit-reset-after'))));
        return result('unavailable','Discord is busy. Retrying automatically.',{transient:true,retryAt:new Date(retryAfter).toISOString()});
      }
      if (!response.ok) {
        const transient = response.status >= 500;
        if (transient) defer(userId,30000);
        return result('unavailable','Could not verify Discord server access. Check your connection and retry.',{transient});
      }
      const member = await response.json();
      if (!member || typeof member !== 'object' || !member.user) return result('unavailable','Discord returned invalid membership data. Please retry.');
      const current = await identity.status();
      if (!current.linked || current.user?.id !== userId || member.user?.id !== userId) return result('unavailable','Discord account changed. Select your profile again.');
      const until = Date.parse(member.communication_disabled_until || '');
      if (member.communication_disabled_until && !Number.isFinite(until)) return result('unavailable','Discord returned an invalid timeout. Please retry.');
      // Prefer Discord's server time so an incorrect PC clock does not end a timeout early.
      const serverTime = Date.parse(response.headers?.get?.('date') || '');
      if (Number.isFinite(until) && until > (Number.isFinite(serverTime)?serverTime:now())) return result('timed-out','Your Discord server timeout also applies to NEXAVERSE.',{until:new Date(until).toISOString()});
      if (member.pending === true) return result('pending','Accept the server rules in Discord, then check access again.');
      return result('member','Server access verified.');
    } catch (error) {
      if (/session expired|invalid session|Link Discord|account changed/i.test(error?.message || '')) return result('reconnect','Reconnect Discord to verify server access.');
      defer(userId,30000);
      return result('unavailable','Could not verify Discord server access. Retrying automatically.',{transient:true});
    }
  }
  async function check(userId, {force=false} = {}) {
    if (pending.has(userId)) return pending.get(userId);
    if (retryUser === userId && now()<retryAfter) return grace(userId,result('unavailable','Discord is busy. Retrying automatically.',{transient:true,retryAt:new Date(retryAfter).toISOString()}));
    // Forced entry/launch checks still coalesce a just-completed verification.
    // Polls and the three steps of one launch must not each consume a Discord request.
    const age=cached ? now()-cached.at : Infinity;
    if (cached?.userId === userId && age >= 0 && age < (force?5000:30000)) return grace(userId,cached.value);
    const promise = verify(userId).then(value => {
      cached={userId,at:now(),value};
      if (value.status === 'member') lastMember={userId,at:now(),value};
      else if (!value.transient) lastMember=null;
      return grace(userId,value);
    }).finally(()=>pending.delete(userId));
    pending.set(userId,promise);
    return promise;
  }
  return {check};
}
module.exports = {create};
