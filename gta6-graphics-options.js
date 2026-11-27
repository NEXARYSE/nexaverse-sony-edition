(function(root,factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else root.GTA6Graphics=api})(globalThis,()=>{
 const resolutions=Object.freeze({
  '720p-native':{label:'720p native',output:'1280x720',scale:'1.0',native:true},
  '720p':{label:'720p → 1080p (default)',output:'1920x1080',scale:'0.6666666666666666',native:false},
  '1080p':{label:'1080p native',output:'1920x1080',scale:'1.0',native:true},
  '1440p':{label:'2K / 1440p native',output:'2560x1440',scale:'1.0',native:true}
 });
 const toggle=[['false','Off'],['true','On']];
 const fields=[
  {key:'frameGeneration',label:'Frame generation',group:'Display',default:'off',choices:[['off','Off'],['2x','DLSS 2×']],feature:'frameGeneration'},
  {key:'upscaler',label:'Upscaler / anti-aliasing',group:'Display',default:'temporal',choices:[['temporal','Game temporal'],['dlss','NVIDIA DLSS / DLAA']],feature:'dlss'},
  {key:'dlssMode',label:'DLSS mode (upscaled rendering)',group:'Display',default:'quality',choices:[['quality','Quality'],['balanced','Balanced'],['performance','Performance'],['ultraperformance','Ultra Performance']],feature:'dlss'},
  {key:'frameLimit',label:'Frame-rate target',group:'Display',default:'30',choices:[['30','30 FPS'],['60','60 FPS'],['120','120 FPS']],xml:['desiredFPS','frameRateLimit']},
  {key:'vSync',label:'V-Sync',group:'Display',default:'0',choices:[['0','Off'],['1','On']],xml:['vSync']},
  {key:'hdr',label:'HDR',group:'Display',default:'false',choices:[['false','Off — SDR output']],locked:true,xml:['hdr']},
  {key:'anisotropicFiltering',label:'Anisotropic filtering',group:'Textures',default:'0',choices:[['0','Game default'],['2','2×'],['4','4×'],['8','8×'],['16','16×']],xml:['anisotropicFiltering']},
  {key:'lodScale',label:'Draw distance',group:'World',default:'0.5',choices:[['0.25','25%'],['0.5','50% (default)'],['0.75','75%'],['1','100%']],xml:['lodScale']},
  {key:'grassLod',label:'Grass detail distance',group:'World',default:'0.8',choices:[['0.25','25%'],['0.5','50%'],['0.8','80% (default)'],['1','100%']],xml:['grassLod']},
  {key:'motionBlur',label:'Motion blur',group:'Effects',default:'false',choices:toggle,xml:['motionBlur']},
  {key:'sharpenEnabled',label:'Sharpening',group:'Effects',default:'false',choices:toggle,xml:['sharpenEnabled']},
  {key:'sharpenIntensity',label:'Sharpening strength',group:'Effects',default:'1',choices:[['0.25','25%'],['0.5','50%'],['1','100%']],xml:['sharpenIntensity']},
  {key:'shadowAlphaTintedShadows',label:'Tinted alpha shadows',group:'Shadows',default:'false',choices:toggle,xml:['shadowAlphaTintedShadows']},
  {key:'shadowParticleShadows',label:'Particle shadows',group:'Shadows',default:'false',choices:toggle,xml:['shadowParticleShadows']},
  {key:'shadowLongShadows',label:'Long shadows',group:'Shadows',default:'false',choices:toggle,xml:['shadowLongShadows']},
  {key:'directionalScreenSpaceShadowQuality',label:'Screen-space directional shadows',group:'Shadows',default:'0',choices:[['0','Off'],['0.5','Half'],['1','Full']],xml:['directionalScreenSpaceShadowQuality']},
  {key:'worldHeightShadowQuality',label:'World-height shadows',group:'Shadows',default:'0',choices:[['0','Off'],['0.5','Half'],['1','Full']],xml:['worldHeightShadowQuality']},
  {key:'waterReflectionSSR',label:'Water screen-space reflections',group:'Reflections',default:'false',choices:toggle,xml:['waterReflectionSSR']},
  {key:'waterReflectionSize',label:'Water reflection resolution',group:'Reflections',default:'512x256',choices:[['256x128','256 × 128'],['512x256','512 × 256 (default)'],['1024x512','1024 × 512']]},
  {key:'ssroSampleCount',label:'Reflection sample count',group:'Reflections',default:'2',choices:[['1','1'],['2','2 (default)'],['4','4']],xml:['ssroSampleCount']},
  {key:'mapMSAA',label:'Map MSAA',group:'Effects',default:'false',choices:toggle,xml:['mapMSAA']},
  {key:'treeTessellationEnabled',label:'Tree tessellation',group:'World',default:'false',choices:toggle,xml:['treeTessellationEnabled']},
  {key:'vrsEnabled',label:'Variable-rate shading',group:'Effects',default:'false',choices:toggle,xml:['vrsEnabled']},
  {key:'asyncComputeEnabled',label:'Async compute',group:'Advanced',default:'true',choices:toggle,xml:['asyncComputeEnabled']},
  {key:'transferQueuesEnabled',label:'Transfer queues',group:'Advanced',default:'true',choices:toggle,xml:['transferQueuesEnabled']}
 ];
 function resolution(value){return typeof value==='string'&&Object.hasOwn(resolutions,value)?value:'720p'}
 function normalize(value){const input=value&&typeof value==='object'&&!Array.isArray(value)?value:{};return Object.fromEntries(fields.map(f=>[f.key,f.choices.some(([v])=>v===input[f.key])?input[f.key]:f.default]))}
 function validatePatch(patch){if(!patch||typeof patch!=='object'||Array.isArray(patch))throw Error('Invalid graphics settings.');for(const [key,value]of Object.entries(patch)){const f=fields.find(f=>f.key===key);if(!f||typeof value!=='string'||!f.choices.some(([v])=>v===value))throw Error('Unsupported graphics setting: '+key);}return patch}
 function xmlValues(settings){const s=normalize(settings),values={};for(const f of fields)for(const name of f.xml||[])values[name]=['lodScale','grassLod','sharpenIntensity','directionalScreenSpaceShadowQuality','worldHeightShadowQuality'].includes(name)?Number(s[f.key]).toFixed(6):s[f.key];const [w,h]=s.waterReflectionSize.split('x');values.waterReflectionResX=w;values.waterReflectionResY=h;return values}
 return {resolutions,fields,resolution,normalize,validatePatch,xmlValues};
});
