const {test}=require('node:test'),assert=require('node:assert/strict');
const {decode,brand,Sampler,spatial,edit,mount}=require('./controller-navigation.js');
const {parseHTML}=require('linkedom');
function pad(id='Xbox One Game Controller',pressed=[],axes=[0,0,0,0],mapping='standard'){return {id,index:0,connected:true,mapping,axes,buttons:Array.from({length:18},(_,i)=>({pressed:pressed.includes(i),value:pressed.includes(i)?1:0}))};}
test('Xbox and PlayStation standard layouts share navigation and platform button identities',()=>{
  assert.equal(brand('Xbox Wireless Controller'),'xbox');assert.equal(brand('Sony DualSense 054c'),'playstation');
  for(const id of ['Xbox One Game Controller','DualSense Wireless Controller']){
    const input=decode(pad(id,[0,4,15]));assert.equal(input.confirm,true);assert.equal(input.tabLeft,true);assert.equal(input.direction,'right');
    assert.equal(decode(pad(id,[5])).tabRight,true);assert.equal(decode(pad(id,[],[0,-.8,0,0])).direction,'up');
    assert.equal(decode(pad(id,[],[.15,.1,0,0])).direction,null);
  }
  assert.equal(decode(pad('Sony DualShock',[1],[],'')).confirm,true);
  assert.equal(decode(pad('Sony DualShock',[2],[],'')).back,true);
  assert.equal(decode(pad('Sony DualShock',[],Array(10).fill(0),'')).direction,null);
  const hats=Array(10).fill(0);hats[9]=-1;assert.equal(decode(pad('Sony DualShock',[],hats,'')).direction,'up');
});
test('held buttons do not repeat confirmations; sticks and D-pad repeat with a navigation delay',()=>{
  const sampler=new Sampler();assert.deepEqual(sampler.sample(decode(pad()),0),[]);
  assert.deepEqual(sampler.sample(decode(pad('',[0,5])),20),['confirm','tabRight']);
  assert.deepEqual(sampler.sample(decode(pad('',[0,5])),200),[]);
  sampler.sample(decode(pad()),210);
  assert.deepEqual(sampler.sample(decode(pad('',[15])),220),['right']);
  assert.deepEqual(sampler.sample(decode(pad('',[15])),400),[]);
  assert.deepEqual(sampler.sample(decode(pad('',[15])),560),['right']);
  const connectedHeld=new Sampler();connectedHeld.sample(decode(pad('',[0])),0);assert.deepEqual(connectedHeld.sample(decode(pad('',[0])),500),[]);
});
test('spatial navigation favors aligned tiles and text editing preserves selection and Unicode',()=>{
  const rect=(x,y)=>({getBoundingClientRect:()=>({left:x,top:y,width:100,height:100,right:x+100,bottom:y+100})});
  const current=rect(0,0),right=rect(120,0),diagonal=rect(110,130);assert.equal(spatial(current,[diagonal,right],'right'),right);
  assert.equal(spatial(current,[right],'left'),null);
  assert.deepEqual(edit('A😀',3,3,null),{value:'A',start:1,end:1});
  assert.equal(edit('abc',1,2,'XYZ',4).value,'aXYc');
});
function fixture(){
  const {window,document}=parseHTML('<html><body><header><nav><button class="tab active" data-page="games">Games</button><button class="tab" data-page="media">Media</button><button class="tab" data-page="leaderboard">Leaderboard</button><button class="tab" data-page="downloads">Downloads</button></nav></header><main><button id="tile1" data-x="0" data-y="100">One</button><button id="tile2" data-x="120" data-y="100">Two</button><input id="search" type="search" aria-label="Search games"><button id="settings">Settings</button></main></body></html>');
  let active=document.body;
  Object.defineProperty(document,'activeElement',{get:()=>active});document.hasFocus=()=>true;
  Object.defineProperty(window.HTMLElement.prototype,'tabIndex',{configurable:true,get(){return this.hasAttribute('tabindex')?Number(this.getAttribute('tabindex')):/BUTTON|INPUT|SELECT|TEXTAREA|A/.test(this.tagName)?0:-1;},set(value){this.setAttribute('tabindex',String(value));}});
  window.HTMLElement.prototype.focus=function(){active=this;this.dispatchEvent(new window.Event('focusin',{bubbles:true}));};
  window.HTMLElement.prototype.scrollIntoView=function(){};
  window.HTMLElement.prototype.getClientRects=function(){return this.hidden||this.closest('[hidden]')?[]:[this.getBoundingClientRect()];};
  window.HTMLElement.prototype.getBoundingClientRect=function(){const x=Number(this.dataset.x)||0,y=Number(this.dataset.y)||0;return {left:x,top:y,right:x+100,bottom:y+50,width:100,height:50};};
  window.HTMLElement.prototype.showModal=function(){this.setAttribute('open','');};window.HTMLElement.prototype.close=function(){this.removeAttribute('open');};
  window.getComputedStyle=()=>({display:'block',visibility:'visible',overflowY:'visible'});window.AbortController=AbortController;
  window.requestAnimationFrame=()=>1;window.cancelAnimationFrame=()=>{};
  for(const b of document.querySelectorAll('.tab'))b.onclick=()=>{document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('active',t===b));};
  const environment={document,navigator:{getGamepads:()=>[]},AbortController,Event:window.Event,CustomEvent:window.CustomEvent,getComputedStyle:window.getComputedStyle,requestAnimationFrame:()=>1,cancelAnimationFrame:()=>{},dispatchEvent:event=>window.dispatchEvent(event)};
  return {window,document,ui:mount(environment)};
}
test('bumpers cycle Games, Media, Leaderboard, Downloads in both directions',()=>{
  const f=fixture();for(const expected of ['media','leaderboard','downloads','games']){f.ui.action('tabRight');assert.equal(f.document.querySelector('.tab.active').dataset.page,expected);}
  f.ui.action('tabLeft');assert.equal(f.document.querySelector('.tab.active').dataset.page,'downloads');f.ui.destroy();
});
test('D-pad/stick movement uses tile geometry and text fields open the controller keyboard',()=>{
  const f=fixture();f.document.querySelector('#tile1').focus();f.ui.move('right');assert.equal(f.document.activeElement.id,'tile2');
  const input=f.document.querySelector('#search');input.value='g';input.maxLength=30;input.focus();f.ui.action('confirm');
  const keyboard=f.document.querySelector('dialog[open]');assert(keyboard);
  [...keyboard.querySelectorAll('button')].find(b=>b.textContent==='t').click();assert.equal(input.value,'gt');
  f.ui.action('details');assert.equal(input.value,'g');f.ui.action('back');assert.equal(input.value,'g');assert.equal(f.document.querySelector('dialog[open]'),null);f.ui.destroy();
});

test('controller adjusts sliders, selects dropdown options and backs out of dialogs',()=>{
  const f=fixture(),range=f.document.createElement('input');range.type='range';range.min='0';range.max='100';range.step='5';range.value='10';f.document.querySelector('main').append(range);
  range.focus();f.ui.move('right');assert.equal(range.value,'15');f.ui.move('left');assert.equal(range.value,'10');
  const select=f.document.createElement('select');select.innerHTML='<option value="one">One</option><option value="two">Two</option>';Object.defineProperty(select,'value',{value:'one',writable:true});f.document.querySelector('main').append(select);let changed=0;select.addEventListener('change',()=>changed++);
  select.focus();f.ui.action('confirm');const dialog=f.document.querySelector('dialog[open]');assert(dialog);[...dialog.querySelectorAll('button')].find(b=>b.textContent==='Two').click();assert.equal(select.value,'two');assert.equal(changed,1);
  select.focus();f.ui.action('confirm');f.ui.action('back');assert.equal(select.value,'two');assert.equal(changed,1,'cancel must not write a setting');
  const modal=f.document.createElement('dialog');modal.innerHTML='<button>Example</button>';f.document.body.append(modal);modal.showModal();f.ui.action('back');assert.equal(modal.hasAttribute('open'),false);f.ui.destroy();
});

test('controller tester receives button presses without accidental activation; Menu closes it',()=>{
  const f=fixture(),tester=f.document.createElement('dialog');tester.id='panel';tester.className='gamepad-dialog';let clicks=0;const button=f.document.createElement('button');button.textContent='Test control';button.onclick=()=>clicks++;tester.append(button);f.document.body.append(tester);tester.showModal();button.focus();f.ui.action('confirm');f.ui.action('back');assert.equal(clicks,0);assert.equal(tester.hasAttribute('open'),true);f.ui.action('menu');assert.equal(tester.hasAttribute('open'),false);f.ui.destroy();
});
