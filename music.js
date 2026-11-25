(() => {
  const slider=document.querySelector('#musicVolume'),output=document.querySelector('#musicVolumeValue'),toggle=document.querySelector('#musicToggle');
  const previous=document.querySelector('#musicPrevious'),next=document.querySelector('#musicNext');
  const tracks=[{title:"BUT I THINK IT'S A DREAM",artist:'Captain & Tennille',src:'assets/audio/but-i-think-its-a-dream.mp3'},{title:'VIEWS',artist:'Drake',src:'assets/audio/views.mp3'},{title:'FREAKING OUT THE NEIGHBORHOOD',artist:'Mac DeMarco',src:'assets/audio/freaking-out-the-neighborhood.mp3'},{title:'FIRM FRIENDS',artist:'',src:'assets/audio/firm-friends.mp3'},{title:'HIGHS & LOWS',artist:'Hagler',src:'assets/audio/highs-and-lows.mp3'},{title:'SUMMER SALT',artist:'Smino feat. Bari',src:'assets/audio/summer-salt.mp3'},{title:'SAFETY',artist:'',src:'assets/audio/safety.mp3'},{title:'HIGH TIDE',artist:'Lemaitre · Oliver Nelson & Tobtok Remix',src:'assets/audio/high-tide.mp3'},{title:'DIGITAL',artist:'IDK',src:'assets/audio/digital.mp3'},{title:'BACK ON 74',artist:'Jungle',src:'assets/audio/back-on-74.mp3'},{title:'DISPARATE YOUTH',artist:'Santigold',src:'assets/audio/disparate-youth.mp3'},{title:'TADOW',artist:'Masego & FKJ',src:'assets/audio/tadow.mp3'}];let trackIndex=0;const key='sony-music-volume:v1';let volume=5,wantsMusic=true,gameRunning=false,request=0;
  try{const saved=localStorage.getItem(key);if(saved!==null&&saved.trim()!==''&&Number.isFinite(Number(saved)))volume=Math.max(0,Math.min(100,Number(saved)))}catch{}
  const audio=document.createElement('audio');audio.id='launcherMusic';audio.hidden=true;audio.loop=false;audio.preload='metadata';audio.volume=volume/100;audio.src=tracks[trackIndex].src;document.body.append(audio);
  function renderVolume(){slider.value=volume;slider.setAttribute('aria-valuetext',volume+' percent');output.textContent=volume+'%';audio.volume=volume/100;slider.style.setProperty('--volume',volume+'%')}
  function renderPlayback(){const track=tracks[trackIndex],name=track.title;document.querySelector('#musicTrackTitle').textContent=name;slider.setAttribute('aria-label',name+' volume');document.querySelector('.music-control').title=name+(track.artist?' · '+track.artist:'');const label=gameRunning?name+' paused while a game is running':audio.paused?'Play '+name:'Pause '+name;toggle.disabled=gameRunning;previous.disabled=gameRunning;next.disabled=gameRunning;toggle.setAttribute('aria-label',label);toggle.title=label;toggle.setAttribute('aria-pressed',String(!audio.paused));toggle.textContent=!audio.paused?'Ⅱ':'▶'}
  async function reconcile(){const generation=++request;if(gameRunning||!wantsMusic){audio.pause();renderPlayback();return}try{await audio.play()}catch{if(generation===request)renderPlayback();return}if(generation!==request){if(gameRunning||!wantsMusic)audio.pause();return}if(gameRunning||!wantsMusic)audio.pause();renderPlayback()}
  slider.addEventListener('input',()=>{volume=Math.max(0,Math.min(100,Number(slider.value)||0));renderVolume();try{localStorage.setItem(key,String(volume))}catch{}});
  toggle.onclick=()=>{if(gameRunning)return;wantsMusic=audio.paused;return reconcile()};
  window.addEventListener('nexaverse-game-sessions',event=>{const running=event.detail===true;if(running===gameRunning)return;gameRunning=running;reconcile()});
  function changeTrack(direction){trackIndex=(trackIndex+direction+tracks.length)%tracks.length;audio.pause();audio.src=tracks[trackIndex].src;audio.load();renderPlayback();return reconcile()}
  previous.onclick=()=>{if(!gameRunning)return changeTrack(-1)};
  next.onclick=()=>{if(!gameRunning)return changeTrack(1)};
  audio.addEventListener('ended',()=>changeTrack(1));
  audio.addEventListener('play',renderPlayback);audio.addEventListener('pause',renderPlayback);
  audio.addEventListener('error',()=>{renderPlayback();toggle.title=tracks[trackIndex].title+' could not play. Click to retry.'});
  renderVolume();renderPlayback();reconcile();
})();
