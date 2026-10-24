import test from 'node:test';
import assert from 'node:assert/strict';
import {parseBadgeInfo} from '../web/firmware-compat.js';
import {connectedProfile,validateManifest} from '../web/board-profiles.js';
import {hasSd,hasGpsAndSd} from '../web/sd-resources.js';
import {hasFullLab} from '../web/lab-capabilities.js';
import {readFileSync} from 'node:fs';
const fixtures=JSON.parse(readFileSync(new URL('./fixtures/watcher-0.13.29-manifests.json',import.meta.url)));
test('Watcher identity requires its exact size, version and name',()=>{
 const info=parseBadgeInfo('BADGE_INFO 0.13.29 412 READY Seeed SenseCAP Watcher');assert.equal(connectedProfile(info),'watcher');
 for(const line of ['BADGE_INFO 0.13.28 412 READY Seeed SenseCAP Watcher','BADGE_INFO 0.13.29 412 READY Other board','BADGE_INFO 0.13.29 412 LITE Seeed SenseCAP Watcher'])assert.equal(parseBadgeInfo(line),null);
 assert.equal(connectedProfile({...info,size:466}),null);assert.ok(hasSd('watcher'));assert.ok(hasGpsAndSd('watcher'));assert.ok(hasFullLab('watcher','0.13.29'));
});
test('Watcher factory and update manifests preserve exact partitions',()=>{
 for(const mode of ['factory','update']){assert.equal(validateManifest(fixtures[mode],'watcher',mode),fixtures[mode]);const bad=structuredClone(fixtures[mode]);bad.builds[0].parts[3].offset=0x20000;assert.throws(()=>validateManifest(bad,'watcher',mode));}
 assert.ok(!fixtures.update.builds[0].parts.some(p=>p.path==='littlefs.bin'));
 assert.equal(fixtures.factory.builds[0].parts.at(-1).offset,0xc10000);
});
