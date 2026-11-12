import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from '../web/board-profiles.js';
import {parseBadgeInfo,hasWifiClock} from '../web/firmware-compat.js';
import {hasFullLab,hasFullRecon,hasSessionTools} from '../web/lab-capabilities.js';
import {hasBleTools} from '../web/ble-tools.js';
import {hasWebAudio} from '../web/audio-transfer.js';
import {hasSd,hasGpsAndSd,resourceSetup} from '../web/sd-resources.js';
import {checkRelease} from '../web/install-checks.js';
import {verifyClock,clockResultText} from '../web/clock-verification.js';

const board='waveshare_143c',profile=boardProfiles[board];
const read=path=>readFileSync(new URL(path,import.meta.url),'utf8');
const fixture=JSON.parse(read('./fixtures/waveshare-143c-0.14.7-manifests.json'));

test('1.43C accepts its exact 466 px identity only from its first supported firmware',()=>{
  assert.equal(connectedProfile(parseBadgeInfo(`BADGE_INFO 0.14.7 466 READY ${profile.firmwareName}`)),board);
  for(const version of ['0.14.6','0.13.42','0.14.07','0.15.0'])assert.equal(parseBadgeInfo(`BADGE_INFO ${version} 466 READY ${profile.firmwareName}`),null);
  for(const size of [240,360,412,480])assert.equal(parseBadgeInfo(`BADGE_INFO 0.14.7 ${size} READY ${profile.firmwareName}`),null);
  for(const state of ['RADAR_ONLY','LITE'])assert.equal(parseBadgeInfo(`BADGE_INFO 0.14.7 466 ${state} ${profile.firmwareName}`),null);
  // The 16 MB 1.43 / 1.43-B and the other 466 px boards are different hardware.
  for(const name of ['Waveshare ESP32-S3-Touch-AMOLED-1.43','Waveshare ESP32-S3-Touch-AMOLED-1.43-B','Waveshare ESP32-S3-Touch-AMOLED-1.43C-EN'])
    assert.equal(connectedProfile({board:name,size:466}),null);
  assert.equal(connectedProfile({board:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',size:466}),'waveshare');
  assert.equal(connectedProfile({board:profile.firmwareName,size:360}),null);
  assert.ok(supportsBadge(board)&&!hasSd(board)&&!hasGpsAndSd(board));
  for(const feature of [hasFullLab,hasFullRecon,hasSessionTools,hasBleTools,hasWifiClock,hasWebAudio]){
    assert.ok(feature(board,'0.14.7'));
    assert.equal(feature(board,'0.14.6'),false);
  }
  // The profile carries the gate itself, and the 1.75C's 0.9.8-0.12 lab window is not inherited.
  assert.equal(profile.minVersion,'0.14.7');
  for(const old of ['0.12.0','0.9.8'])for(const feature of [hasFullLab,hasFullRecon])assert.equal(feature(board,old),false);
});

test('1.43C partition and manifest contracts fill exactly 8 MB and reject similar boards',()=>{
  const entries=Object.fromEntries(read('../hardware/waveshare-amoled-143c/partitions.csv').split(/\r?\n/).filter(s=>s&&!s.startsWith('#')).map(s=>{
    const row=s.split(',');return [row[0],{offset:Number(row[3]),size:Number(row[4])}];
  }));
  assert.deepEqual(entries,{nvs:{offset:0x9000,size:0x5000},otadata:{offset:0xe000,size:0x2000},app0:{offset:0x10000,size:0x400000},spiffs:{offset:0x410000,size:0x3f0000}});
  assert.equal(entries.spiffs.offset,profile.filesystemOffset);
  assert.equal(entries.spiffs.offset+entries.spiffs.size,8*1024*1024);
  for(const mode of ['factory','update']){
    const manifest=fixture[mode];assert.equal(validateManifest(manifest,board,mode),manifest);
    assert.equal(manifest.builds[0].parts.some(p=>p.path==='littlefs.bin'),mode==='factory');
    assert.ok(!manifest.builds[0].parts.some(p=>p.offset===entries.nvs.offset));
    assert.equal(resourceSetup(board,manifest),null);
    for(const wrong of ['waveshare','waveshare_175','waveshare_175_gps','lilygo_175','stopwatch','xiao_round'])assert.throws(()=>validateManifest(manifest,wrong,mode));
    assert.throws(()=>validateManifest({...manifest,version:'0.14.6'},board,mode));
    assert.throws(()=>validateManifest({...manifest,sdResources:{schema:1,required:false}},board,mode),/no SD slot/);
  }
  const moved=structuredClone(fixture.factory);moved.builds[0].parts.at(-1).offset=0x810000;
  assert.throws(()=>validateManifest(moved,board,'factory'));
});

test('1.43C installer checks files against 8 MB of flash, not the 16 MB default',async()=>{
  const rig=sizes=>({fetchImpl:async url=>url.endsWith('.json')?{ok:true,json:async()=>structuredClone(fixture.factory)}:
    {ok:true,headers:new Headers({'content-length':String(sizes[url.split('/').pop()]??4096),'content-type':'application/octet-stream'})}});
  assert.equal((await checkRelease(board,'factory',rig({'littlefs.bin':0x3f0000,'firmware.bin':0x400000}))).version,'0.14.7');
  await assert.rejects(checkRelease(board,'factory',rig({'littlefs.bin':0x3f0001})),/flash capacity/);
  await assert.rejects(checkRelease(board,'factory',rig({'firmware.bin':0x7f0001})),/flash capacity/);
});

test('1.43C clock setup accepts the system clock and explains recovery after power loss',async()=>{
  const epoch=1794283200;
  const makeLink=({rtc=0,valid=1}={})=>({async write(command){this.command=command;},async wait(match){
    const line=this.command==='clock status'?`CLOCK_STATUS rtc=${rtc} valid=${valid} epoch=${epoch} offset=0`:
      this.command==='orientation status'?'ORIENTATION_STATUS manual=1 degrees=0':`CLOCK_HARDWARE detected=${rtc} valid=0 epoch=0`;
    assert.ok(match(line));return line;
  }});
  const now=()=>{const date=new Date(epoch*1000);date.getTimezoneOffset=()=>0;return date;};
  const result=await verifyClock(makeLink(),board,'0.14.7',{now,delay:async()=>{}});
  assert.equal(result.rtc,false);assert.equal(result.rtcVerified,false);
  assert.match(clockResultText(result),/no detected external RTC/);
  await assert.rejects(verifyClock(makeLink({valid:0}),board,'0.14.7',{now,delay:async()=>{}}),/readback/);
});

test('1.43C catalogue offers one installer, its guide and the exact store link',()=>{
  const html=read('../web/index.html');
  assert.equal((html.match(/data-board="waveshare_143c"/g)||[]).length,1);
  assert.equal((html.match(/value="waveshare_143c"/g)||[]).length,1);
  assert.match(html,/id="board-143c-setup"[^>]*hidden/);
  const setup=html.match(/<aside\b[^>]*id="board-143c-setup"[^>]*\bhidden\b[^>]*>([\s\S]*?)<\/aside>/);
  assert.ok(setup,'setup notes exist and start hidden');
  assert.match(setup[1],/href="guides\/WAVESHARE-AMOLED-143C\.html"/);
  assert.match(html,/href="https:\/\/www\.waveshare\.com\/esp32-s3-touch-amoled-1\.43c\.htm\?&amp;aff_id=122099"/);
  assert.match(read('../web/app.js'),/\$\('board-143c-setup'\)\.hidden=board!=='waveshare_143c';/);
  assert.match(read('../README.md'),/https:\/\/www\.waveshare\.com\/esp32-s3-touch-amoled-1\.43c\.htm\?&aff_id=122099/);
  assert.match(read('../docs/WAVESHARE-AMOLED-143C.md'),/^# Waveshare ESP32-S3-Touch-AMOLED-1\.43C/);
  // The plain 1.43 / 1.43-B has the same chip, so no check can tell it from a 1.43C. These texts are the only protection.
  assert.match(setup[1],/1\.43-B/);assert.match(setup[1],/must not use this build/);
  assert.match(profile.features,/1\.43-B/);
  // The status line speaks for this board first. The generic texts name SD, GPS and a 0.13 layout it never had.
  const status=read('../web/app.js').split('\n').find(line=>line.includes("$('board-status').textContent = `${data.version}"));
  assert.match(status,/^[^?]*\$\{data\.version\}\$\{board==='waveshare_143c'\?' · [^']+':/);
  assert.match(status,/board==='waveshare_143c'\?'Update requires this exact 8 MB AMOLED 1\.43C profile, version 0\.14\.7 or newer\. Use Fresh installation /);
});

test('1.43C firmware name, packaging and CI stay tied to the web profile',()=>{
  const pins=read('../firmware/include/board_pins.h').split('#elif defined(BOARD_WAVESHARE_143C)')[1].split(/\n#elif /)[0];
  assert.ok(pins.includes(`#define BOARD_NAME "${profile.firmwareName}"`));
  assert.ok(read('../tools/build_release.py').includes(`'${board}':('${profile.firmwareName}','waveshare-amoled-143c')`));
  // Only a node test can guard this line: the Python script cannot notice that CI stopped running it.
  assert.match(read('../.github/workflows/build.yml'),/python tools\/test_waveshare143c\.py --cxx g\+\+/);
});
