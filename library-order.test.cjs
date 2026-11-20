const {test}=require('node:test'),assert=require('node:assert/strict');
const {DEFAULT_HOME_ORDER,recentFirst,create}=require('./library-order.cjs');
test('fresh profiles receive all ten default games in the requested order',()=>{
 const games=[{id:'other'},...DEFAULT_HOME_ORDER.toReversed().map(id=>({id}))],before=JSON.stringify(games);
 assert.deepEqual(recentFirst(games).slice(0,10).map(g=>g.id),[...DEFAULT_HOME_ORDER]);assert.equal(JSON.stringify(games),before);
});
test('existing profiles are ordered by their actual most-recent launch',async()=>{
 const games=[...DEFAULT_HOME_ORDER.toReversed().map((id,index)=>({id,lastPlayed:1000+index,seconds:37})),{id:'custom-game',lastPlayed:9000,seconds:42}];
 const before=JSON.stringify(games),order=create(),initial=await Promise.all([order.order(games),order.order(games)]);
 for(const items of initial)assert.equal(items[0].id,'custom-game');assert.equal(JSON.stringify(games),before);
 const launched=games.map(g=>g.id==='custom-game'?{...g,lastPlayed:9001}:g),personalized=await order.order(launched);
 assert.equal(personalized[0].id,'custom-game');assert.equal(personalized.slice(0,10).length,10);assert.equal(personalized[0].seconds,42);
 assert.equal((await create().order(launched))[0].id,'custom-game');
 const next=launched.map(g=>g.id==='last-of-us-2'?{...g,lastPlayed:9002}:g);assert.deepEqual((await order.order(next)).slice(0,2).map(g=>g.id),['last-of-us-2','custom-game']);
});
test('each launch makes that game the first entry for both Home and Game Library',async()=>{
 const order=create();
 const games=[...DEFAULT_HOME_ORDER.map(id=>({id})),{id:'library-game'}];
 await order.order(games);
 const first=await order.order(games.map(game=>game.id==='library-game'?{...game,lastPlayed:100}:game));
 assert.equal(first[0].id,'library-game');assert.equal(first.slice(0,10).length,10);assert.equal(first.slice(0,10).includes(games.find(game=>game.id==='ufc-6')),false);
 const second=await order.order(first.map(game=>game.id==='god-of-war-ragnarok'?{...game,lastPlayed:200}:game));
 assert.deepEqual(second.slice(0,2).map(game=>game.id),['god-of-war-ragnarok','library-game']);
});
