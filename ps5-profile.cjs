function playerName(user){let name='';for(const c of String(user.global_name||user.name||user.username||'')){if(Buffer.byteLength(name+c,'utf8')>16)break;if(!/[\u0000-\u001f\u007f]/u.test(c))name+=c;}if(!name.trim())throw Error('Reconnect Discord to load your player name.');return name;}
module.exports={playerName};
