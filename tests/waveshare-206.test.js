import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest} from '../web/board-profiles.js';
import {parseBadgeInfo,hasWifiClock} from '../web/firmware-compat.js';
import {hasFullLab,hasFullRecon,hasSessionTools} from '../web/lab-capabilities.js';
import {hasBleTools} from '../web/ble-tools.js';
import {hasWebAudio} from '../web/audio-transfer.js';
import {encodeBadge} from '../web/badge-codec.js';
import {badgeTransform,fitBadgeZoom,badgeTextLayout} from '../web/badge-editor.js';

const board='waveshare_206',profile=boardProfiles[board];
const fixture=JSON.parse(readFileSync(new URL('./fixtures/waveshare-206-0.14.9-manifests.json',import.meta.url),'utf8'));

test('2.06 accepts only its own native identity and 0.14.9-compatible package',()=>{
  assert.equal(connectedProfile(parseBadgeInfo(`BADGE_INFO 0.14.9 410 READY ${profile.firmwareName}`)),board);
  for(const line of [`BADGE_INFO 0.14.8 410 READY ${profile.firmwareName}`,`BADGE_INFO 0.14.9 466 READY ${profile.firmwareName}`,`BADGE_INFO 0.14.9 410 LITE ${profile.firmwareName}`,'BADGE_INFO 0.14.9 410 READY Waveshare ESP32-S3-Touch-AMOLED-1.75C'])assert.equal(parseBadgeInfo(line),null);
  for(const mode of ['factory','update']){
    assert.equal(validateManifest(fixture[mode],board,mode).version,'0.14.9');
    assert.throws(()=>validateManifest(fixture[mode],'waveshare',mode));
    assert.throws(()=>validateManifest({...fixture[mode],version:'0.14.8'},board,mode));
  }
  for(const available of [hasFullLab,hasFullRecon,hasSessionTools,hasBleTools,hasWebAudio,hasWifiClock]){
    assert.ok(available(board,'0.14.9'));assert.equal(available(board,'0.14.8'),false);
  }
});

test('2.06 badge encoding includes every rectangular pixel and rejects mismatched geometry',()=>{
  const rgba=new Uint8ClampedArray(410*502*4);rgba.set([255,0,0,255],rgba.length-4);
  const data=encodeBadge(rgba,410,502);
  assert.equal(data.length,8+410*502*2);
  assert.deepEqual([...data.slice(0,8)],[68,68,66,49,154,1,246,1]);
  assert.deepEqual([...data.slice(-2)],[248,0]);
  assert.throws(()=>encodeBadge(rgba,410));
  assert.throws(()=>encodeBadge(new Uint8ClampedArray(410*503*4),410,503));
});

test('rectangular badge fit, fill and captions include corners without a circular crop',()=>{
  for(const turns of [0,1,2,3]){
    const zoom=fitBadgeZoom(1000,600,410,turns,502);
    const fit=badgeTransform(1000,600,410,{zoom,turns,canvasHeight:502});
    const fill=badgeTransform(1000,600,410,{turns,canvasHeight:502});
    const w=turns%2?600:1000,h=turns%2?1000:600;
    assert.equal(fit.cy,251);assert.ok(w*fit.scale<=410+1e-8&&h*fit.scale<=502+1e-8);
    assert.ok(w*fill.scale>=410-1e-8&&h*fill.scale>=502-1e-8);
  }
  for(const line of badgeTextLayout('CONFERENCE\nDRAGON DETECTOR',410,{yPercent:90,canvasHeight:502})){
    assert.equal(line.maxWidth,369);assert.ok(line.y+line.fontSize<502);
  }
});
