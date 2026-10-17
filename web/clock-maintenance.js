import {readWearable} from './wearable-transfer.js?v=0.13.11';
import {syncVerifiedClock} from './clock-verification.js?v=0.13.11';

// Called only while the granted USB connection is idle. Never infer current
// time from an old timestamp; the computer supplies the present instant.
export async function maintainComputerClock(link,board,version,options={}){
  const state=await readWearable(link),now=options.now?.()??new Date();
  if(state.valid&&state.offset===-now.getTimezoneOffset()&&Math.abs(state.epoch-now.getTime()/1000)<=2)
    return {corrected:false,...state};
  return {corrected:true,...await syncVerifiedClock(link,board,version,options)};
}
