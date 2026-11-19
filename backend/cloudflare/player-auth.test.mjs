import assert from 'node:assert/strict';
import {playerIdentity} from './player-auth.mjs';
let calls=0;
const request=token=>new Request('https://fixture/client/Session/login',{headers:token?{Authorization:token}:{}});
assert.equal(await playerIdentity(request(),()=>{throw Error('must not fetch')}),null);
const identity=await playerIdentity(request('Bearer player-token'),async(url,options)=>{
 calls++;assert.equal(url,'https://discord.com/api/v10/users/@me');assert.equal(options.headers.authorization,'Bearer player-token');assert.equal(options.redirect,'manual');
 return Response.json({id:'123456789012345678',username:'private',global_name:'Private'});
});
assert.deepEqual(identity,{id:'123456789012345678',displayName:'Private'});assert.equal(calls,1);
assert.equal(await playerIdentity(request('Bearer invalid'),async()=>new Response('',{status:401})),null);
assert.equal(await playerIdentity(request('Bearer invalid'),async()=>Response.json({id:'not-an-account'})),null);
console.log('PASS: player tokens verified only with Discord, invalid credentials denied, only verified account ID retained.');
