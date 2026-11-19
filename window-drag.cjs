// Cursor and window bounds use Electron's display-independent coordinates.
module.exports=function createWindowDrag({getWindow,screen,canDrag}){
 let timer=null,origin=null;
 function pointOrCursor(point){return point&&Number.isFinite(point.x)&&Number.isFinite(point.y)?point:screen.getCursorScreenPoint()}
 function move(point){
  const win=getWindow();
  if(!origin||!win||win.isDestroyed())return;
  const cursor=pointOrCursor(point);
  win.setPosition(Math.round(origin.bounds.x+cursor.x-origin.cursor.x),Math.round(origin.bounds.y+cursor.y-origin.cursor.y),false);
 }
 function cancel(){if(timer)clearInterval(timer);timer=null;origin=null}
 function start(point){
  cancel();const win=getWindow();
  if(!win||win.isDestroyed()||win.isMinimized()||!canDrag())return false;
  origin={bounds:win.getBounds(),cursor:pointOrCursor(point)};
  timer=setInterval(()=>move(),16);timer.unref();return true;
 }
 function stop(point){move(point);cancel()}
 return {start,stop,cancel};
};
