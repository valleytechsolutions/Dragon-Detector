import {BadgeSerial} from './serial.js?v=0.13.3';
import {deviceQuery} from './device-query.js';
import {parseBadgeInfo,hasWearableTools,hasClockTools} from './firmware-compat.js?v=2026-10-23-watcher';
import {connectedProfile,boardProfiles} from './board-profiles.js?v=2026-10-23-watcher';
import {syncVerifiedClock} from './clock-verification.js?v=2026-10-23-watcher';

// Called only after the installer has relinquished this exact granted port.
// Never guess among getPorts(): another attached detector could be changed.
export async function syncInstalledClock(port,board,{
  makeLink=p=>new BadgeSerial(p),delay=ms=>new Promise(r=>setTimeout(r,ms)),now=()=>new Date()
}={}){
  if(!boardProfiles[board])throw new Error('Choose the exact board first.');
  let failure;
  for(let attempt=0;attempt<3;attempt++){
    await delay(attempt?1500:800);
    const link=makeLink(port);
    try{
      await link.open();
      const info=parseBadgeInfo(await deviceQuery(link,'badge info',s=>s.startsWith('BADGE_INFO ')));
      if(connectedProfile(info)!==board||!hasWearableTools(info?.version)){
        const error=new Error('Board identity does not match, or its clock protocol is too old. No clock settings were changed.');
        error.identity=true;throw error;
      }
      if(!hasClockTools(board,info.version))return {skipped:true,reason:'This older Lite firmware has no clock.'};
      const clock=await syncVerifiedClock(link,board,info.version,{now,delay});
      return {...clock,board,version:info.version};
    }catch(error){failure=error;if(error.identity)throw error;}
    finally{await link.close();}
  }
  throw failure;
}

// Keep the lifecycle separate from UI rendering so close/handoff failures can
// be tested without flashing a board. A declined checkbox still closes USB.
export async function finishInstallSession(port,board,autoClock,options){
  await port.close();
  if(!autoClock)return {skipped:true};
  return syncInstalledClock(port,board,options);
}

// Cancelling the dialog must never send setup commands to existing firmware.
// A closed/disconnected port is fine on cancellation; a locked port on a
// completed install must still fail before another USB reader is created.
export async function completeInstallSession(port,board,{succeeded,autoClock},options){
  if(!succeeded){
    try{await port.close();}catch{}
    return {cancelled:true};
  }
  if(port.readable===null&&port.writable===null){
    if(!autoClock)return {skipped:true};
    return syncInstalledClock(port,board,options);
  }
  return finishInstallSession(port,board,autoClock,options);
}
