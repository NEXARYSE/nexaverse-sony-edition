const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs/promises'),path=require('node:path'),os=require('node:os');
const {DEFAULT_HOME_ORDER,recentFirst,create}=require('./library-order.cjs');
test('fresh profiles receive all ten default games in the requested order',()=>{
 const games=[{id:'other'},...DEFAULT_HOME_ORDER.toReversed().map(id=>({id}))],before=JSON.stringify(games);
 assert.deepEqual(recentFirst(games).slice(0,10).map(g=>g.id),[...DEFAULT_HOME_ORDER]);assert.equal(JSON.stringify(games),before);
});
test('existing profiles get the new baseline once; later library launches move into the home row and survive restart',async()=>{
 const dir=await fs.mkdtemp(path.join(os.tmpdir(),'nexaverse-order-')),statePath=path.join(dir,'home-order-v47.json');
 const games=[...DEFAULT_HOME_ORDER.toReversed().map((id,index)=>({id,lastPlayed:1000+index,seconds:37})),{id:'custom-game',lastPlayed:9000,seconds:42}];
 const before=JSON.stringify(games),order=create({statePath}),initial=await Promise.all([order.order(games),order.order(games)]);
 for(const items of initial)assert.deepEqual(items.slice(0,10).map(g=>g.id),[...DEFAULT_HOME_ORDER]);assert.equal(JSON.stringify(games),before);
 const launched=games.map(g=>g.id==='custom-game'?{...g,lastPlayed:9001}:g),personalized=await order.order(launched);
 assert.equal(personalized[0].id,'custom-game');assert.equal(personalized.slice(0,10).length,10);assert.deepEqual(personalized.slice(1,4).map(g=>g.id),DEFAULT_HOME_ORDER.slice(0,3));assert.equal(personalized[0].seconds,42);
 assert.equal((await create({statePath}).order(launched))[0].id,'custom-game');
 const next=launched.map(g=>g.id==='last-of-us-2'?{...g,lastPlayed:9002}:g);assert.deepEqual((await order.order(next)).slice(0,2).map(g=>g.id),['last-of-us-2','custom-game']);
 const other=create({statePath:path.join(dir,'another-user.json')});assert.deepEqual((await other.order(next)).slice(0,10).map(g=>g.id),[...DEFAULT_HOME_ORDER]);
});
