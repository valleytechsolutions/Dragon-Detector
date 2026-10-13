import test from 'node:test';
import assert from 'node:assert/strict';
import {crc32,encodeBadge,cropGeometry,fitCircleZoom} from '../web/badge-codec.js';
import {BadgeSerial} from '../web/serial.js';
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
