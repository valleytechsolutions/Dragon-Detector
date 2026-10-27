import test from 'node:test';
import assert from 'node:assert/strict';
import {verifyClock,clockResultText} from '../web/clock-verification.js';
import {clockInstallDialogClass} from '../web/clock-install-dialog.js';
import {boardProfiles,validateManifest} from '../web/board-profiles.js';

test('verified-clock manifests require safe erase prompting and skip unused Improv',()=>{
  for(const [board,p] of Object.entries(boardProfiles))for(const mode of ['factory','update']){
    const parts=[['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]];
    if(mode==='factory')parts.push(['littlefs.bin',p.filesystemOffset]);
    if(mode==='factory'&&p.reconOffset)parts.push(['reconfs.bin',p.reconOffset]);
    const m={name:`Dragon Detector — ${p.firmwareName} (${mode})`,version:p.minVersion||'0.13.10',new_install_prompt_erase:true,new_install_improv_wait_time:0,builds:[{chipFamily:'ESP32-S3',parts:parts.map(([path,offset])=>({path,offset}))}]};
    assert.equal(validateManifest(m,board,mode),m);
    for(const bad of [{new_install_prompt_erase:false},{new_install_improv_wait_time:10}])assert.throws(()=>validateManifest({...m,...bad},board,mode),/manifest/);
  }
});

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
  for(const board of ['m5dial','stopwatch','xiao_round','waveshare_175']){
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
    _state='INSTALL';_installState=undefined;updateComplete=Promise.resolve();
    async _initialize(value){events.push(['initialize',value]);if(value&&!this._installState)this._installState={state:'finished'};await Promise.resolve();events.push('USB released by loader');}
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
test('personal-theme handoff requires a completed install even without clock sync',async()=>{
  for(const finished of [false,true]){
    const {dialog,events}=dialogRig();dialog.autoClock=false;dialog.autoTheme=true;
    await dialog._initialize(finished);
    assert.equal(dialog.installSucceeded,finished);
    assert.equal(events.includes('normal close'),finished);
  }
  const {dialog,events}=dialogRig();dialog.autoTheme=true;dialog._state='ERROR';
  await dialog._initialize(true);assert.equal(dialog.installSucceeded,true);assert.ok(!events.includes('normal close'));
});

test('completed flash stays successful when USB cannot reopen; failed writes do not',()=>{
  const {dialog}=dialogRig();
  assert.equal(dialog.installSucceeded,false);
  dialog._installState={state:'finished'};dialog._state='ERROR';dialog._error='USB disappeared';
  assert.equal(dialog.installSucceeded,true);assert.equal(dialog.installError,'USB disappeared');
  dialog._installState={state:'error',message:'Writing stopped'};
  assert.equal(dialog.installSucceeded,false);assert.equal(dialog.installError,'Writing stopped');
});

test('install counter reports successful writes once, including clock opt-out and USB reopen failure',async()=>{
  for(const config of [{},{autoClock:false},{_state:'ERROR'}]){
    const {dialog}=dialogRig();Object.assign(dialog,config);let reported=0;dialog.onFirmwareInstalled=()=>++reported;
    await dialog._initialize(false);dialog._closeDialog();assert.equal(reported,0);
    dialog._installState={state:'error'};dialog._closeDialog();assert.equal(reported,0);
    dialog._installState={state:'finished'};await dialog._initialize(true);dialog._closeDialog();dialog._closeDialog();assert.equal(reported,1);
    dialog._startInstall();dialog._installState={state:'finished'};await dialog._initialize(true);assert.equal(reported,2);
  }
  class BrokenUsb {async _initialize(){throw new Error('USB reopen failed');}_startInstall(){}_closeDialog(){}}
  const dialog=new (clockInstallDialogClass(BrokenUsb))();dialog._installState={state:'finished'};let reported=0;dialog.onFirmwareInstalled=()=>++reported;
  await assert.rejects(dialog._initialize(true),/USB reopen/);dialog._closeDialog();assert.equal(reported,1);
  dialog._installReported=false;dialog.onFirmwareInstalled=()=>{throw new Error('counter failed');};assert.doesNotThrow(()=>dialog._closeDialog());
});
