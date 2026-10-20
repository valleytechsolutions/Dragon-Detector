import test from 'node:test';
import assert from 'node:assert/strict';
import {boardProfiles,connectedProfile,validateManifest} from '../web/board-profiles.js';
import {hasFullLab,hasFullRecon} from '../web/lab-capabilities.js';
import {hasWifiClock} from '../web/firmware-compat.js';
import {verifyClock} from '../web/clock-verification.js';

const board='waveshare_175',profile=boardProfiles[board];
function manifest(mode){
  const parts=[['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]];
  if(mode==='factory')parts.push(['littlefs.bin',0x810000]);
  return {name:`Dragon Detector — ${profile.firmwareName} (${mode})`,version:'0.13.19',new_install_prompt_erase:true,new_install_improv_wait_time:0,builds:[{chipFamily:'ESP32-S3',parts:parts.map(([path,offset])=>({path,offset}))}]};
}
test('non-C AMOLED stays distinct from same-resolution boards and the 32 MB C layout',()=>{
  assert.equal(profile.experimental,true);
  assert.equal(profile.reconOffset,undefined);
  assert.equal(connectedProfile({board:profile.firmwareName,size:466}),board);
  assert.equal(connectedProfile({board:boardProfiles.waveshare.firmwareName,size:466}),'waveshare');
  for(const mode of ['factory','update']){
    const good=manifest(mode);assert.equal(validateManifest(good,board,mode),good);
    assert.throws(()=>validateManifest(good,'waveshare',mode));
    for(const version of ['0.13.18','0.9.4','0.13.019'])assert.throws(()=>validateManifest({...good,version},board,mode));
    const bad=structuredClone(good);bad.builds[0].parts.push({path:'reconfs.bin',offset:0x1000000});
    assert.throws(()=>validateManifest(bad,board,mode));
  }
  const wrong=manifest('factory');wrong.builds[0].parts.at(-1).offset=0xc10000;
  assert.throws(()=>validateManifest(wrong,board,'factory'));
  assert.ok(hasFullLab(board,'0.13.19')&&hasFullRecon(board,'0.13.19')&&hasWifiClock(board,'0.13.19'));
});
test('non-C USB clock verification rejects missing, stale and stopped RTCs',async()=>{
  const epoch=1792380000;
  for(const [rtc,drift,tick,ok] of [[1,0,1,true],[0,0,1,false],[1,60,1,false],[1,0,0,false]]){
    let hardwareReads=0;
    const link={async write(c){this.command=c;},async wait(match){
      const reply=this.command==='clock status'?`CLOCK_STATUS rtc=${rtc} valid=1 epoch=${epoch} offset=0`:
        this.command==='orientation status'?'ORIENTATION_STATUS manual=1 degrees=0':
        `CLOCK_HARDWARE detected=${rtc} valid=1 epoch=${epoch+drift+(hardwareReads++?tick:0)}`;
      assert.ok(match(reply));return reply;
    }};
    const options={now:()=>{const d=new Date(epoch*1000);d.getTimezoneOffset=()=>0;return d;},delay:async()=>{}};
    if(ok)assert.equal((await verifyClock(link,board,'0.13.19',options)).rtcVerified,true);
    else await assert.rejects(verifyClock(link,board,'0.13.19',options),/RTC/);
  }
});
test('GPS target has a distinct firmware identity and rejects base/C/old manifests',()=>{
  const gps=boardProfiles.waveshare_175_gps;
  assert.equal(gps.filesystemOffset,0x810000);
  assert.equal(connectedProfile({board:gps.firmwareName,size:466}),'waveshare_175_gps');
  for(const mode of ['factory','update']){
    const value=manifest(mode);
    value.name=`Dragon Detector — ${gps.firmwareName} (${mode})`;value.version='0.13.21';
    assert.equal(validateManifest(value,'waveshare_175_gps',mode),value);
    for(const board of ['waveshare','waveshare_175'])assert.throws(()=>validateManifest(value,board,mode));
    for(const version of ['0.13.20','0.13.021'])assert.throws(()=>validateManifest({...value,version},'waveshare_175_gps',mode));
  }
  assert.ok(hasFullLab('waveshare_175_gps','0.13.21')&&hasFullRecon('waveshare_175_gps','0.13.21'));
  assert.ok(hasWifiClock('waveshare_175_gps','0.13.21'));
});
