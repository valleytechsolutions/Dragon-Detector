import test from 'node:test';
import assert from 'node:assert/strict';
import {crc32,encodeBadge,cropGeometry,fitCircleZoom} from '../web/badge-codec.js';
import {BadgeSerial} from '../web/serial.js';
import {uploadBadge,BADGE_CHUNK_SIZE} from '../web/badge-transfer.js';
import {readHistory} from '../web/detector-transfer.js';
import {parseBadgeInfo,hasDetectorTools,hasActivityTools,hasRadarTools} from '../web/firmware-compat.js';
import {readRadarSettings,saveRadarSettings} from '../web/radar-transfer.js';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from '../web/board-profiles.js';
import {badgeTransform,fitBadgeZoom,panFromPointer,rescalePan} from '../web/badge-editor.js';

test('firmware release updates preserve the independently pinned installer dependency',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  assert.match(html,/https:\/\/unpkg\.com\/esp-web-tools@10\.4\.0\/dist\/web\/install-button\.js\?module/);
});

test('every board is installable; only full boards have badge preview controls',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  const options=html.match(/<select id="badge-board">([\s\S]*?)<\/select>/)[1];
  for(const id of Object.keys(boardProfiles)){
    assert.ok(html.includes(`data-board="${id}"`),`installer ${id}`);
    assert.equal(options.includes(`value="${id}"`),supportsBadge(id),`badge preview ${id}`);
  }
});

test('badge chunks stay bounded and terminate with a short USB packet',()=>{
  assert.ok(BADGE_CHUNK_SIZE>64&&BADGE_CHUNK_SIZE<256);
  assert.notEqual(BADGE_CHUNK_SIZE%64,0);
});

test('USB handshake accepts current/legacy boards and gates detector/activity tools',()=>{
  for(const version of ['0.3.1','0.4.0','0.5.0','0.5.1','0.6.0','0.7.0','0.8.0','0.9.0','0.9.2'])for(const size of [360,466]){
    assert.deepEqual(parseBadgeInfo(`BADGE_INFO ${version} ${size} READY Test board`),{version,size,board:'Test board'});
    assert.equal(hasDetectorTools(version),!version.startsWith('0.3.'));
    assert.equal(hasActivityTools(version),/^0\.[6789]\./.test(version));
    assert.equal(hasRadarTools(version),/^0\.[789]\./.test(version));
  }
  for(const line of ['BADGE_INFO 0.5.0 466 NO_STORAGE Board','BADGE_INFO 0.50.0 466 READY Board','BADGE_INFO 0.5.0 240 READY Board','BADGE_INFO 1.0.0 466 READY Board'])assert.equal(parseBadgeInfo(line),null);
  assert.equal(hasDetectorTools('0.50.0'),false);
});

test('radar settings preserve all themes and gate each acknowledged write',async()=>{
  for(const [index,theme] of ['dragon','green','gold'].entries()){
    const writes=[];const replies=[`RADAR_SETTINGS theme=${index} labels=1 recent=0 page=1 pages=2 plotted=6`,`RADAR_THEME ${index}`,'RADAR_LABELS 1','RADAR_RECENT 0'];
    const link={write:async s=>writes.push(s),wait:async match=>{const s=replies.shift();assert.ok(match(s));return s;}};
    const config=await readRadarSettings(link);assert.deepEqual(config,{theme,labels:true,recent:false});await saveRadarSettings(link,config);
    assert.deepEqual(writes,['radar status',`radar theme ${theme}`,'radar labels on','radar recent off']);
  }
  let writes=0;const link={write:async()=>writes++,wait:async()=> 'RADAR_THEME 0'};
  await assert.rejects(saveRadarSettings(link,{theme:'unknown',labels:true,recent:true}),/Invalid/);assert.equal(writes,0);
  await assert.rejects(saveRadarSettings(link,{theme:'green',labels:true,recent:true}),/acknowledged/);assert.equal(writes,1);
  await assert.rejects(readRadarSettings(link),/Invalid radar settings/);
});
test('CRC matches firmware IEEE CRC-32 wire contract',()=>assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926));
for(const size of [240,360,466])test(`DDB1 ${size}: header, exact size and RGB565 byte order`,()=>{
  const p=new Uint8Array(size*size*4);p.set([255,0,0,255,0,255,0,255,0,0,255,255]);const b=encodeBadge(p,size);
  assert.equal(b.length,8+size*size*2);assert.deepEqual([...b.slice(0,8)],[68,68,66,49,size&255,size>>8,size&255,size>>8]);
  assert.deepEqual([...b.slice(8,14)],[248,0,7,224,0,31]);
});
test('fit whole image keeps every corner inside the circular screen',()=>{
  for(const [w,h] of [[800,400],[400,800],[466,466]]){
    const size=466,g=cropGeometry(w,h,size,fitCircleZoom(w,h,size),0,0);
    assert.ok(Math.hypot(g.width/2,g.height/2)<=size/2+1e-8);
  }
});
test('unsupported sizes and truncated pixels rejected',()=>{assert.throws(()=>encodeBadge(new Uint8Array(4),466));assert.throws(()=>encodeBadge(new Uint8Array(4),1));});
test('portrait and landscape crop centers remain centered, pan uses output pixels',()=>{
  assert.deepEqual(cropGeometry(800,400,400,1,0,0),{x:-200,y:0,width:800,height:400});
  assert.deepEqual(cropGeometry(400,800,400,1,12,-9),{x:12,y:-209,width:400,height:800});
});
function mockPort(){
  let controller;const writes=[];
  return {writes,open:async()=>{},close:async()=>{},readable:new ReadableStream({start(c){controller=c;}}),writable:new WritableStream({write(b){writes.push(b);}}),emit(text){controller.enqueue(new TextEncoder().encode(text));},end(){controller.close();}};
}
test('USB reader handles fragmented ACKs, ignores health lines, and keeps early responses',async()=>{
  const port=mockPort(),serial=new BadgeSerial(port);await serial.open();
  port.emit('HEALTH unrelated\nBADGE_AC');port.emit('K 10\nBADGE_OK\n');
  assert.equal(await serial.wait(s=>s.startsWith('BADGE_ACK ')),'BADGE_ACK 10');
  assert.equal(await serial.wait(s=>s==='BADGE_OK'),'BADGE_OK');
  await serial.write('badge info');assert.equal(new TextDecoder().decode(port.writes[0]),'badge info\n');
  await serial.close();
});
test('USB disconnect rejects a pending transfer and permits cleanup',async()=>{
  const port=mockPort(),serial=new BadgeSerial(port);await serial.open();
  const waiting=serial.wait(s=>s==='BADGE_OK');port.end();
  await assert.rejects(waiting,/disconnected/);await serial.close();
});
test('USB missing acknowledgement times out instead of hanging',async()=>{
  const serial=new BadgeSerial(mockPort());await serial.open();
  await assert.rejects(serial.wait(()=>false,20),/timed out/);await serial.close();
});

test('badge transfer fits a 256-byte hardware RX queue and waits for each ACK',async()=>{
  const bytes=new Uint8Array(1031);let sent=0,awaiting=false,ready=false;const progress=[];
  const link={lines:[],async write(data){
    assert.equal(awaiting,false,'must consume response before next write');awaiting=true;
    if(typeof data==='string'){assert.match(data,/^badge upload 1031 [0-9a-f]+$/);ready=true;return;}
    assert.ok(data.length<=256,'original native USB FIFO would drop data');
    assert.ok(data.length<=BADGE_CHUNK_SIZE);sent+=data.length;
  },async wait(match){
    const response=ready?'BADGE_READY':awaiting?`BADGE_ACK ${sent}`:'BADGE_OK';
    ready=false;awaiting=false;assert.ok(match(response),response);return response;
  }};
  await uploadBadge(link,bytes,p=>progress.push(p));assert.equal(sent,1031);
  assert.equal(progress.at(-1),100);assert.equal(progress.length,Math.ceil(1031/BADGE_CHUNK_SIZE));
});

test('badge transfer stops on device errors before sending more image data',async()=>{
  let writes=0;const link={lines:[],write:async()=>{writes++;},wait:async()=>writes===1?'BADGE_READY':'BADGE_ERROR transfer or checksum'};
  await assert.rejects(uploadBadge(link,new Uint8Array(500)),/BADGE_ERROR/);
  assert.equal(writes,2);
});

test('history export preserves all 512 rows even in one USB read',async()=>{
  const port=mockPort(),serial=new BadgeSerial(port);await serial.open();
  const result=readHistory(serial);const rows=Array.from({length:512},(_,i)=>`BLE,0,address-${i},name-${i}`);
  port.emit('HEALTH unrelated\nHISTORY_BEGIN\nradio,address_type,mac,name\n'+rows.join('\n')+'\nHISTORY_END\n');
  assert.equal(await result,'radio,address_type,mac,name\r\n'+rows.join('\r\n')+'\r\n');
  assert.equal(serial.listeners.size,0);await serial.close();
});
test('history export rejects storage failure, timeout and disconnect with cleanup',async()=>{
  for(const mode of ['storage','timeout','disconnect']){
    const port=mockPort(),serial=new BadgeSerial(port);await serial.open();const result=readHistory(serial,20);
    if(mode==='storage')port.emit('HISTORY_ERROR\n');if(mode==='disconnect')port.end();
    await assert.rejects(result,/unavailable|timed out|disconnected/);assert.equal(serial.listeners.size,0);await serial.close();
  }
});

test('same-resolution boards are distinguished by exact USB identity',()=>{
 assert.deepEqual(parseBadgeInfo('BADGE_INFO 0.9.2 240 READY Seeed XIAO ESP32-S3 + Round Display'),{version:'0.9.2',size:240,board:'Seeed XIAO ESP32-S3 + Round Display'});
 for(const [board,p] of Object.entries(boardProfiles)){
  assert.equal(connectedProfile({board:p.firmwareName,size:p.size}),board);
  assert.equal(connectedProfile({board:p.firmwareName,size:128}),null);
 }
 assert.equal(connectedProfile({board:'Unrecognized AMOLED',size:466}),null);
});

test('flasher rejects wrong boards, partitions, remote paths and destructive update manifests',()=>{
 for(const [board,p] of Object.entries(boardProfiles))for(const mode of ['factory','update']){
  const parts=[{path:'bootloader.bin',offset:0},{path:'partitions.bin',offset:0x8000},{path:'boot_app0.bin',offset:0xe000},{path:'firmware.bin',offset:0x10000}];
  if(mode==='factory')parts.push({path:'littlefs.bin',offset:p.filesystemOffset});
  const good={name:`Dragon Detector — ${p.firmwareName} (${mode})`,version:'0.9.4',new_install_prompt_erase:mode==='factory',builds:[{chipFamily:'ESP32-S3',parts}]};
  assert.equal(validateManifest(good,board,mode),good);
  if(board==='waveshare_lite'){const old=structuredClone(good);old.version='0.9.3';assert.throws(()=>validateManifest(old,board,mode),/0.9.4/);}
  if(board==='xiao_round'){const old=structuredClone(good);old.version='0.9.1';assert.throws(()=>validateManifest(old,board,mode),/0.9.2/);}
  for(const mutate of [d=>d.name+=' wrong',d=>d.builds[0].chipFamily='ESP32',d=>d.builds[0].parts[3].offset=0,d=>d.builds[0].parts[3].path='https://example.com/firmware.bin',d=>d.builds[0].parts.push(d.builds[0].parts[0]),d=>d.builds[0].parts.pop(),d=>d.new_install_prompt_erase=!d.new_install_prompt_erase,d=>d.version='0.10.0']){
   const bad=structuredClone(good);mutate(bad);assert.throws(()=>validateManifest(bad,board,mode),/manifest/);
  }
  if(mode==='factory'){const bad=structuredClone(good);bad.builds[0].parts[4].offset=p.filesystemOffset===0x810000?0xc10000:0x810000;assert.throws(()=>validateManifest(bad,board,mode));}
 }
});

test('rotated and mirrored badge fits preserve every image corner within the safe circle',()=>{
 for(const size of [240,360,466])for(const [w,h] of [[800,400],[400,800],[466,466],[2000,50]])for(const turns of [-1,0,1,2,3,4])for(const mirror of [false,true]){
  const g=badgeTransform(w,h,size,{zoom:fitBadgeZoom(w,h,size,turns),turns,mirror});
  for(const x of [-w/2,w/2])for(const y of [-h/2,h/2]){
   const dx=g.scale*(x*g.mirror*Math.cos(g.angle)-y*Math.sin(g.angle)),dy=g.scale*(x*g.mirror*Math.sin(g.angle)+y*Math.cos(g.angle));
   assert.ok(Math.hypot(dx,dy)<=size*.47+1e-8);
  }
  const fill=badgeTransform(w,h,size,{turns});assert.ok(w*fill.scale>=size&&h*fill.scale>=size);
 }
});

test('badge positioning scales with preview size, survives board changes and stays bounded',()=>{
 const start={x:10,y:10,ox:0,oy:0};
 assert.deepEqual(panFromPointer(start,{x:110,y:60},233,466),{x:200,y:100});
 assert.deepEqual(panFromPointer(start,{x:9999,y:-9999},233,466),{x:466,y:-466});
 const pan=rescalePan(46.6,-93.2,466,360);assert.ok(Math.abs(pan.x-36)<1e-8);assert.ok(Math.abs(pan.y+72)<1e-8);
 const back=rescalePan(pan.x,pan.y,360,466);assert.ok(Math.abs(back.x-46.6)<1e-8);
});

test('Lite identifies itself without advertising badge or full-UI capabilities',()=>{
 const name='Waveshare ESP32-S3-LCD-1.28 Lite';
 const info=parseBadgeInfo(`BADGE_INFO 0.9.4 240 RADAR_ONLY ${name}`);
 assert.deepEqual(info,{version:'0.9.4',size:240,board:name});
 assert.equal(connectedProfile(info),'waveshare_lite');
 assert.equal(supportsBadge('waveshare_lite'),false);
 assert.equal(supportsBadge('unknown'),false);
 for(const version of ['0.9.1','0.9.2','0.9.3'])assert.equal(parseBadgeInfo(`BADGE_INFO ${version} 240 RADAR_ONLY ${name}`),null);
 assert.equal(parseBadgeInfo(`BADGE_INFO 0.9.4 240 READY ${name}`),null);
 assert.equal(parseBadgeInfo(`BADGE_INFO 0.9.4 466 RADAR_ONLY ${name}`),null);
 assert.equal(parseBadgeInfo('BADGE_INFO 0.9.4 240 RADAR_ONLY Seeed XIAO ESP32-S3 + Round Display'),null);
});
