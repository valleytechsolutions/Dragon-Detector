import test from 'node:test';
import assert from 'node:assert/strict';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from '../web/board-profiles.js';
import {parseBadgeInfo,hasWifiClock} from '../web/firmware-compat.js';
import {encodeBadge} from '../web/badge-codec.js';
import {hasFullLab,hasFullRecon} from '../web/lab-capabilities.js';
import {verifyClock,clockResultText} from '../web/clock-verification.js';

const board='elecrow_21',profile=boardProfiles[board],version='0.13.20';
function manifest(mode){
  const parts=[['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]];
  if(mode==='factory')parts.push(['littlefs.bin',0x810000]);
  return {name:`Dragon Detector — ${profile.firmwareName} (${mode})`,version,new_install_prompt_erase:true,new_install_improv_wait_time:0,builds:[{chipFamily:'ESP32-S3',parts:parts.map(([path,offset])=>({path,offset}))}]};
}

test('Elecrow identity and manifests cannot be confused with another round display',()=>{
  const info=parseBadgeInfo(`BADGE_INFO ${version} 480 READY ${profile.firmwareName}`);
  assert.equal(connectedProfile(info),board);
  assert.equal(profile.experimental,true);
  assert.equal(connectedProfile({...info,size:466}),null);
  for(const name of ['M5Stack StopWatch C152','Spotpear JC3636W518','Unknown'])
    assert.equal(parseBadgeInfo(`BADGE_INFO ${version} 480 READY ${name}`),null);
  for(const old of ['0.13.19','0.13.020','0.9.4'])
    assert.equal(parseBadgeInfo(`BADGE_INFO ${old} 480 READY ${profile.firmwareName}`),null);
  for(const mode of ['factory','update']){
    const good=manifest(mode);
    assert.equal(validateManifest(good,board,mode),good);
    for(const other of Object.keys(boardProfiles).filter(x=>x!==board))
      assert.throws(()=>validateManifest(good,other,mode));
    assert.throws(()=>validateManifest({...good,version:'0.13.19'},board,mode));
    const wrong=structuredClone(good);wrong.builds[0].parts.push({path:'reconfs.bin',offset:0x1000000});
    assert.throws(()=>validateManifest(wrong,board,mode));
  }
  const wrong=manifest('factory');wrong.builds[0].parts.at(-1).offset=0xc10000;
  assert.throws(()=>validateManifest(wrong,board,'factory'));
  assert.ok(supportsBadge(board)&&hasFullLab(board,version)&&hasFullRecon(board,version)&&hasWifiClock(board,version));
});

test('native 480 px badge has correct dimensions and full RGB565 payload',()=>{
  const pixels=new Uint8Array(480*480*4);
  pixels.set([255,0,0,255]);pixels.set([0,255,0,255],pixels.length-4);
  const encoded=encodeBadge(pixels,480);
  assert.deepEqual([...encoded.slice(0,8)],[68,68,66,49,224,1,224,1]);
  assert.equal(encoded.length,8+480*480*2);
  assert.deepEqual([...encoded.slice(8,10)],[248,0]);
  assert.deepEqual([...encoded.slice(-2)],[7,224]);
  assert.throws(()=>encodeBadge(pixels.subarray(4),480));
});

test('Elecrow clock sync verifies software time without claiming RTC retention',async()=>{
  const epoch=1792380000;
  for(const drift of [0,60]){
    const link={async write(c){this.command=c;},async wait(match){
      const response=this.command==='clock status'?`CLOCK_STATUS rtc=0 valid=1 epoch=${epoch+drift} offset=0`:
        this.command==='orientation status'?'ORIENTATION_STATUS manual=1 degrees=0':
        'CLOCK_HARDWARE detected=0 valid=0 epoch=0';
      assert.ok(match(response));return response;
    }};
    const options={now:()=>{const d=new Date(epoch*1000);d.getTimezoneOffset=()=>0;return d;},delay:async()=>{}};
    if(drift)await assert.rejects(verifyClock(link,board,version,options),/readback/);
    else{
      const result=await verifyClock(link,board,version,options);
      assert.equal(result.rtcVerified,false);
      assert.match(clockResultText(result),/after full shutdown/);
    }
  }
});
