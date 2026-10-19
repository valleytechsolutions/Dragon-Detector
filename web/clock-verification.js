import {readWearable,syncComputerClock} from './wearable-transfer.js?v=0.13.10';
import {hasHardwareClockCheck} from './firmware-compat.js?v=0.13.19';
import {deviceQuery} from './device-query.js?v=0.13.10';

export async function readHardwareClock(link){
  const line=await deviceQuery(link,'clock hardware',s=>s.startsWith('CLOCK_HARDWARE '));
  const m=/^CLOCK_HARDWARE detected=([01]) valid=([01]) epoch=(\d+)$/.exec(line);
  if(!m)throw new Error('Invalid hardware clock response.');
  return {detected:m[1]==='1',valid:m[2]==='1',epoch:Number(m[3])};
}

// Verify the RTC itself, not a software copy that vanishes when power is cut.
export async function verifyClock(link,board,version,{
  now=()=>new Date(),delay=ms=>new Promise(r=>setTimeout(r,ms))
}={}){
  const clock=await readWearable(link),current=now();
  if(!clock.valid||clock.offset!==-current.getTimezoneOffset()||Math.abs(clock.epoch-current.getTime()/1000)>10)
    throw new Error('Clock readback did not match this computer.');
  const expected=['stopwatch','xiao_round','waveshare_175'].includes(board);
  if(expected&&!clock.rtc)throw new Error('This board requires an RTC, but it was not detected. Clock setup is incomplete.');
  let rtcVerified=false;
  if(hasHardwareClockCheck(version)){
    const first=await readHardwareClock(link);
    if(first.detected!==clock.rtc)throw new Error('RTC detection changed during setup.');
    if(first.detected){
      if(!first.valid||Math.abs(first.epoch-now().getTime()/1000)>10)throw new Error('RTC did not retain the time written to it.');
      await delay(1200);
      const second=await readHardwareClock(link);
      if(!second.detected||!second.valid||second.epoch<=first.epoch||Math.abs(second.epoch-now().getTime()/1000)>10)
        throw new Error('RTC is not advancing correctly. Clock setup is incomplete.');
      rtcVerified=true;
    }
  }
  return {...clock,rtcVerified,board,version};
}
export async function syncVerifiedClock(link,board,version,options={}){
  await syncComputerClock(link,options.now?.()??new Date());
  return verifyClock(link,board,version,options);
}
export function clockResultText(result){
  if(result.skipped)return 'Installer closed. Automatic clock setup was not requested.';
  if(result.rtcVerified)return 'Local date, time and timezone set. Hardware RTC readback and ticking verified.';
  if(result.rtc)return 'Local time set. Update to 0.13.10+ to verify the hardware RTC directly.';
  return 'Local date, time and timezone set. This device has no detected external RTC: after full shutdown, use saved Wi-Fi time recovery or reconnect here to sync. Keeping it powered preserves time.';
}
