import {BadgeSerial} from './serial.js?v=0.14.9';
import {deviceQuery} from './device-query.js';
import {parseBadgeInfo} from './firmware-compat.js?v=0.14.9';
import {connectedProfile} from './board-profiles.js?v=0.14.9';

export function p4UpdateSlot(line){
  const match=/^P4_LAYOUT layout=1 revision=(\d+) running=(\d+) boot=(\d+)$/.exec(line);
  if(!match)throw new Error('The installed partition layout could not be verified.');
  const [revision,running,boot]=match.slice(1).map(Number);
  if(revision<300||revision>399)throw new Error('This firmware requires an ESP32-P4 revision 3 device.');
  if(running!==boot)throw new Error('An OTA boot change is pending. Restart the board and try again.');
  if(running===0x10000)return 0;
  if(running===0x610000)return 1;
  throw new Error('The running application is outside the supported P4 slots.');
}

export async function readP4UpdateSlot(port,{makeLink=p=>new BadgeSerial(p)}={}){
  const link=makeLink(port);
  try{
    await link.open();
    const identity=parseBadgeInfo(await deviceQuery(link,'badge info',line=>line.startsWith('BADGE_INFO ')));
    if(connectedProfile(identity)!=='waveshare_p4_knob')throw new Error('Connect the running Waveshare P4 knob to update it.');
    let layout;
    try{layout=await deviceQuery(link,'install layout',line=>line.startsWith('P4_LAYOUT '));}
    catch(error){throw new Error('Update needs a running P4 build that reports its active slot. '+error.message);}
    return p4UpdateSlot(layout);
  }finally{await link.close();}
}
