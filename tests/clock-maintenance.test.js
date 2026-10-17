import test from 'node:test';
import assert from 'node:assert/strict';
import {maintainComputerClock} from '../web/clock-maintenance.js';

test('USB maintenance leaves a correct clock alone and recovers drift, missing time and timezone changes',async()=>{
  const epoch=1792213540,now=new Date(epoch*1000);now.getTimezoneOffset=()=>240;
  for(const initial of [{epoch,offset:-240,valid:1},{epoch:epoch-8,offset:-240,valid:1},{epoch:0,offset:-240,valid:0},{epoch,offset:-300,valid:1}]){
    let state={...initial},command='';const writes=[];
    const link={async write(c){command=c;writes.push(c);},async wait(match){
      let line;
      if(command.startsWith('time ')){state.epoch=Number(command.slice(5));state.valid=1;line='TIME_OK';}
      else if(command.startsWith('clock offset ')){state.offset=Number(command.slice(13));line='CLOCK_OK';}
      else if(command==='clock status')line=`CLOCK_STATUS rtc=0 valid=${state.valid} epoch=${state.epoch} offset=${state.offset}`;
      else if(command==='orientation status')line='ORIENTATION_STATUS manual=1 degrees=0';
      else if(command==='clock hardware')line='CLOCK_HARDWARE detected=0 valid=0 epoch=0';
      else throw Error(command);
      assert.ok(match(line));return line;
    }};
    const result=await maintainComputerClock(link,'waveshare','0.13.11',{now:()=>now});
    const corrected=initial.epoch!==epoch||initial.offset!==-240;
    assert.equal(result.corrected,corrected);assert.equal(result.epoch,epoch);assert.equal(result.offset,-240);
    assert.equal(writes.some(c=>c.startsWith('time ')),corrected);
  }
});
