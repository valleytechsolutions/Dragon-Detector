import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {knownFirmware,firmwareAtLeast} from '../web/firmware-version.js';
import * as compat from '../web/firmware-compat.js';
import {boardProfiles,validateManifest,connectedProfile} from '../web/board-profiles.js';
import {hasBleTools,hasRepeatingBeeper} from '../web/ble-tools.js';
import {hasFullLab,hasFullRecon,hasSessionTools} from '../web/lab-capabilities.js';
import {hasNetworkTools} from '../web/network-tools.js';
import {hasNotebook} from '../web/notebook.js';
import {hasReconTools} from '../web/recon-transfer.js';
import {hasPersonalThemes} from '../web/theme-transfer.js';
import {resourcePack,labelResourcePack,resourceSetup,hasSd} from '../web/sd-resources.js';
const fixture=name=>JSON.parse(readFileSync(new URL(`./fixtures/${name}`,import.meta.url)));
const manifests=fixture('release-0.13.26-manifests.json');
manifests.m5dial=fixture('dial-0.13.27-manifests.json');
manifests.watcher=fixture('watcher-0.13.29-manifests.json');
manifests.waveshare_185b=fixture('waveshare-185b-0.14.1-manifests.json');

test('0.14 keeps every exact board handshake and installation contract',()=>{
 for(const [board,p] of Object.entries(boardProfiles)){
  const version=board==='waveshare_185b'?'0.14.1':'0.14.0';
  assert.equal(connectedProfile(compat.parseBadgeInfo(`BADGE_INFO ${version} ${p.size} ${p.lite?'LITE':'READY'} ${p.firmwareName}`)),board);
  for(const mode of ['factory','update']){
   const m=structuredClone(manifests[board][mode]);m.version=version;
   assert.equal(validateManifest(m,board,mode),m,board);
   for(const mutate of [x=>x.version='0.15.0',x=>x.builds[0].parts[3].offset=0x20000,x=>x.name+=' wrong',x=>x.new_install_prompt_erase=false,x=>x.builds[0].chipFamily='ESP32']){
    const bad=structuredClone(m);mutate(bad);assert.throws(()=>validateManifest(bad,board,mode),board);
   }
   if(hasSd(board))assert.ok(resourceSetup(board,{...m,sdResources:{...(board==='waveshare_185b'?labelResourcePack:resourcePack)}}));
  }
 }
});
test('0.14 enables existing studio features, including repeat beeper with patch zero',()=>{
 const featureNames=['hasDetectorTools','hasActivityTools','hasRadarTools','hasWearableTools','hasRetainedClock','hasHardwareClockCheck','hasBrandedThemes','hasCommunityThemes','hasLplTheme','hasRunnerThemes','hasGreenArtThemes','hasCanCommanderTheme','hasCapsuleSparrowsThemes','hasMuldersRevengeTheme','hasMitmTheme','hasDelta44Theme','hasLocksportCollection'];
 const gates=[...featureNames.map(k=>compat[k]),hasNetworkTools,hasNotebook,hasReconTools,hasPersonalThemes];
 for(const gate of gates){assert.equal(gate('0.14.0'),true);for(const v of ['0.15.0','1.0.0','0.14.00','0.14.0-beta'])assert.equal(gate(v),false);}
 for(const [board,p] of Object.entries(boardProfiles))for(const gate of [hasFullLab,hasFullRecon,hasSessionTools])assert.equal(gate(board,board==='waveshare_185b'?'0.14.1':'0.14.0'),!p.lite);
 assert.ok(hasBleTools('stopwatch','0.14.0'));assert.ok(hasRepeatingBeeper('stopwatch','0.14.0'));
 assert.ok(!hasRepeatingBeeper('stopwatch','0.13.21'));assert.ok(!hasBleTools('m5dial','0.14.0'));
});
test('version comparison preserves minimum patches and rejects unreviewed series',()=>{
 assert.ok(firmwareAtLeast('0.14.0','0.13.999'));assert.ok(!firmwareAtLeast('0.13.41','0.13.42'));
 for(const v of ['0.2.99','0.15.0','1.0.0','0.14.00','0.014.0','x',null])assert.equal(knownFirmware(v),false);
 for(const v of ['0.3.0','0.13.42','0.14.0','0.14.99'])assert.ok(knownFirmware(v));
});
