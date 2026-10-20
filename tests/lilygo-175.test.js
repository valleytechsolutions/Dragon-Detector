import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest} from '../web/board-profiles.js';
import {hasFullLab,hasFullRecon} from '../web/lab-capabilities.js';
import {hasBleTools} from '../web/ble-tools.js';
import {verifyClock} from '../web/clock-verification.js';
const board='lilygo_175',p=boardProfiles[board];
test('LILYGO flasher accepts only its own 16 MB layout and current release',()=>{
  assert.equal(connectedProfile({board:'LilyGo T-Display-S3-AMOLED-1.75',size:466}),board);
  assert.equal(connectedProfile({board:p.firmwareName,size:240}),null);
  for(const mode of ['factory','update']){
    const parts=[['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]];
    if(mode==='factory')parts.push(['littlefs.bin',0x810000]);
    const data={name:`Dragon Detector — ${p.firmwareName} (${mode})`,version:'0.13.25',new_install_prompt_erase:true,new_install_improv_wait_time:0,builds:[{chipFamily:'ESP32-S3',parts:parts.map(([path,offset])=>({path,offset}))}]};
    assert.equal(validateManifest(data,board,mode),data);
    for(const wrong of ['waveshare','waveshare_175','stopwatch'])assert.throws(()=>validateManifest(data,wrong,mode));
    for(const version of ['0.13.20','0.13.24','0.13.025'])assert.throws(()=>validateManifest({...data,version},board,mode));
    const bad=structuredClone(data);bad.builds[0].parts.at(-1).offset=0xc10000;assert.throws(()=>validateManifest(bad,board,mode));
  }
  assert.ok(hasFullLab(board,'0.13.25')&&hasFullRecon(board,'0.13.25')&&hasBleTools(board,'0.13.25'));
});
test('LILYGO clock setup requires a present, current and ticking PCF8563',async()=>{
  const epoch=1792380000;
  for(const [rtc,drift,tick,ok] of [[1,0,1,true],[0,0,1,false],[1,60,1,false],[1,0,0,false]]){
    let reads=0;
    const link={async write(c){this.command=c;},async wait(match){
      const reply=this.command==='clock status'?`CLOCK_STATUS rtc=${rtc} valid=1 epoch=${epoch} offset=0`:
        this.command==='orientation status'?'ORIENTATION_STATUS manual=1 degrees=0':
        `CLOCK_HARDWARE detected=${rtc} valid=1 epoch=${epoch+drift+(reads++?tick:0)}`;
      assert.ok(match(reply));return reply;
    }};
    const options={now:()=>{const d=new Date(epoch*1000);d.getTimezoneOffset=()=>0;return d;},delay:async()=>{}};
    if(ok)assert.equal((await verifyClock(link,board,'0.13.25',options)).rtcVerified,true);
    else await assert.rejects(verifyClock(link,board,'0.13.25',options),/RTC/);
  }
});
test('LILYGO appears once in board selection with its matching setup guide',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  assert.equal((html.match(/data-board="lilygo_175"/g)||[]).length,1);
  assert.match(html,/id="board-lilygo-setup"[^>]*hidden/);
  assert.match(html,/guides\/LILYGO-AMOLED-175.html/);
});
