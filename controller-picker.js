(() => {
  const api=window.controllerPicker,$=s=>document.querySelector(s);let current=null,loading=false;
  async function open(directory){
    if(loading)return;loading=true;$('#message').textContent='Loading folder…';
    try{
      current=await api.list(directory);$('#title').textContent=current.title;document.title=current.title;
      $('#path').value=current.directory;$('#parent').disabled=current.parent===current.directory;
      const drives=$('#drives');drives.replaceChildren();for(const root of current.roots){const option=document.createElement('option');option.value=root;option.textContent=root;drives.append(option);}drives.value=current.roots.find(root=>current.directory.toLowerCase().startsWith(root.toLowerCase()))||current.roots[0]||'';
      const entries=$('#entries');entries.replaceChildren();
      for(const entry of current.entries){const button=document.createElement('button');button.type='button';const icon=document.createElement('span');icon.textContent=entry.folder?'▣':'◇';icon.setAttribute('aria-hidden','true');const name=document.createElement('span');name.textContent=entry.name;button.append(icon,name);button.onclick=()=>entry.folder?open(entry.path):choose(entry.path);entries.append(button);}
      $('#chooseFolder').hidden=!current.folderMode;$('#message').textContent=current.entries.length?'':'No matching files or folders here.';
      (entries.querySelector('button')||$('#chooseFolder:not([hidden])')||$('#parent')).focus();
    }catch(error){$('#message').textContent=error.message||'Could not read this folder. Choose a parent folder or another drive.';}
    finally{loading=false;}
  }
  async function choose(selection){try{await api.choose(selection);}catch(error){$('#message').textContent=error.message;}}
  $('#parent').onclick=()=>current&&open(current.parent);$('#drives').onchange=()=>open($('#drives').value);
  $('#go').onclick=()=>open($('#path').value);$('#path').onkeydown=event=>{if(event.key==='Enter')open($('#path').value);};
  $('#chooseFolder').onclick=()=>current&&choose(current.directory);$('#cancel').onclick=()=>api.cancel();
  window.addEventListener('nexaverse-controller-back',event=>{event.preventDefault();if(current&&current.parent!==current.directory)open(current.parent);else api.cancel();});
  open();
})();
