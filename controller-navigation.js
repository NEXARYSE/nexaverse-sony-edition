(function(root,factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else{root.NexaController=api;root.nexaControllerNavigation=api.mount(root);}})(typeof window!=='undefined'?window:globalThis,function(){
  const clamp=n=>Math.max(-1,Math.min(1,Number(n)||0));
  function brand(id=''){return /xbox|xinput|045e/i.test(id)?'xbox':/054c|sony|dualshock|dualsense|playstation|wireless controller/i.test(id)?'playstation':'xbox';}
  function decode(pad){
    const type=brand(pad.id),rawSony=type==='playstation'&&pad.mapping!=='standard';
    const pressed=i=>{const b=pad.buttons?.[i];return typeof b==='number'?b>.6:Boolean(b?.pressed||b?.value>.6)};
    const ax=i=>clamp(pad.axes?.[i]);
    let up=pressed(12),down=pressed(13),left=pressed(14),right=pressed(15);
    if(rawSony){
      up=down=left=right=false;
      const hat=pad.axes?.[9];
      if(Number.isFinite(hat)&&hat>=-1&&hat<=1){const d=Math.round((hat+1)*3.5);if(Math.abs(hat-(d/3.5-1))<.075){up=[0,1,7].includes(d);right=[1,2,3].includes(d);down=[3,4,5].includes(d);left=[5,6,7].includes(d);}}
    }
    const x=ax(0),y=ax(1);
    let direction=up?'up':down?'down':left?'left':right?'right':null;
    if(!direction&&Math.max(Math.abs(x),Math.abs(y))>.5)direction=Math.abs(x)>=Math.abs(y)?(x>0?'right':'left'):(y>0?'down':'up');
    return {brand:type,direction,confirm:pressed(rawSony?1:0),back:pressed(rawSony?2:1),details:pressed(rawSony?0:2),search:pressed(3),tabLeft:pressed(4),tabRight:pressed(5),profile:pressed(8),menu:pressed(9),scroll:Math.abs(ax(rawSony?5:3))>.25?ax(rawSony?5:3):pressed(7)?1:pressed(6)?-1:0};
  }
  class Sampler{
    constructor(){this.previous=null;this.armed=false;this.next=0;}
    sample(input,time){
      const keys=['confirm','back','details','search','tabLeft','tabRight','profile','menu'];
      const idle=!input.direction&&!keys.some(k=>input[k]);
      if(!this.previous){this.previous=input;this.armed=idle;return [];}
      if(!this.armed){this.previous=input;if(idle)this.armed=true;return [];}
      const events=keys.filter(k=>input[k]&&!this.previous[k]);
      if(input.direction){if(input.direction!==this.previous.direction){events.push(input.direction);this.next=time+330;}else if(time>=this.next){events.push(input.direction);this.next=time+115;}}
      this.previous=input;return events;
    }
  }
  function spatial(current,candidates,direction){
    const a=current.getBoundingClientRect(),ax=a.left+a.width/2,ay=a.top+a.height/2;
    let best=null,score=Infinity;
    for(const el of candidates){if(el===current)continue;const b=el.getBoundingClientRect(),dx=b.left+b.width/2-ax,dy=b.top+b.height/2-ay;
      const primary=direction==='right'?dx:direction==='left'?-dx:direction==='down'?dy:-dy;
      if(primary<=2)continue;
      const perpendicular=direction==='left'||direction==='right'?Math.abs(dy):Math.abs(dx);
      const overlap=direction==='left'||direction==='right'?Math.min(a.bottom,b.bottom)-Math.max(a.top,b.top):Math.min(a.right,b.right)-Math.max(a.left,b.left);
      const value=primary+perpendicular*2.8+(overlap>0?0:250);
      if(value<score){score=value;best=el;}
    }
    return best;
  }
  function edit(value,start,end,text,maxLength=-1){
    start=Math.max(0,Math.min(value.length,start));end=Math.max(start,Math.min(value.length,end));
    if(text===null){if(start===end&&start>0)start-=Array.from(value.slice(0,start)).at(-1).length;text='';}
    if(maxLength>=0)text=text.slice(0,Math.max(0,maxLength-(value.length-(end-start))));
    return {value:value.slice(0,start)+text+value.slice(end),start:start+text.length,end:start+text.length};
  }
  function mount(win){
    const doc=win.document;if(!doc||!win.navigator?.getGamepads)return null;
    const bridge=win.consoleLauncher||win.controllerPicker||{};
    const signal=new win.AbortController();
    let activeIndex=null,activeId='',sampler=new Sampler(),frame=0,lastTime=0,mode=false,lastBrand='',lastFocused=null,lastTile=null,overlay=null;
    const hints=doc.createElement('div');hints.className='controller-hints';hints.hidden=true;hints.setAttribute('aria-hidden','true');doc.body.append(hints);
    const on=(target,event,handler)=>target.addEventListener(event,handler,{signal:signal.signal});
    function visible(el){
      if(!el?.isConnected||el.disabled||el.getAttribute('aria-disabled')==='true'||el.closest('[hidden],[inert]'))return false;
      for(let p=el;p;p=p.parentElement)if(p.inert)return false;
      const s=win.getComputedStyle(el);return s.display!=='none'&&s.visibility!=='hidden'&&el.getClientRects().length>0;
    }
    function scope(){
      const dialogs=[...doc.querySelectorAll('dialog[open]')];if(dialogs.length)return dialogs.at(-1);
      const welcome=doc.getElementById('nexaWelcome');if(welcome&&!welcome.hidden){const power=welcome.querySelector('[data-welcome-power-menu]');return power&&!power.hidden?power:welcome;}
      return doc.body;
    }
    function candidates(root=scope()){return [...root.querySelectorAll('button,a[href],input:not([type="hidden"]),select,textarea,[tabindex]')].filter(el=>el.tabIndex>=0&&visible(el));}
    function focus(el,preview=true){
      if(!el||!visible(el))return;
      lastFocused?.classList.remove('controller-focus');lastFocused=el;el.classList.add('controller-focus');el.focus({preventScroll:true});el.scrollIntoView?.({block:'nearest',inline:'nearest',behavior:'auto'});
      if(el.dataset.controllerTile){
        lastTile=el.dataset.controllerTile;
        if(preview&&!el.classList.contains('selected'))win.dispatchEvent(new win.CustomEvent('nexaverse-controller-preview',{detail:{id:lastTile}}));
      }
    }
    function ensureFocus(){
      const root=scope(),current=doc.activeElement;
      if(root.contains(current)&&visible(current)&&current.matches('button,a,input,select,textarea,[tabindex]'))return current;
      let target=null;
      if(lastTile&&root===doc.body)target=[...root.querySelectorAll('[data-controller-tile]')].find(el=>el.dataset.controllerTile===lastTile&&visible(el));
      for(const selector of ['[autofocus]','[data-welcome-profile]','[data-welcome-check]','.tile.selected','.pill','button','input','select']){if(target)break;target=[...root.querySelectorAll(selector)].find(visible);}
      focus(target||candidates(root)[0],false);return doc.activeElement;
    }
    function setMode(value,type='xbox'){
      hints.hidden=!value;
      if(mode===value&&(!value||lastBrand===type))return;
      lastBrand=type;
      mode=value;doc.body.classList.toggle('controller-mode',value);doc.body.dataset.controllerBrand=type;hints.hidden=!value;
      if(value)hints.textContent=type==='playstation'?'× Select   ○ Back   L1 / R1 Tabs   OPTIONS Settings':'A Select   B Back   LB / RB Tabs   MENU Settings';
      else lastFocused?.classList.remove('controller-focus');
      bridge.setControllerMode?.(value);
    }
    function closeOverlay(commit=false){
      if(!overlay)return;const old=overlay;overlay=null;
      const changed=old.input&&old.input.value!==old.original;
      if(!commit&&changed){old.input.value=old.original;old.input.dispatchEvent(new win.Event('input',{bubbles:true}));}
      if(commit&&changed)old.input.dispatchEvent(new win.Event('change',{bubbles:true}));
      old.dialog.close();old.dialog.remove();if(old.input?.isConnected)focus(old.input,false);
    }
    function dialog(title){const d=doc.createElement('dialog');d.className='controller-keyboard';const h=doc.createElement('h2');h.textContent=title;d.append(h);doc.body.append(d);on(d,'cancel',event=>{event.preventDefault();closeOverlay(false);});return d;}
    function keyboard(input){
      if(overlay)return;const d=dialog(input.getAttribute('aria-label')||input.placeholder||'Enter text');
      const display=doc.createElement('input');display.readOnly=true;display.tabIndex=-1;display.setAttribute('aria-label','Text being entered');d.append(display);
      overlay={dialog:d,input,original:input.value,start:input.selectionStart??input.value.length,end:input.selectionEnd??input.value.length,shift:false};
      const update=()=>{display.value=input.type==='password'?'•'.repeat(input.value.length):input.value;};
      const insert=text=>{if(!overlay)return;const value=edit(input.value,overlay.start,overlay.end,text,input.maxLength);input.value=value.value;overlay.start=value.start;overlay.end=value.end;input.dispatchEvent(new win.Event('input',{bubbles:true}));update();};
      const add=(parent,label,run)=>{const b=doc.createElement('button');b.type='button';b.textContent=label;b.setAttribute('aria-label',label==='⌫'?'Backspace':label);b.onclick=run;parent.append(b);return b;};
      const numeric=['number','tel'].includes(input.type)||input.inputMode==='numeric';
      const rows=numeric?['123','456','789','0.-']:['1234567890','qwertyuiop','asdfghjkl','zxcvbnm','@._-/:\\'];
      for(const chars of rows){const row=doc.createElement('div');row.className='controller-key-row';for(const c of chars){const b=add(row,c,()=>insert(overlay?.shift?c.toUpperCase():c));b.dataset.letter=c;}d.append(row);}
      const controls=doc.createElement('div');controls.className='controller-key-row';
      add(controls,'Shift',()=>{if(!overlay)return;overlay.shift=!overlay.shift;d.querySelectorAll('[data-letter]').forEach(b=>b.textContent=overlay.shift?b.dataset.letter.toUpperCase():b.dataset.letter);});
      add(controls,'Space',()=>insert(' '));add(controls,'⌫',()=>insert(null));add(controls,'Clear',()=>{input.value='';overlay.start=overlay.end=0;input.dispatchEvent(new win.Event('input',{bubbles:true}));update();});
      d.append(controls);const footer=doc.createElement('div');footer.className='controller-key-row';add(footer,'Cancel',()=>closeOverlay(false));add(footer,'Done',()=>closeOverlay(true));d.append(footer);
      overlay.backspace=()=>insert(null);update();d.showModal();focus(d.querySelector('button'));
    }
    function select(input){
      if(overlay)return;const d=dialog(input.getAttribute('aria-label')||'Choose an option');overlay={dialog:d,input,original:input.value};
      const choices=doc.createElement('div');choices.className='controller-select-options';
      for(const option of input.options){if(option.disabled||option.hidden)continue;const b=doc.createElement('button');b.textContent=option.textContent;b.classList.toggle('selected',option.value===input.value);b.onclick=()=>{input.value=option.value;input.dispatchEvent(new win.Event('input',{bubbles:true}));closeOverlay(true);};choices.append(b);}
      d.append(choices);d.showModal();focus(d.querySelector('.selected')||d.querySelector('button'));
    }
    function move(direction){
      const current=ensureFocus();if(!current)return;
      if(current.matches('input[type="range"]')&&['left','right'].includes(direction)){
        const step=Number(current.step)||1,min=Number(current.min)||0,max=current.max===''?100:Number(current.max);
        current.value=String(Math.max(min,Math.min(max,(Number(current.value)||0)+(direction==='right'?step:-step))));current.dispatchEvent(new win.Event('input',{bubbles:true}));current.dispatchEvent(new win.Event('change',{bubbles:true}));return;
      }
      let choices=candidates();
      if(current.dataset.controllerTile&&['left','right'].includes(direction))choices=choices.filter(el=>el.dataset.controllerTile);
      const next=spatial(current,choices,direction);if(next)focus(next);
    }
    function tabs(direction){
      if(overlay)return;
      const welcome=doc.getElementById('nexaWelcome');if(welcome&&!welcome.hidden)return;
      const root=scope();let list;
      if(root===doc.body)list=[...doc.querySelectorAll('header .tab[data-page]')].filter(visible);
      else list=[...root.querySelectorAll('[role="tab"],.profile-tabs button')].filter(visible);
      if(!list.length)return;
      const index=list.findIndex(b=>b.classList.contains('active')||b.classList.contains('selected')||b.getAttribute('aria-selected')==='true');
      const target=list[(Math.max(0,index)+direction+list.length)%list.length];target.click();focus(target,false);lastTile=null;
    }
    function clickVisible(selector){const target=[...doc.querySelectorAll(selector)].find(visible);if(target){focus(target,false);target.click();return true;}return false;}
    function back(){
      if(overlay){closeOverlay(false);return;}
      const custom=new win.CustomEvent('nexaverse-controller-back',{cancelable:true});if(!win.dispatchEvent(custom))return;
      const welcome=doc.getElementById('nexaWelcome');if(welcome&&!welcome.hidden){const power=welcome.querySelector('[data-welcome-power-menu]');if(power&&!power.hidden)welcome.querySelector('[data-welcome-quit]').click();else focus(welcome.querySelector('[data-welcome-quit]'));return;}
      const root=scope();if(root!==doc.body){const button=candidates(root).find(b=>/^(←\s*)?(back|cancel|return|settings)\b/i.test(b.textContent.trim()));if(button)button.click();else{root.close();ensureFocus();}return;}
      if(!clickVisible('#libraryBack,#profileBack'))clickVisible('header .tab[data-page="games"]');
    }
    function action(name){
      const tester=doc.querySelector('#panel.gamepad-dialog[open]');
      if(tester){hints.hidden=true;if(name==='menu')tester.close();return;}
      if(['up','down','left','right'].includes(name)){move(name);return;}
      if(name==='tabLeft'||name==='tabRight'){tabs(name==='tabRight'?1:-1);return;}
      if(name==='back'){back();return;}
      if(name==='confirm'){
        const el=ensureFocus();if(!el)return;
        if(el.tagName==='SELECT'){select(el);return;}
        if(el.matches('textarea,input:not([type="checkbox"]):not([type="radio"]):not([type="range"]):not([type="button"]):not([type="submit"])')){if(!el.readOnly)keyboard(el);return;}
        if(el.dataset.controllerTile&&!['welcome','library'].includes(el.dataset.controllerTile)&&el.classList.contains('selected')){clickVisible('#heroAction');return;}
        el.click();return;
      }
      if(overlay){if(name==='details')overlay.backspace?.();if(name==='menu')closeOverlay(true);return;}
      const welcome=doc.getElementById('nexaWelcome');
      if(welcome&&!welcome.hidden){if(name==='menu')clickVisible('[data-welcome-quit]');if(name==='details')clickVisible('[data-welcome-link]');return;}
      if(name==='menu')clickVisible('#settings');if(name==='details')clickVisible('#details');if(name==='search')clickVisible('#search');if(name==='profile')clickVisible('#profile');
    }
    function scroll(value,dt){
      if(!value||overlay)return;let container=doc.activeElement;
      while(container&&container!==doc.body){const style=win.getComputedStyle(container);if(/auto|scroll/.test(style.overflowY)&&container.scrollHeight>container.clientHeight+2)break;container=container.parentElement;}
      if(!container||container===doc.body){const root=scope();container=[root,...root.querySelectorAll('section,div')].find(el=>visible(el)&&/auto|scroll/.test(win.getComputedStyle(el).overflowY)&&el.scrollHeight>el.clientHeight+2)||doc.scrollingElement;}
      container?.scrollBy?.({top:value*dt*1000,behavior:'instant'});
    }
    function poll(time){
      frame=win.requestAnimationFrame(poll);let pads=[];try{pads=Array.from(win.navigator.getGamepads()||[]).filter(p=>p&&p.connected!==false);}catch{return;}
      if(!pads.length){activeIndex=null;activeId='';sampler=new Sampler();if(mode)setMode(false);return;}
      let pad=pads.find(p=>p.index===activeIndex)||pads[0];
      const moving=pads.filter(p=>{const d=decode(p);return d.direction||['confirm','back','details','search','tabLeft','tabRight','profile','menu'].some(k=>d[k]);}).sort((a,b)=>(b.timestamp||0)-(a.timestamp||0));
      if(moving.length)pad=moving[0];
      if(pad.index!==activeIndex||pad.id!==activeId){activeIndex=pad.index;activeId=pad.id;sampler=new Sampler();}
      const input=decode(pad),dt=Math.min(.05,lastTime?(time-lastTime)/1000:0);lastTime=time;
      if(doc.hidden||!doc.hasFocus()){sampler=new Sampler();return;}
      const events=sampler.sample(input,time);
      if(events.length||input.scroll){setMode(true,input.brand);for(const event of events)action(event);scroll(input.scroll,dt);}
    }
    on(doc,'pointerdown',event=>{if(event.isTrusted!==false)setMode(false);});
    on(doc,'keydown',event=>{if(event.isTrusted!==false)setMode(false);});
    on(doc,'focusin',event=>{if(mode&&visible(event.target)){lastFocused?.classList.remove('controller-focus');lastFocused=event.target;lastFocused.classList.add('controller-focus');}});
    bridge.onDiscordState?.(state=>{if(!state.entered)closeOverlay(false);});
    frame=win.requestAnimationFrame(poll);
    return {action,move,tabs,keyboard,select,setMode,destroy(){win.cancelAnimationFrame(frame);signal.abort();closeOverlay(false);setMode(false);hints.remove();}};
  }
  return {brand,decode,Sampler,spatial,edit,mount};
});
