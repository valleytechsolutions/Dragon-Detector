import test from 'node:test';
import assert from 'node:assert/strict';
import {crc32,encodeBadge,cropGeometry,fitCircleZoom} from '../web/badge-codec.js';
import {BadgeSerial} from '../web/serial.js';
import {uploadBadge,BADGE_CHUNK_SIZE} from '../web/badge-transfer.js';
import {readHistory} from '../web/detector-transfer.js';
import {parseBadgeInfo,hasDetectorTools,hasActivityTools} from '../web/firmware-compat.js';
import {readFileSync} from 'node:fs';

test('firmware release updates preserve the independently pinned installer dependency',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  assert.match(html,/https:\/\/unpkg\.com\/esp-web-tools@10\.4\.0\/dist\/web\/install-button\.js\?module/);
});

test('USB handshake accepts current/legacy boards and enables tools for 0.4/0.5/0.6',()=>{
  for(const version of ['0.3.1','0.4.0','0.5.0','0.5.1','0.6.0'])for(const size of [360,466]){
    assert.deepEqual(parseBadgeInfo(`BADGE_INFO ${version} ${size} READY Test board`),{version,size,board:'Test board'});
    assert.equal(hasDetectorTools(version),!version.startsWith('0.3.'));
    assert.equal(hasActivityTools(version),version.startsWith('0.6.'));
  }
  for(const line of ['BADGE_INFO 0.5.0 466 NO_STORAGE Board','BADGE_INFO 0.50.0 466 READY Board','BADGE_INFO 0.5.0 240 READY Board','BADGE_INFO 1.0.0 466 READY Board'])assert.equal(parseBadgeInfo(line),null);
  assert.equal(hasDetectorTools('0.50.0'),false);
});
test('CRC matches firmware IEEE CRC-32 wire contract',()=>assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926));
for(const size of [360,466])test(`DDB1 ${size}: header, exact size and RGB565 byte order`,()=>{
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
