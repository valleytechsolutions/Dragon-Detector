import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {boardProfiles,validateManifest,connectedProfile} from '../web/board-profiles.js';
import {parseBadgeInfo} from '../web/firmware-compat.js';
import {resourceSetup,hasSd} from '../web/sd-resources.js';
import {p4UpdateSlot} from '../web/p4-install.js';

// The published 0.14.8 P4 Knob manifests, carried over unchanged into every upload.
const fixture=mode=>JSON.parse(readFileSync(new URL(`./fixtures/waveshare-p4-knob-0.14.8-${mode}.json`,import.meta.url)));
const board='waveshare_p4_knob';

test('carried P4 Knob 0.14.8 manifests pass the browser checks for their own slots only',()=>{
  assert.equal(boardProfiles[board].chipFamily,'ESP32-P4');
  validateManifest(fixture('factory'),board,'factory');
  validateManifest(fixture('update'),board,'update');
  validateManifest(fixture('update-app1'),board,'update',1);
  assert.throws(()=>validateManifest(fixture('update-app1'),board,'update',0));
  assert.throws(()=>validateManifest(fixture('update'),board,'update',1));
  assert.throws(()=>validateManifest(fixture('factory'),board,'factory',1),/application slot/);
  assert.throws(()=>validateManifest(fixture('factory'),'waveshare_185b','factory'));
});

test('P4 Knob handshake, SD starter pack and update slot rules',()=>{
  assert.equal(connectedProfile(parseBadgeInfo('BADGE_INFO 0.14.8 466 READY Waveshare ESP32-P4-Knob-Touch-AMOLED-1.75')),board);
  assert.equal(parseBadgeInfo('BADGE_INFO 0.14.7 466 READY Waveshare ESP32-P4-Knob-Touch-AMOLED-1.75'),null);
  assert.ok(hasSd(board));
  assert.match(resourceSetup(board,fixture('factory')).title,/P4 Knob microSD starter pack/);
  assert.equal(p4UpdateSlot('P4_LAYOUT layout=1 revision=301 running=65536 boot=65536'),0);
  assert.equal(p4UpdateSlot('P4_LAYOUT layout=1 revision=301 running=6356992 boot=6356992'),1);
  assert.throws(()=>p4UpdateSlot('P4_LAYOUT layout=1 revision=301 running=65536 boot=6356992'),/OTA boot change/);
  assert.throws(()=>p4UpdateSlot('P4_LAYOUT layout=1 revision=201 running=65536 boot=65536'),/revision 3/);
});
