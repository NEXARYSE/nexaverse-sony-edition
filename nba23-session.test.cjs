const {test}=require('node:test'),assert=require('node:assert/strict'),{EventEmitter}=require('node:events');
const {create}=require('./nba23-session.cjs');
test('NBA 2K23 bridge uses player identity, its own port and closes with the game',async()=>{
 let closed=0,received;
 const adapter=create({cloudAccessToken:async id=>{assert.equal(id,'123456789012345678');return 'fixture-player-token'},relayFactory:options=>{received=options;return {listen:async()=>({port:32123}),close:async()=>{closed++}}},diagnostics:true});
 const plan=await adapter.prepare({discordUserId:'123456789012345678',exe:'game.exe',args:['--shader-cache'],cwd:'nba23-account',env:{}});
 assert.equal(received.token,'fixture-player-token');assert.equal(plan.env.NEXAVERSE_NBA23_BACKEND_PORT,'32123');assert(!plan.env.NEXAVERSE_NBA22_BACKEND_PORT);assert(plan.args.includes('--emulate-psn'));assert(plan.args.includes('--network-connected'));assert(plan.args.includes('--shader-cache'));
 const child=new EventEmitter();adapter.spawn(plan,()=>child);child.emit('exit',0);await adapter.close();assert.equal(closed,1);
 const failed=await adapter.prepare({discordUserId:'123456789012345678',exe:'bad',args:[],env:{}});assert.throws(()=>adapter.spawn(failed,()=>{throw Error('spawn failed')}),/spawn failed/);await adapter.close();assert.equal(closed,2);
 await assert.rejects(create().prepare({}),/linked Discord/);
});
