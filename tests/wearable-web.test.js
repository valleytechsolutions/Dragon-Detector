import test from 'node:test';
import assert from 'node:assert/strict';
import {localClockEpoch,setClock,readWearable,setOrientation} from '../web/wearable-transfer.js';
import {hasWearableTools} from '../web/firmware-compat.js';
test('local time conversion honors offsets, leap days, and invalid dates',()=>{
 assert.equal(localClockEpoch('2024-02-29T00:00:00',0),1709164800);
 assert.equal(localClockEpoch('2024-02-29T05:30:00',330),1709164800);
 assert.equal(localClockEpoch('2024-02-28T20:00',-240),1709164800);
 for(const value of ['2023-02-29T00:00','2024-04-31T00:00','2024-02-29T24:00','2024-02-29T12:60','2100-01-01T00:00','2024-02-29',''])assert.throws(()=>localClockEpoch(value,0));
 for(const offset of [841,-721,NaN,0.5])assert.throws(()=>localClockEpoch('2024-02-29T00:00',offset));
 assert.throws(()=>localClockEpoch('2000-01-01T00:00',840));
 assert.ok(hasWearableTools('0.9.0'));for(const v of ['0.8.0','0.10.0','0.90.0','1.9.0'])assert.ok(!hasWearableTools(v));
});
function linkWith(responses){const writes=[];return {writes,async write(line){writes.push(line);},async wait(predicate){const r=responses.shift();assert.ok(predicate(r),r);return r;}};}
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
