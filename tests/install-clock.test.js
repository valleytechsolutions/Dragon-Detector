import test from 'node:test';
import assert from 'node:assert/strict';
import {syncInstalledClock,finishInstallSession} from '../web/install-clock.js';
import {boardProfiles} from '../web/board-profiles.js';
function rig(board='waveshare',{rtc=1,offset=345,valid=1,identity,drift=0,openFails=0,version='0.13.7'}={}){
  const events=[],writes=[],now=new Date('2026-10-16T20:10:30Z');now.getTimezoneOffset=()=>-offset;
  const epoch=now.getTime()/1000;let openings=0;
  const options={now:()=>now,delay:async()=>{},makeLink:()=>({
    async open(){events.push('open');if(openings++<openFails)throw new Error('USB restarting');},
    async close(){events.push('close');},
    async write(command){writes.push(command);this.command=command;},
    async wait(match){
      const reply=this.command==='badge info'?`BADGE_INFO ${version} ${boardProfiles[board].size} ${boardProfiles[board].lite?(version==='0.13.7'?'RADAR_ONLY':'LITE'):'READY'} ${identity||boardProfiles[board].firmwareName}`:
        this.command.startsWith('time ')?'TIME_OK':this.command.startsWith('clock offset ')?'CLOCK_OK':
        this.command==='clock status'?`CLOCK_STATUS rtc=${rtc} valid=${valid} epoch=${epoch+drift} offset=${offset}`:'ORIENTATION_STATUS manual=1 degrees=0';
      assert.ok(match(reply),reply);return reply;
    }
  })};
  return {events,writes,options,epoch};
}
test('post-install clock verifies each full board, current offset and RTC state',async()=>{
  for(const board of Object.keys(boardProfiles).filter(k=>!boardProfiles[k].lite)){
    for(const offset of [-720,-240,0,330,345,840]){
      const rtc=['stopwatch','xiao_round'].includes(board)?1:0;
      const r=rig(board,{offset,rtc});const result=await syncInstalledClock({},board,r.options);
      assert.equal(result.board,board);assert.equal(result.rtc,!!rtc);assert.equal(result.offset,offset);
      assert.deepEqual(r.writes,['badge info',`time ${r.epoch}`,`clock offset ${offset}`,'clock status','orientation status']);
      assert.deepEqual(r.events,['open','close']);
    }
  }
});
test('installer ownership is released before clock connection; opt-out never writes',async()=>{
  const r=rig();await finishInstallSession({async close(){r.events.push('installer-close');}},'waveshare',true,r.options);
  assert.deepEqual(r.events,['installer-close','open','close']);
  for(const [board,enabled] of [['waveshare',false],['waveshare_lite',false]]){
    const r=rig();assert.deepEqual(await finishInstallSession({async close(){r.events.push('installer-close');}},board,enabled,r.options),{skipped:true});
    assert.deepEqual(r.events,['installer-close']);assert.deepEqual(r.writes,[]);
  }
});
test('a locked installer port never reaches the clock writer',async()=>{
  const r=rig();await assert.rejects(finishInstallSession({async close(){throw new Error('Locked');}},'waveshare',true,r.options),/Locked/);
  assert.deepEqual(r.events,[]);assert.deepEqual(r.writes,[]);
});
test('wrong identity is rejected before any clock mutation, with no blind retry',async()=>{
  const r=rig('waveshare',{identity:'M5Stack StopWatch C152'});
  await assert.rejects(syncInstalledClock({},'waveshare',r.options),/identity/);
  assert.deepEqual(r.writes,['badge info']);assert.deepEqual(r.events,['open','close']);
});
test('USB restart is retried, but failure is bounded and all links are closed',async()=>{
  let r=rig('waveshare',{openFails:1});await syncInstalledClock({},'waveshare',r.options);
  assert.deepEqual(r.events,['open','close','open','close']);
  r=rig('waveshare',{openFails:9});await assert.rejects(syncInstalledClock({},'waveshare',r.options),/USB restarting/);
  assert.equal(r.events.filter(x=>x==='open').length,3);assert.equal(r.events.filter(x=>x==='close').length,3);
  assert.deepEqual(r.writes,[]);
});
test('stale or invalid readback is never reported as successful setup',async()=>{
  for(const details of [{drift:60},{valid:0}]){
    const r=rig('waveshare',details);await assert.rejects(syncInstalledClock({},'waveshare',r.options),/readback/);
    assert.equal(r.events.filter(x=>x==='close').length,3);
  }
});

test('Lite timing is version gated after exact identity verification',async()=>{
 const old=rig('waveshare_lite');const result=await syncInstalledClock({},'waveshare_lite',old.options);
 assert.equal(result.skipped,true);assert.deepEqual(old.writes,['badge info']);assert.deepEqual(old.events,['open','close']);
 const current=rig('waveshare_lite',{version:'0.13.9',rtc:0});
 const synced=await syncInstalledClock({},'waveshare_lite',current.options);assert.equal(synced.valid,true);assert.equal(synced.rtc,false);
 assert.deepEqual(current.writes,['badge info',`time ${current.epoch}`,'clock offset 345','clock status','orientation status']);
});
