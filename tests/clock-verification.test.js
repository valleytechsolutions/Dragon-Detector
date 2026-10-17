import test from 'node:test';
import assert from 'node:assert/strict';
import {verifyClock,clockResultText} from '../web/clock-verification.js';
import {clockInstallDialogClass} from '../web/clock-install-dialog.js';

function rig({detected=1,valid=1,delta=1,rtcDrift=0,softwareDrift=0,offset=-240}={}){
  const epoch=1792213540;let hardwareReads=0;const writes=[];
  const link={async write(c){writes.push(c);this.c=c;},async wait(match){
    const line=this.c==='clock status'?`CLOCK_STATUS rtc=${detected} valid=1 epoch=${epoch+softwareDrift} offset=${offset}`:
      this.c==='orientation status'?'ORIENTATION_STATUS manual=1 degrees=0':
      `CLOCK_HARDWARE detected=${detected} valid=${valid} epoch=${detected?epoch+rtcDrift+(hardwareReads++?delta:0):0}`;
    assert.ok(match(line));return line;
  }};
  const options={now:()=>{const d=new Date(epoch*1000);d.getTimezoneOffset=()=>240;return d;},delay:async ms=>assert.equal(ms,1200)};
  return {link,options,writes};
}
test('hardware clock must store the correct epoch and actually tick',async()=>{
  const r=rig();const result=await verifyClock(r.link,'stopwatch','0.13.10',r.options);
  assert.equal(result.rtcVerified,true);assert.match(clockResultText(result),/ticking verified/);
  assert.deepEqual(r.writes,['clock status','orientation status','clock hardware','clock hardware']);
  for(const params of [{valid:0},{rtcDrift:300},{delta:0},{delta:-1},{delta:60}]){
    const r=rig(params);await assert.rejects(verifyClock(r.link,'stopwatch','0.13.10',r.options),/RTC/);
  }
});
test('RTC boards reject missing chips; software boards clearly explain shutdown recovery',async()=>{
  for(const board of ['stopwatch','xiao_round']){
    const r=rig({detected:0});await assert.rejects(verifyClock(r.link,board,'0.13.10',r.options),/not detected/);
  }
  for(const board of ['waveshare','spotpear','waveshare_lite']){
    const r=rig({detected:0});const result=await verifyClock(r.link,board,'0.13.10',r.options);
    assert.equal(result.rtcVerified,false);assert.match(clockResultText(result),/full shutdown/);
  }
  for(const params of [{softwareDrift:60},{offset:0}]){
    const r=rig(params);await assert.rejects(verifyClock(r.link,'waveshare','0.13.10',r.options),/readback/);
  }
});
test('older firmware remains syncable but cannot claim direct RTC verification',async()=>{
  const r=rig();const result=await verifyClock(r.link,'stopwatch','0.13.9',r.options);
  assert.equal(result.rtcVerified,false);assert.match(clockResultText(result),/Update to 0.13.10/);
  assert.deepEqual(r.writes,['clock status','orientation status']);
});

function dialogRig(){
  const events=[];
  class Base{
    _state='INSTALL';_installState={state:'finished'};updateComplete=Promise.resolve();
    async _initialize(value){events.push(['initialize',value]);await Promise.resolve();events.push('USB released by loader');}
    _startInstall(erase){events.push(['erase',erase]);}
    _closeDialog(){events.push('normal close');}
  }
  return {dialog:new (clockInstallDialogClass(Base))(),events};
}
test('finished installs automatically hand off to clock setup only after initialization',async()=>{
  const {dialog,events}=dialogRig();await dialog._initialize(true);
  assert.deepEqual(events,[['initialize',true],'USB released by loader','normal close']);
  for(const config of [{autoClock:false},{_state:'ERROR'},{_installState:{state:'writing'}}]){
    const r=dialogRig();Object.assign(r.dialog,config);await r.dialog._initialize(true);assert.ok(!r.events.includes('normal close'));
  }
  const r=dialogRig();await r.dialog._initialize(false);assert.ok(!r.events.includes('normal close'));
});
test('update mode never accepts the upstream no-Improv default erase',()=>{
  const {dialog,events}=dialogRig();dialog._startInstall(true);assert.deepEqual(events,[['erase',false]]);
  dialog.installMode='factory';dialog._startInstall(false);assert.deepEqual(events[1],['erase',true]);
  assert.throws(()=>clockInstallDialogClass(class {}),/integration changed/);
});
