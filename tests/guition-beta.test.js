import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from '../web/board-profiles.js';
import {parseBadgeInfo,hasWifiClock} from '../web/firmware-compat.js';
import {hasFullLab,hasFullRecon,hasSessionTools} from '../web/lab-capabilities.js';
import {hasBleTools} from '../web/ble-tools.js';
import {hasWebAudio} from '../web/audio-transfer.js';
import {hasSd,hasGpsAndSd,resourceSetup,resourcePack} from '../web/sd-resources.js';
import {checkRelease} from '../web/install-checks.js';
import {syncInstalledClock} from '../web/install-clock.js';

const board='guition_185_beta',profile=boardProfiles[board];
const fixture=JSON.parse(readFileSync(new URL('./fixtures/guition-beta1-0.14.2-manifests.json',import.meta.url)));

test('Guition Beta 1 requires its exact full USB identity and remains separate from Spotpear',()=>{
  assert.equal(profile.releaseChannel,'beta');assert.equal(profile.betaIteration,1);assert.equal(profile.experimental,true);
  assert.equal(connectedProfile(parseBadgeInfo('BADGE_INFO 0.14.2 360 READY Guition / Surenoo JC3636W518 Beta 1')),board);
  for(const version of ['0.14.1','0.13.42','0.14.02','0.14.2-beta','0.15.0'])assert.equal(parseBadgeInfo(`BADGE_INFO ${version} 360 READY ${profile.firmwareName}`),null);
  for(const size of [240,412,466,480])assert.equal(parseBadgeInfo(`BADGE_INFO 0.14.2 ${size} READY ${profile.firmwareName}`),null);
  for(const state of ['LITE','RADAR_ONLY'])assert.equal(parseBadgeInfo(`BADGE_INFO 0.14.2 360 ${state} ${profile.firmwareName}`),null);
  for(const name of ['Guition JC3636W518','Guition / Surenoo JC3636W518 Beta 2','Guition JC3636K518'])assert.equal(connectedProfile({board:name,size:360}),null);
  assert.equal(connectedProfile(parseBadgeInfo('BADGE_INFO 0.14.2 360 READY Spotpear JC3636W518')),'spotpear');
  assert.equal(boardProfiles.spotpear.firmwareName,'Spotpear JC3636W518');assert.equal(boardProfiles.spotpear.filesystemOffset,0xc10000);
  assert.equal(boardProfiles.spotpear.releaseChannel,undefined);assert.equal(boardProfiles.spotpear.legacyNames,undefined);
});

test('beta manifest metadata and namespaces prevent selecting the original image accidentally',async()=>{
  for(const mode of ['factory','update']){
    const manifest=fixture[mode];assert.equal(validateManifest(manifest,board,mode),manifest);
    assert.equal(manifest.builds[0].parts.some(x=>x.path==='littlefs.bin'),mode==='factory');
    assert.equal(resourceSetup(board,manifest).required,false);
    for(const wrong of ['spotpear','waveshare_185b','waveshare_175'])assert.throws(()=>validateManifest(manifest,wrong,mode),/manifest/);
    for(const change of [{version:'0.14.1'},{release_channel:'stable'},{release_channel:undefined},{beta_iteration:2},{beta_iteration:'1'},{hardware_validation:'verified'},{hardware_validation:undefined}])assert.throws(()=>validateManifest({...manifest,...change},board,mode),/manifest/);
    const renamed={...manifest,name:`Dragon Detector — ${boardProfiles.spotpear.firmwareName} (${mode})`};
    assert.throws(()=>validateManifest(renamed,'spotpear',mode),/beta manifest/);
    const requests=[];
    const result=await checkRelease(board,mode,{fetchImpl:async url=>{requests.push(url);return url.endsWith('.json')?{ok:true,json:async()=>manifest}:{ok:true,headers:new Headers({'content-length':'4096','content-type':'application/octet-stream'})};}});
    assert.equal(result.version,'0.14.2');assert.ok(requests.every(url=>url.startsWith(`firmware/${board}/`)));
    const badRequests=[];
    await assert.rejects(checkRelease(board,mode,{fetchImpl:async url=>{badRequests.push(url);return {ok:true,json:async()=>renamed};}}),/manifest/);
    assert.deepEqual(badRequests,[`firmware/${board}/${mode}.json`]);
  }
});

test('beta capabilities follow its full software profile without inventing GPS or an RTC',()=>{
  for(const capability of [hasFullLab,hasFullRecon,hasSessionTools,hasBleTools,hasWebAudio,hasWifiClock]){
    assert.equal(capability(board,'0.14.2'),true);assert.equal(capability(board,'0.14.1'),false);
  }
  assert.ok(supportsBadge(board)&&hasSd(board));assert.equal(hasGpsAndSd(board),false);
  assert.throws(()=>resourceSetup(board,{version:'0.14.1',sdResources:{...resourcePack}}),/Unsupported/);
  assert.match(profile.features,/Software clock, no external RTC or GPS\/wardrive/);
});

test('post-install beta identity mismatch stops before writing time to a Spotpear device',async()=>{
  const writes=[];let closed=false;
  await assert.rejects(syncInstalledClock({},board,{delay:async()=>{},makeLink:()=>({async open(){},async close(){closed=true;},async write(command){writes.push(command);},async wait(){return 'BADGE_INFO 0.14.2 360 READY Spotpear JC3636W518';}})}),/does not match/);
  assert.deepEqual(writes,['badge info']);assert.equal(closed,true);
});

test('beta has a separate installer, matching guide and unverified status beside unchanged original hardware entry',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  assert.equal((html.match(/data-board="guition_185_beta"/g)||[]).length,1);
  assert.equal((html.match(/data-board="spotpear"/g)||[]).length,1);
  assert.match(html,/value="guition_185_beta"/);assert.match(html,/value="spotpear">Spotpear \/ Guition · 360 × 360/);
  assert.match(html,/data-board="guition_185_beta"[^>]*>[\s\S]*?Physically unverified/);
  assert.match(html,/id="board-guition-beta-setup"[\s\S]*?Beta 1 — physically unverified/);
  assert.match(html,/href="guides\/GUITION-SURENOO-BETA\.html"/);
  assert.match(html,/href="https:\/\/www\.surenoo\.com\/products\/23280116"/);
  assert.match(html,/<b>Spotpear \/ Guition<\/b><span>JC3636W518 · round LCD<\/span><small>16 MB · touch · optional microSD<\/small>/);
});
