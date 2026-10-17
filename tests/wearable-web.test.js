import test from 'node:test';
import assert from 'node:assert/strict';
import {localClockEpoch,setClock,syncComputerClock,readWearable,setOrientation} from '../web/wearable-transfer.js';
import {hasWearableTools} from '../web/firmware-compat.js';
test('local time conversion honors offsets, leap days, and invalid dates',()=>{
 assert.equal(localClockEpoch('2024-02-29T00:00:00',0),1709164800);
 assert.equal(localClockEpoch('2024-02-29T05:30:00',330),1709164800);
 assert.equal(localClockEpoch('2024-02-28T20:00',-240),1709164800);
 for(const value of ['2023-02-29T00:00','2024-04-31T00:00','2024-02-29T24:00','2024-02-29T12:60','2100-01-01T00:00','2024-02-29',''])assert.throws(()=>localClockEpoch(value,0));
 for(const offset of [841,-721,NaN,0.5])assert.throws(()=>localClockEpoch('2024-02-29T00:00',offset));
 assert.throws(()=>localClockEpoch('2000-01-01T00:00',840));
 assert.ok(hasWearableTools('0.9.0'));for(const v of ['0.8.0','0.14.0','0.90.0','1.9.0'])assert.ok(!hasWearableTools(v));
});
function linkWith(responses){const writes=[];return {writes,async write(line){writes.push(line);},async wait(predicate){const r=responses.shift();assert.ok(predicate(r),r);return r;}};}
test('one-click sync copies the instant and current local offset, including fractional zones',async()=>{
 for(const minutes of [-720,-300,-240,0,330,345,840]){
  const now=new Date('2026-10-16T20:10:30.987Z');now.getTimezoneOffset=()=>-minutes;
  const link=linkWith(['TIME_OK','CLOCK_OK']);await syncComputerClock(link,now);
  assert.deepEqual(link.writes,['time 1792181430',`clock offset ${minutes}`]);
 }
 const link=linkWith([]);await assert.rejects(syncComputerClock(link,new Date(NaN)),/Invalid clock/);assert.equal(link.writes.length,0);
});
test('clock writes require acknowledgments and propagate RTC failure',async()=>{
 let link=linkWith(['TIME_OK','CLOCK_OK']);await setClock(link,1709164800,-240);assert.deepEqual(link.writes,['time 1709164800','clock offset -240']);
 link=linkWith(['TIME_ERROR']);await assert.rejects(setClock(link,1709164800,0),/clock could not/);assert.equal(link.writes.length,1);
 link=linkWith(['TIME_OK','CLOCK_ERROR offset']);await assert.rejects(setClock(link,1709164800,0),/offset/);
});
test('hardware status and orientation commands are strictly bounded',async()=>{
 const link=linkWith(['CLOCK_STATUS rtc=1 valid=1 epoch=1709164800 offset=-240','ORIENTATION_STATUS manual=1 degrees=90']);
 assert.deepEqual(await readWearable(link),{rtc:true,valid:true,epoch:1709164800,offset:-240,orientation:'90'});
 for(const value of ['0','90','180','270']){const l=linkWith(['ORIENTATION_OK']);await setOrientation(l,value);assert.deepEqual(l.writes,[`orientation set ${value}`]);}
 await assert.rejects(setOrientation(linkWith([]),'45'),/Choose/);
 await assert.rejects(setOrientation(linkWith([]),'auto'),/Choose/);
 await assert.rejects(setOrientation(linkWith(['ORIENTATION_ERROR unavailable']),'90'),/unavailable/);
});

import {hasClockTools,hasWifiClock,hasRetainedClock,parseBadgeInfo} from '../web/firmware-compat.js';
test('clock recovery and Lite timing capabilities reject unsupported boards and versions',()=>{
 for(const version of ['0.13.8','0.13.09','0.14.0','1.13.9','']){assert.equal(hasRetainedClock(version),false);assert.equal(hasClockTools('waveshare_lite',version),false);assert.equal(hasWifiClock('spotpear',version),false);}
 for(const board of ['waveshare','stopwatch','spotpear','xiao_round'])assert.equal(hasWifiClock(board,'0.13.9'),true);
 for(const board of ['waveshare_lite','other'])assert.equal(hasWifiClock(board,'0.13.9'),false);
 assert.equal(hasClockTools('waveshare_lite','0.13.9'),true);
 assert.ok(parseBadgeInfo('BADGE_INFO 0.13.9 240 LITE Waveshare ESP32-S3-LCD-1.28 Lite'));
 assert.equal(parseBadgeInfo('BADGE_INFO 0.13.8 240 LITE Waveshare ESP32-S3-LCD-1.28 Lite'),null);
 assert.equal(parseBadgeInfo('BADGE_INFO 0.13.9 360 LITE Spotpear JC3636W518'),null);
});
