const {contextBridge,ipcRenderer}=require('electron');
contextBridge.exposeInMainWorld('controllerPicker',{
  list:directory=>ipcRenderer.invoke('controller-picker-list',directory),
  choose:selection=>ipcRenderer.invoke('controller-picker-choose',selection),
  cancel:()=>ipcRenderer.invoke('controller-picker-cancel'),
  setControllerMode:mode=>ipcRenderer.send('controller-input-mode',mode),
});
