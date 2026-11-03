import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from '../web/board-profiles.js';
import {parseBadgeInfo,hasWifiClock} from '../web/firmware-compat.js';
import {hasFullLab,hasFullRecon,hasSessionTools} from '../web/lab-capabilities.js';
import {hasBleTools} from '../web/ble-tools.js';
import {hasSd,hasGpsAndSd,resourceSetup} from '../web/sd-resources.js';
import {verifyClock} from '../web/clock-verification.js';

const board='waveshare_185b',profile=boardProfiles[board];
const read=path=>readFileSync(new URL(path,import.meta.url),'utf8');
const fixture=JSON.parse(read('./fixtures/waveshare-185b-0.14.1-manifests.json'));

test('1.85B accepts its exact 360 px identity only from its first supported firmware',()=>{
  assert.equal(connectedProfile(parseBadgeInfo(`BADGE_INFO 0.14.1 360 READY ${profile.firmwareName}`)),board);
  for(const version of ['0.14.0','0.13.42','0.14.01','0.15.0'])assert.equal(parseBadgeInfo(`BADGE_INFO ${version} 360 READY ${profile.firmwareName}`),null);
  for(const size of [240,412,466,480])assert.equal(parseBadgeInfo(`BADGE_INFO 0.14.1 ${size} READY ${profile.firmwareName}`),null);
  for(const name of ['Waveshare ESP32-S3-Touch-LCD-1.85','Waveshare ESP32-S3-Touch-LCD-1.85C'])assert.equal(connectedProfile({board:name,size:360}),null);
  assert.equal(connectedProfile({board:'Spotpear JC3636W518',size:360}),'spotpear');
  assert.ok(supportsBadge(board)&&hasSd(board)&&hasGpsAndSd(board));
  for(const feature of [hasFullLab,hasFullRecon,hasSessionTools,hasBleTools,hasWifiClock]){
    assert.ok(feature(board,'0.14.1'));
    assert.equal(feature(board,'0.14.0'),false);
  }
});

test('1.85B partition, manifest and resource contracts preserve update files and reject similar boards',()=>{
  const entries=Object.fromEntries(read('../hardware/waveshare-lcd-185b/partitions.csv').split(/\r?\n/).filter(s=>s&&!s.startsWith('#')).map(s=>{
    const row=s.split(',');return [row[0],{offset:Number(row[3]),size:Number(row[4])}];
  }));
  assert.deepEqual(entries,{nvs:{offset:0x9000,size:0x5000},otadata:{offset:0xe000,size:0x2000},app0:{offset:0x10000,size:0x400000},app1:{offset:0x410000,size:0x400000},spiffs:{offset:0x810000,size:0x7f0000}});
  assert.equal(entries.spiffs.offset,profile.filesystemOffset);
  assert.equal(entries.spiffs.offset+entries.spiffs.size,16*1024*1024);
  for(const mode of ['factory','update']){
    const manifest=fixture[mode];assert.equal(validateManifest(manifest,board,mode),manifest);
    assert.equal(manifest.builds[0].parts.some(p=>p.path==='littlefs.bin'),mode==='factory');
    assert.ok(!manifest.builds[0].parts.some(p=>p.offset===entries.nvs.offset));
    assert.equal(resourceSetup(board,manifest).required,true);
    for(const wrong of ['spotpear','waveshare','waveshare_175','waveshare_175_gps'])assert.throws(()=>validateManifest(manifest,wrong,mode));
    assert.throws(()=>validateManifest({...manifest,version:'0.14.0'},board,mode));
  }
  assert.equal(resourceSetup(board,{version:'0.14.1'}),null);
});

test('1.85B installer requires a real advancing RTC rather than only system time',async()=>{
  const epoch=1793721600;let reads=0;
  const makeLink=({rtc=1,advances=true}={})=>({async write(command){this.command=command;},async wait(match){
    const line=this.command==='clock status'?`CLOCK_STATUS rtc=${rtc} valid=1 epoch=${epoch} offset=0`:
      this.command==='orientation status'?'ORIENTATION_STATUS manual=1 degrees=0':`CLOCK_HARDWARE detected=${rtc} valid=1 epoch=${epoch+(advances?reads++:0)}`;
    assert.ok(match(line));return line;
  }});
  const now=()=>{const date=new Date(epoch*1000);date.getTimezoneOffset=()=>0;return date;};
  const options={now,delay:async()=>{}};
  await assert.rejects(verifyClock(makeLink({rtc:0}),board,'0.14.1',options),/requires an RTC/);
  await assert.rejects(verifyClock(makeLink({advances:false}),board,'0.14.1',options),/not advancing/);
  reads=0;assert.equal((await verifyClock(makeLink(),board,'0.14.1',options)).rtcVerified,true);
});

test('1.85B catalogue exposes the exact affiliate URL and its own installer and wiring guide',()=>{
  const html=read('../web/index.html');
  assert.match(html,/data-board="waveshare_185b"/);
  assert.match(html,/value="waveshare_185b"/);
  assert.match(html,/href="https:\/\/www\.waveshare\.com\/esp32-s3-touch-lcd-1\.85b\.htm\?&amp;aff_id=122099"/);
  assert.match(html,/href="guides\/WAVESHARE-LCD-185B\.html"/);
  assert.match(read('../README.md'),/https:\/\/www\.waveshare\.com\/esp32-s3-touch-lcd-1\.85b\.htm\?&aff_id=122099/);
});
