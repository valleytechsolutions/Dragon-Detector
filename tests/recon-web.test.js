import test from 'node:test';import assert from 'node:assert/strict';
import {crc32} from '../web/ble-lab.js';
import {hasReconTools,captureName,exportCapture,importPortalHtml} from '../web/recon-transfer.js';
import {parseBadgeInfo,hasWearableTools} from '../web/firmware-compat.js';
import {validateManifest,boardProfiles} from '../web/board-profiles.js';
import {hasNetworkTools,networkProfileCommand,networkResultsCsv} from '../web/network-tools.js';
test('Network profiles encode spaces and punctuation without command injection; old firmware stays gated',()=>{assert.ok(hasNetworkTools('0.11.1'));for(const v of ['0.11.0','0.14.0','0.11.01'])assert.ok(!hasNetworkTools(v));assert.equal(networkProfileCommand(0,'A B',''), 'net profile put 0 412042 -');assert.equal(networkProfileCommand(7,'lab','12345678'),'net profile put 7 6c6162 3132333435363738');for(const args of [[8,'x',''],[0,'x\nnet disconnect',''],[0,'x','short'],[0,'x','a'.repeat(64)],[0,'é','']])assert.throws(()=>networkProfileCommand(...args));assert.equal(captureName('net-0123abcd.csv'),'net-0123abcd.csv');});
test('Network result export excludes malformed or formula cells',()=>{assert.equal(networkResultsCsv(['RECON_HOST 192.168.1.1 34','RECON_HOST 999.1.1.1 2','RECON_HOST =CMD 4','RECON_HOST 1.2.3.4 99999']), 'ipv4,rtt_ms\n192.168.1.1,34\n');});
test('0.10 contracts enable supported hardware and keep unknown protocols closed',()=>{assert.ok(hasReconTools('0.10.0'));assert.ok(hasWearableTools('0.10.0'));for(const v of ['0.9.8','0.14.0','1.10.0','0.100.0'])assert.ok(!hasReconTools(v));for(const id of Object.keys(boardProfiles)){const p=boardProfiles[id],state=p.lite?'RADAR_ONLY':'READY';assert.equal(!!parseBadgeInfo(`BADGE_INFO 0.10.0 ${p.size} ${state} ${p.firmwareName}`),!['m5dial','elecrow_21'].includes(id));}
 const p=boardProfiles.waveshare,parts=[{path:'bootloader.bin',offset:0},{path:'partitions.bin',offset:0x8000},{path:'boot_app0.bin',offset:0xe000},{path:'firmware.bin',offset:0x10000}];const m={name:`Dragon Detector — ${p.firmwareName} (update)`,version:'0.10.0',new_install_prompt_erase:false,builds:[{chipFamily:'ESP32-S3',parts}]};validateManifest(m,'waveshare','update');parts.push({path:'littlefs.bin',offset:0xc10000},{path:'reconfs.bin',offset:0x1000000});assert.throws(()=>validateManifest(m,'waveshare','update'));m.name=m.name.replace('(update)','(factory)');m.new_install_prompt_erase=true;validateManifest(m,'waveshare','factory');
});
function fake(lines){let cb,removed=false;return {get removed(){return removed;},onLine(fn){cb=fn;return()=>removed=true;},async write(c){assert.equal(c,'recon get wifi-0123abcd.pcap');for(const line of lines)cb(line);}};}
test('Capture USB checks offsets, lengths and CRC; ignores health output',async()=>{const data=Uint8Array.from({length:2000},(_,i)=>i%256),lines=[`RECON_FILE ${data.length}`,'HEALTH unrelated'];for(let i=0;i<data.length;i+=128)lines.push(`RECON_DATA ${i} ${Buffer.from(data.slice(i,i+128)).toString('hex')}`);lines.push(`RECON_END ${data.length} ${crc32(data).toString(16).padStart(8,'0')}`);const link=fake(lines);assert.deepEqual(await exportCapture(link,'wifi-0123abcd.pcap'),data);assert.ok(link.removed);});
test('Capture export rejects corruption, gaps, huge files, disconnect and write failure',async()=>{for(const lines of [[null],['RECON_FILE 67108865'],['RECON_FILE 1','RECON_DATA 1 00'],['RECON_FILE 1','RECON_DATA 0 0001'],['RECON_FILE 1','RECON_DATA 0 00','RECON_END 1 00000000'],['RECON_ERROR unavailable']]){const link=fake(lines);await assert.rejects(exportCapture(link,'wifi-0123abcd.pcap'));assert.ok(link.removed);}let removed=false;await assert.rejects(exportCapture({onLine(){return()=>removed=true;},async write(){throw Error('write failed');}},'wifi-0123abcd.pcap'),/write failed/);assert.ok(removed);});
test('Only bounded generated capture filenames can be downloaded or removed',()=>{assert.equal(captureName('ble-0123abcd.csv'),'ble-0123abcd.csv');for(const name of ['../profiles/a.ddg','/captures/a.pcap','wifi-test.pcap','wifi-0123abcd.pcap\nmode set 0','ble-0123abcd.csv.part'])assert.throws(()=>captureName(name));});
test('SD segment and WiGLE names are accepted; partial/path-injected names are rejected',()=>{
 for(const name of ['wifi-0123abcd-0000.pcap','ble-0123abcd-0123.csv','wigle-0123abcd-0000.csv'])assert.equal(captureName(name),name);
 for(const name of ['wigle-0123abcd-0000.csv.part','../wardrive/wigle-0123abcd-0000.csv','wigle-0123abcd-0000.csv\nsystem restart'])assert.throws(()=>captureName(name));
});
test('SD downloads exceed the old 2 MiB limit and still verify every chunk and CRC',async()=>{
 const data=Uint8Array.from({length:3*1024*1024},(_,i)=>i%251);let cb,closed=false;
 const link={onLine(fn){cb=fn;return()=>closed=true;},async write(command){
  assert.equal(command,'recon get wifi-0123abcd-0000.pcap');cb(`RECON_FILE ${data.length}`);
  for(let at=0;at<data.length;at+=128)cb(`RECON_DATA ${at} ${Buffer.from(data.subarray(at,at+128)).toString('hex')}`);
  cb(`RECON_END ${data.length} ${crc32(data).toString(16).padStart(8,'0')}`);
 }};
 assert.deepEqual(await exportCapture(link,'wifi-0123abcd-0000.pcap'),data);assert.ok(closed);
 const busy=fake(['WARDRIVE_BUSY stop wardriving first']);
 await assert.rejects(exportCapture(busy,'wifi-0123abcd.pcap'),/WARDRIVE_BUSY/);assert.ok(busy.removed);
});

import {normalizeHex,probeRows,probeCsv} from '../web/recon-analysis.js';
test('recon analysis validates hex and preserves raw probe SSIDs without spreadsheet formulas',()=>{assert.equal(normalizeHex('01:02-03 04'),'01020304');for(const bad of ['0','0 1','01 g2','gg','00'.repeat(513)])assert.throws(()=>normalizeHex(bad));const rows=['RECON_PROBE 10:11:12:13:14:15 ch=6 rssi=-50 age=1 count=4 ssid_hex=3d317fff'];assert.equal(probeRows(rows)[0].hex,'3d317fff');assert.ok(probeCsv(rows).includes("'"+'=1'));assert.ok(hasReconTools('0.11.0'));});

test('HTML import waits for bounded CRC-checked chunk acknowledgements',async()=>{const data=new TextEncoder().encode('<html>Test</html>'),writes=[],responses=['RECON_READY','RECON_ACK '+data.length,'RECON_OK template saved'];const link={lines:[],async write(s){writes.push(s);},async wait(f){const r=responses.shift();assert.ok(f(r));return r;}};await importPortalHtml(link,data);assert.equal(writes.length,2);assert.deepEqual(writes[1],data);await assert.rejects(importPortalHtml(link,new Uint8Array([0])));assert.equal(captureName('portal-0123abcd.csv'),'portal-0123abcd.csv');});

import {hasNotebook,readNotebook,customAdvertCommand} from '../web/notebook.js';
import {hasFullLab,hasFullRecon,hasSessionTools} from '../web/lab-capabilities.js';
import {sessionName,missionCommand,recipePutCommand,sourceMacCommand,validateMissionFile,zipStored} from '../web/mission-tools.js';
import {exportReconFile} from '../web/recon-transfer.js';
test('0.13 labs target exactly four full boards; old boards and unknown protocols stay gated',()=>{
 for(const id of ['waveshare','stopwatch','spotpear','xiao_round']){assert.ok(hasFullLab(id,'0.13.0'));assert.ok(hasFullRecon(id,'0.13.0'));assert.ok(hasSessionTools(id,'0.13.0'));}
 for(const id of ['waveshare_lite','unknown','',null])assert.ok(!hasFullLab(id,'0.13.0'));
 for(const id of ['stopwatch','spotpear','xiao_round'])assert.ok(!hasFullLab(id,'0.12.0'));
 assert.ok(hasFullLab('waveshare','0.9.8'));assert.ok(!hasFullRecon('waveshare','0.9.8'));assert.ok(!hasSessionTools('waveshare','0.12.0'));assert.ok(!hasFullLab('waveshare','0.14.0'));
});
test('mission and recipe commands reject traversal, command injection and unsupported records',()=>{
 assert.equal(sessionName('field_01'),'field_01');assert.equal(missionCommand('field_01','obs.rlog'),'mission get field_01 obs.rlog');assert.equal(recipePutCommand('survey','WAIT 2'),'recipe put survey 574149542032');
 for(const bad of ['../x','x y','x\nTOOL','x'.repeat(33),''])assert.throws(()=>sessionName(bad));assert.throws(()=>missionCommand('x','../../badge.bin'));assert.throws(()=>recipePutCommand('x','a'.repeat(513)));assert.throws(()=>recipePutCommand('x','WAIT 2\0'));
 assert.equal(sourceMacCommand('02:11:22:33:44:55'),'tool source 02:11:22:33:44:55');for(const bad of ['01:11:22:33:44:55','00:00:00:00:00:00','02:11:22:33:44:55\ntool start 1'])assert.throws(()=>sourceMacCommand(bad));
});
test('recon mission ZIP records preserve headers, CRC, central directory offsets and byte boundaries',()=>{
 const manifest={schema:1,mode:'wifi',state:'complete',observations:'obs.rlog',probes:'probes.rlog',relationships:'relations.rlog'};
 const entries=[{name:'test/session.json',data:new TextEncoder().encode(JSON.stringify(manifest))},...Object.entries({'obs.rlog':['RCN1',21],'probes.rlog':['PRB1',45],'relations.rlog':['REL1',12]}).map(([name,[header,length]])=>{const data=new Uint8Array(4+length);data.set(new TextEncoder().encode(header));return {name:'test/'+name,data};})];
 for(const e of entries)validateMissionFile(e.name.slice(5),e.data);assert.throws(()=>validateMissionFile('obs.rlog',new Uint8Array(5)));const zip=zipStored(entries),v=new DataView(zip.buffer);let at=0;for(const e of entries){assert.equal(v.getUint32(at,true),0x04034b50);assert.equal(v.getUint32(at+14,true),crc32(e.data));const n=v.getUint16(at+26,true);assert.equal(new TextDecoder().decode(zip.slice(at+30,at+30+n)),e.name);assert.deepEqual(zip.slice(at+30+n,at+30+n+e.data.length),e.data);at+=30+n+e.data.length;}assert.equal(v.getUint32(at,true),0x02014b50);assert.equal(v.getUint32(zip.length-6,true),at);assert.equal(v.getUint16(zip.length-12,true),4);
});
test('mission export uses the checked recon transport and rejects arbitrary command execution',async()=>{
 let cb;const data=new TextEncoder().encode('RCN1'),link={onLine(f){cb=f;return()=>{};},async write(c){assert.equal(c,'mission get test obs.rlog');for(const s of ['RECON_FILE 4','RECON_DATA 0 52434e31',`RECON_END 4 ${crc32(data).toString(16).padStart(8,'0')}`])cb(s);}};
 assert.deepEqual(await exportReconFile(link,'mission get test obs.rlog'),data);await assert.rejects(exportReconFile(link,'tool start 1'));await assert.rejects(exportReconFile(link,'mission get test ../x'));
});
test('0.12 contracts accept every real board manifest and preserve both Waveshare filesystems',()=>{
 assert.ok(hasNotebook('0.12.0'));assert.ok(hasReconTools('0.12.0'));assert.ok(hasNetworkTools('0.12.0'));assert.ok(!hasNotebook('0.11.1'));
 for(const version of ['0.11.1','0.12.0'])for(const [id,p] of Object.entries(boardProfiles).filter(([,p])=>!p.minVersion))for(const mode of ['update','factory']){
  const parts=[{path:'bootloader.bin',offset:0},{path:'partitions.bin',offset:0x8000},{path:'boot_app0.bin',offset:0xe000},{path:'firmware.bin',offset:0x10000}];
  if(mode==='factory'){parts.push({path:'littlefs.bin',offset:p.filesystemOffset});if(id==='waveshare')parts.push({path:'reconfs.bin',offset:p.reconOffset});}
  const data={name:`Dragon Detector — ${p.firmwareName} (${mode})`,version,new_install_prompt_erase:mode==='factory',builds:[{chipFamily:'ESP32-S3',parts}]};assert.equal(validateManifest(data,id,mode),data);assert.ok(parseBadgeInfo(`BADGE_INFO ${version} ${p.size} ${p.lite?'RADAR_ONLY':'READY'} ${p.firmwareName}`));
 }
});
test('Custom advertising validates lengths before preparing a transmit payload',()=>{
 assert.equal(customAdvertCommand('02 01 06 04 09 4c 61 62'),'tool payload 02010604094c6162');
 for(const x of ['', '02010604094c61','0101ff','gg','ff'.repeat(32),'02 01 06\ntool start 1'])assert.throws(()=>customAdvertCommand(x));
});
test('Notebook CSV streams bounded snapshots and rejects malformed output',async()=>{
 const run=lines=>{let cb;return readNotebook({onLine(fn){cb=fn;return()=>{};},async write(c){assert.equal(c,'notebook probes');for(const l of lines)cb(l);}},'probes');};
 const header='mac,channel,rssi,seen_ms,count,ssid_hex',row='aa:bb:cc:dd:ee:ff,6,-60,1,2,3d434d44';assert.equal(await run(['NOTEBOOK_BEGIN',header,row,'NOTEBOOK_END']),header+'\n'+row+'\n');
 await assert.rejects(run(['NOTEBOOK_BEGIN',header,'=CMD,6,-60,1,2,00','NOTEBOOK_END']));await assert.rejects(run(['NOTEBOOK_BEGIN','oops','NOTEBOOK_END']));await assert.rejects(run(['NOTEBOOK_ERROR memory']));
});
