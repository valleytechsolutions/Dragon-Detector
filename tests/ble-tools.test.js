import test from 'node:test';
import assert from 'node:assert/strict';
import {hasBleTools,observedAddress,iBeaconCommand,parseIBeacon} from '../web/ble-tools.js';
test('BLE tool version gate excludes Lite and old firmware',()=>{
 for(const board of ['waveshare','waveshare_175','stopwatch','spotpear','xiao_round']){assert.equal(hasBleTools(board,'0.13.20'),true);assert.equal(hasBleTools(board,'0.13.19'),false);assert.equal(hasBleTools(board,'0.13.020'),false);}
 assert.equal(hasBleTools('waveshare_lite','0.13.20'),false);
});
test('one explicit observed BLE address is required',()=>{
 assert.equal(observedAddress('E0:B1:C2:D3:E4:F5 1'),'E0:B1:C2:D3:E4:F5 1');
 for(const value of ['', 'all','E0:B1:C2:D3:E4:F5 4','::::: 1','E0:B1:C2:D3:E4:F5 1\nlab replay'])assert.throws(()=>observedAddress(value));
});
test('iBeacon identity validates complete UUID and signed measured power',()=>{
 const uuid='00112233-4455-6677-8899-aabbccddeeff';
 assert.equal(iBeaconCommand(uuid,0,65535,-128),`tool ibeacon ${uuid} 0 65535 -128`);
 for(const fields of [[uuid,'',1,-59],[uuid,1.5,1,-59],[uuid,65536,1,-59],[uuid,1,-1,-59],[uuid,1,1,128],[uuid,1,1,-129],[uuid+'\nlab sound',1,1,-59]])assert.throws(()=>iBeaconCommand(...fields));
 assert.deepEqual(parseIBeacon([`RECON_IBEACON uuid=${uuid} major=8 minor=9 power=-59`]),{uuid,major:'8',minor:'9',power:'-59'});
 assert.throws(()=>parseIBeacon(['RECON_OK']));
});

import {hasRepeatingBeeper} from '../web/ble-tools.js';
test('repeat beeper is available only on full boards with 0.13.22+',()=>{
 for(const board of ['waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear','xiao_round']){
  assert.ok(hasRepeatingBeeper(board,'0.13.22'));assert.ok(hasRepeatingBeeper(board,'0.13.100'));
  for(const version of ['0.13.20','0.13.21','0.12.22','invalid',null])assert.equal(hasRepeatingBeeper(board,version),false);
 }
 assert.equal(hasRepeatingBeeper('waveshare_lite','0.13.22'),false);
});
