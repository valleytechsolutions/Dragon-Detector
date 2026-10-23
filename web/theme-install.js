import {BadgeSerial} from './serial.js?v=0.13.3';
import {deviceQuery} from './device-query.js';
import {parseBadgeInfo} from './firmware-compat.js?v=2026-10-23-dial-v11';
import {connectedProfile} from './board-profiles.js?v=2026-10-23-dial-v11';
import {hasPersonalThemes,uploadTheme} from './theme-transfer.js';
import {decodeTheme} from './theme-codec.js';

export async function installPersonalTheme(port,board,bytes,onProgress=()=>{}, {makeLink=p=>new BadgeSerial(p)}={}){
  decodeTheme(bytes);
  const link=makeLink(port);
  try{
    await link.open();
    const info=parseBadgeInfo(await deviceQuery(link,'badge info',s=>s.startsWith('BADGE_INFO ')));
    if(board==='m5dial'||connectedProfile(info)!==board||!hasPersonalThemes(info?.version))throw new Error('The running board or firmware does not match. The personal theme was not changed.');
    return await uploadTheme(link,bytes,onProgress);
  }finally{await link.close();}
}
