import test from 'node:test';
import assert from 'node:assert/strict';
import {defaultTheme,encodeTheme,decodeTheme,removeEdgeBackground,THEME_SIZE} from '../web/theme-codec.js';
import {uploadTheme,readTheme,hasPersonalThemes} from '../web/theme-transfer.js';
import {installPersonalTheme} from '../web/theme-install.js';
import {crc32} from '../web/badge-codec.js';
import {boardProfiles} from '../web/board-profiles.js';

const rgba=new Uint8Array(160*160*4);rgba.set([255,128,2,123]);
const bytes=encodeTheme(defaultTheme,rgba);
const resign=b=>{new DataView(b.buffer,b.byteOffset,b.byteLength).setUint32(b.length-4,crc32(b.subarray(0,-4)),true);return b;};

test('persisted theme checksum distinguishes valid designs, excluding their CRC trailers',async()=>{
  const different=encodeTheme({...defaultTheme,name:'Another theme'},rgba);
  assert.notEqual(crc32(bytes.subarray(0,-4)),crc32(different.subarray(0,-4)));
  const r=rig();const write=r.link.write;
  r.link.write=async value=>{await write(value);};
  const wait=r.link.wait;r.link.wait=async match=>{
    const reply=await wait(match);
    return reply.startsWith('THEME_INFO')?reply.replace(/crc=[a-f\d]+/,`crc=${crc32(different.subarray(0,-4)).toString(16).padStart(8,'0')}`):reply;
  };
  await assert.rejects(uploadTheme(r.link,bytes),/did not match/);
});
test('portable personal theme round-trips every layout/style and preserves alpha',()=>{
  for(let layout=0;layout<3;layout++)for(let hands=0;hands<4;hands++)for(let font=0;font<3;font++)for(let markers=0;markers<4;markers++)for(let sweep=0;sweep<3;sweep++){
    const theme={...defaultTheme,name:'My <theme> & logo',layout,hands,font,markers,sweep,logoSize:2,locksport:true,hour24:false};
    const packed=encodeTheme(theme,rgba),out=decodeTheme(packed);assert.equal(packed.length,THEME_SIZE);assert.deepEqual(out.theme,theme);assert.deepEqual(out.rgba,rgba);
  }
});
test('malformed or corrupted personal themes are rejected before any device writes',async()=>{
  for(const size of [0,63,64,THEME_SIZE-1,THEME_SIZE+1])assert.throws(()=>decodeTheme(new Uint8Array(size)));
  for(const offset of [0,4,48,49,50,51,52,56,63,100,THEME_SIZE-1]){const bad=bytes.slice();bad[offset]^=1;assert.throws(()=>decodeTheme(bad));}
  for(const [offset,value] of [[5,3],[6,4],[7,3],[8,4],[9,16],[10,3],[11,3],[24,10],[47,65]]){const bad=bytes.slice();bad[offset]=value;assert.throws(()=>decodeTheme(resign(bad)));}
  for(const patch of [{name:''},{name:'x'.repeat(24)},{name:'é'},{bg:'red'},{hands:1.5},{seconds:1}])assert.throws(()=>encodeTheme({...defaultTheme,...patch},rgba));
  let writes=0;await assert.rejects(uploadTheme({write:async()=>writes++},new Uint8Array(4)));assert.equal(writes,0);
});
test('edge background removal keeps enclosed white logo details and unrelated colors',()=>{
  const image=new Uint8ClampedArray(7*7*4).fill(255);
  for(let y=1;y<6;y++)for(let x=1;x<6;x++)if(x===1||x===5||y===1||y===5)image.set([0,0,0,255],(y*7+x)*4);
  const out=removeEdgeBackground(image,7,7,255);assert.equal(out[3],0);assert.equal(out[(3*7+3)*4+3],255);assert.equal(out[(1*7+1)*4+3],255);assert.equal(image[3],255);
  assert.throws(()=>removeEdgeBackground(image,6,7,255));
});
function rig({errorAt=-1,badCrc=false,board='waveshare',version='0.13.17'}={}){
  const writes=[],queue=[];let received=0,last='';
  const link={lines:[],async open(){writes.push('OPEN');},async close(){writes.push('CLOSE');},async write(value){
    if(typeof value==='string'){last=value;writes.push(value);
      if(value.startsWith('theme upload '))queue.push('THEME_READY');
      if(value==='theme info')queue.push(`THEME_INFO format=1 saved=1 bytes=${THEME_SIZE} crc=${(badCrc?1:crc32(bytes.subarray(0,-4))).toString(16).padStart(8,'0')} storage=1`);
      if(value==='badge info')queue.push(`BADGE_INFO ${version} ${boardProfiles[board].size} ${boardProfiles[board].lite?'LITE':'READY'} ${boardProfiles[board].firmwareName}`);
      if(value.startsWith('theme read ')){const [, ,at,n]=value.split(' ');queue.push(`THEME_DATA ${at} `+Buffer.from(bytes.subarray(+at,+at + +n)).toString('hex'));}
    }else{assert.ok(value.length<=127);assert.deepEqual(value,bytes.subarray(received,received+value.length));received+=value.length;queue.push(received>=errorAt&&errorAt>=0?'THEME_ERROR write':`THEME_ACK ${received}`);if(received===THEME_SIZE)queue.push('THEME_OK');}
  },async wait(match){const index=queue.findIndex(match);assert.ok(index>=0,`${last}: no matching reply among ${queue}`);return queue.splice(index,1)[0];}};
  return {link,writes,get received(){return received;}};
}
test('upload acknowledges every short USB chunk and checks persisted checksum',async()=>{
  const r=rig(),progress=[];await uploadTheme(r.link,bytes,n=>progress.push(n));assert.equal(r.received,THEME_SIZE);assert.equal(progress.at(-1),100);assert.ok(progress.slice(0,-1).every(p=>p<=95));assert.equal(r.writes.at(-1),'theme info');
  const failure=rig({errorAt:127});await assert.rejects(uploadTheme(failure.link,bytes),/THEME_ERROR/);assert.equal(failure.received,127);
  await assert.rejects(uploadTheme(rig({badCrc:true}).link,bytes),/did not match/);
});
test('readback is complete, checksum-checked and portable',async()=>{assert.deepEqual(await readTheme(rig().link),bytes);await assert.rejects(readTheme(rig({badCrc:true}).link),/checksum/);});
test('post-flash theme install verifies all board identities and closes USB',async()=>{
  for(const board of Object.keys(boardProfiles)){const r=rig({board,version:boardProfiles[board].minVersion||'0.13.17'});await installPersonalTheme({},board,bytes,()=>{},{makeLink:()=>r.link});assert.deepEqual(r.writes.slice(0,2),['OPEN','badge info']);assert.equal(r.writes.at(-1),'CLOSE');}
  for(const options of [{board:'spotpear'},{version:'0.13.16'}]){const r=rig(options);await assert.rejects(installPersonalTheme({},'waveshare',bytes,()=>{},{makeLink:()=>r.link}),/does not match/);assert.deepEqual(r.writes,['OPEN','badge info','CLOSE']);}
  assert.equal(hasPersonalThemes('0.13.16'),false);assert.equal(hasPersonalThemes('0.13.17'),true);assert.equal(hasPersonalThemes('garbage'),false);
});
