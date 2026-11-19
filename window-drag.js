for(const zone of document.querySelectorAll('.window-drag-region,.header-drag-handle')){
 let dragging=false;
 zone.addEventListener('pointerdown',event=>{
  if(event.button!==0||!document.body.classList.contains('window-restored'))return;
  event.preventDefault();dragging=true;zone.setPointerCapture(event.pointerId);
  window.consoleLauncher.beginWindowDrag({x:event.screenX,y:event.screenY}).catch(()=>{});
 });
 const finish=event=>{if(!dragging)return;dragging=false;window.consoleLauncher.endWindowDrag(event&&Number.isFinite(event.screenX)?{x:event.screenX,y:event.screenY}:undefined).catch(()=>{})};
 zone.addEventListener('pointerup',finish);
 zone.addEventListener('pointercancel',finish);
 zone.addEventListener('lostpointercapture',finish);
 window.addEventListener('blur',finish);
}
