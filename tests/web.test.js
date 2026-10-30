import test from 'node:test';
import assert from 'node:assert/strict';
import {crc32,encodeBadge,cropGeometry,fitCircleZoom} from '../web/badge-codec.js';
import {BadgeSerial} from '../web/serial.js';
import {uploadBadge,BADGE_CHUNK_SIZE} from '../web/badge-transfer.js';
import {readHistory} from '../web/detector-transfer.js';
import {parseBadgeInfo,hasDetectorTools,hasActivityTools,hasRadarTools,hasBrandedThemes,hasCommunityThemes,hasRunnerThemes,hasCapsuleSparrowsThemes,hasLplTheme} from '../web/firmware-compat.js';
import {readRadarSettings,saveRadarSettings} from '../web/radar-transfer.js';
import {readFileSync} from 'node:fs';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from '../web/board-profiles.js';
import {badgeTransform,fitBadgeZoom,panFromPointer,rescalePan,badgeTextLayout,drawBadgeText} from '../web/badge-editor.js';
import {deviceQuery} from '../web/device-query.js';

test('stalled settings query retries safely and recovers the studio connection',async()=>{
  const writes=[];let attempts=0;
  const link={async write(c){writes.push(c);},async wait(match){if(++attempts===1)throw new Error('Board response timed out.');const response='CLOCK_STATUS rtc=0 valid=0 epoch=0 offset=0';assert.ok(match(response));return response;}};
  assert.match(await deviceQuery(link,'clock status',s=>s.startsWith('CLOCK_STATUS ')),/^CLOCK_STATUS/);
  assert.deepEqual(writes,['clock status','clock status']);
  await assert.rejects(deviceQuery(link,'badge upload 10 abc',()=>true),/read-only/);
  assert.equal(writes.length,2,'must never retry a binary transfer command');
});
test('settings queries bound retries and stop immediately on USB disconnect',async()=>{
  for(const disconnected of [false,true]){
    let writes=0;const link={failure:disconnected?new Error('USB disconnected'):null,async write(){writes++;},async wait(){throw new Error('Board response timed out.');}};
    await assert.rejects(deviceQuery(link,'badge info',()=>true),/timed out/);
    assert.equal(writes,disconnected?1:3);
  }
});

test('studio reads both legacy paged and 0.9.6+ single-screen radar settings',async()=>{
  for(const tail of ['page=0 pages=2 plotted=6','pages=1 plotted=37 primary=-1','pages=1 plotted=384 primary=12']){
    const link={async write(command){assert.equal(command,'radar status');},async wait(match){const response=`RADAR_SETTINGS theme=2 labels=1 recent=1 ${tail}`;assert.ok(match(response));return response;}};
    assert.deepEqual(await readRadarSettings(link),{theme:'gold',labels:true,recent:true});
  }
  const invalid={async write(){},async wait(){return 'RADAR_SETTINGS theme=45 labels=1 recent=1 pages=1 plotted=0 primary=-1';}};
  await assert.rejects(readRadarSettings(invalid),/Invalid radar settings/);
});

test('badge text stays inside the circular screen at extreme positions and font sizes',()=>{
  assert.deepEqual(badgeTextLayout('',466),[]);
  for(const size of [240,360,466,480])for(const fontPercent of [4,7,14])for(const yPercent of [0,12,50,88,100]){
    const lines=badgeTextLayout('BWM\nDragon Detector\nA long message that must fit\nignored',size,{fontPercent,yPercent});
    assert.equal(lines.length,3);
    for(const line of lines){
      assert.ok(line.maxWidth>0);assert.equal(line.x,size/2);
      assert.ok(Math.hypot(line.maxWidth/2,Math.abs(line.y-size/2)+line.fontSize*.6)<=size*.46+.001);
    }
  }
});
test('badge text is drawn into the exported canvas with a readable outline',()=>{
  const calls=[];const ctx={save(){},restore(){},strokeText(...args){calls.push(['stroke',...args]);},fillText(...args){calls.push(['fill',...args]);}};
  drawBadgeText(ctx,'BWM\nYOUR PAL KAL',466,{color:'#ff0000'});
  assert.equal(ctx.fillStyle,'#ff0000');assert.equal(ctx.strokeStyle,'#000000');
  assert.deepEqual(calls.map(x=>x.slice(0,2)),[['stroke','BWM'],['fill','BWM'],['stroke','YOUR PAL KAL'],['fill','YOUR PAL KAL']]);
});

test('firmware release updates preserve the independently pinned installer dependency',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  assert.match(html,/vendor\/esp-web-tools-10\.4\.0\/install-dialog-im156JnI\.js/);
  assert.doesNotMatch(html,/<script[^>]+src="https:\/\//);
});

test('every board is installable; only full boards have badge preview controls',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  const options=html.match(/<select id="badge-board">([\s\S]*?)<\/select>/)[1];
  for(const id of Object.keys(boardProfiles)){
    assert.ok(html.includes(`data-board="${id}"`),`installer ${id}`);
    assert.equal(options.includes(`value="${id}"`),supportsBadge(id),`badge preview ${id}`);
  }
});

test('firmware CI keeps an independent artifact for every selectable board',()=>{
  const workflow=readFileSync(new URL('../.github/workflows/build.yml',import.meta.url),'utf8');
  const boards=workflow.match(/board: \[([^\]]+)\]/)[1].split(',').map(x=>x.trim());
  assert.deepEqual(boards.sort(),Object.keys(boardProfiles).sort());
  assert.ok(workflow.includes('name: dragon-detector-${{ matrix.board }}'));
  assert.ok(workflow.includes('path: web/firmware/${{ matrix.board }}'));
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

test('branded themes require 0.13.1 firmware',()=>{for(const v of ['0.12.9','0.13.0','invalid'])assert.equal(hasBrandedThemes(v),false);for(const v of ['0.13.1','0.13.12'])assert.equal(hasBrandedThemes(v),true);});
test('Community themes require firmware 0.13.6 while preserving older branded themes',()=>{
  for(const v of ['0.12.9','0.13.0','0.13.5','0.13.06','invalid'])assert.equal(hasCommunityThemes(v),false);
  for(const v of ['0.13.6','0.13.9','0.13.10'])assert.equal(hasCommunityThemes(v),true);
});

test('radar settings read back every write, including when USB acknowledgment is held',async()=>{
  const themes=['dragon','green','gold','valleytech','biscuit','marauder','toool','church','toool-dark','covert','awok','baph0mau5','wdgwars','ghoststrats','n3rdsec','halehound','waveshare','m5stack','lab5','sasquach','nyan','dmac','seeed','dagnazty','octo-pig','deflock','lpl','ghostbusters','lpu','hackazillarex','m33k-t3ch','capsule-corp','sparrows','dzaz','custom','can-commander','ogg','xombi','muldersrevenge','mitm','hypnotic','vectorpotential','argyle','lawlocktools','moki'];
  for(const [index,theme] of themes.entries()){
    const writes=[];let current={theme:index,labels:0,recent:1};
    const link={write:async s=>{writes.push(s);if(s.startsWith('radar labels '))current.labels=Number(s.endsWith('on'));if(s.startsWith('radar recent '))current.recent=Number(s.endsWith('on'));},wait:async match=>{const line=`RADAR_SETTINGS theme=${current.theme} labels=${current.labels} recent=${current.recent} pages=1 plotted=6 primary=-1`;assert.ok(match(line));return line;}};
    assert.equal((await readRadarSettings(link)).theme,theme);
    await saveRadarSettings(link,{theme,labels:true,recent:false});
    assert.deepEqual(writes,['radar status',`radar theme ${theme}`,'radar status','radar labels on','radar status','radar recent off','radar status']);
    assert.deepEqual(current,{theme:index,labels:1,recent:0});
  }
  let writes=[];const link={write:async s=>writes.push(s),wait:async()=> 'RADAR_SETTINGS theme=0 labels=1 recent=0 pages=1 plotted=6 primary=-1'};
  await assert.rejects(saveRadarSettings(link,{theme:'unknown',labels:true,recent:true}),/Invalid/);assert.equal(writes.length,0);
  await assert.rejects(saveRadarSettings(link,{theme:'green',labels:true,recent:true}),/not saved/);assert.deepEqual(writes,['radar theme green','radar status','radar status']);
  for(const theme of ['45','99','100','00','-1']){link.wait=async()=> 'RADAR_SETTINGS theme='+theme+' labels=1 recent=0 pages=1 plotted=6 primary=-1';await assert.rejects(readRadarSettings(link),/Invalid radar settings/);}
});
test('CRC matches firmware IEEE CRC-32 wire contract',()=>assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926));
for(const size of [240,360,466,480])test(`DDB1 ${size}: header, exact size and RGB565 byte order`,()=>{
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

test('badge transfer tolerates split and repeated cumulative ACKs without resending pixels',async()=>{
  const port=mockPort(),serial=new BadgeSerial(port);await serial.open();
  let sent=0;const bytes=new Uint8Array(434320),write=serial.write.bind(serial);
  serial.write=async data=>{
    await write(data);
    if(typeof data==='string'){port.emit('BADGE_READY\n\n');return;}
    const previous=sent;sent+=data.length;
    port.emit(`BADGE_ACK ${previous}\n\nBADGE_ACK ${previous+Math.floor(data.length/2)}\n`);
    port.emit(`BADGE_ACK ${sent}\n\n`);
    if(sent===bytes.length)port.emit('\nBADGE_OK\n\n');
  };
  await uploadBadge(serial,bytes);assert.equal(sent,bytes.length);
  assert.equal(port.writes.length,1+Math.ceil(bytes.length/BADGE_CHUNK_SIZE));
  await serial.close();
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
  const good={name:`Dragon Detector — ${p.firmwareName} (${mode})`,version:p.minVersion||'0.9.4',new_install_prompt_erase:!!p.minVersion||mode==='factory',...(p.minVersion?{new_install_improv_wait_time:0}:{}),builds:[{chipFamily:'ESP32-S3',parts}]};
  assert.equal(validateManifest(good,board,mode),good);
  if(board==='waveshare_lite'){const old=structuredClone(good);old.version='0.9.3';assert.throws(()=>validateManifest(old,board,mode),/0.9.4/);}
  if(board==='xiao_round'){const old=structuredClone(good);old.version='0.9.1';assert.throws(()=>validateManifest(old,board,mode),/0.9.2/);}
  for(const mutate of [d=>d.name+=' wrong',d=>d.builds[0].chipFamily='ESP32',d=>d.builds[0].parts[3].offset=0,d=>d.builds[0].parts[3].path='https://example.com/firmware.bin',d=>d.builds[0].parts.push(d.builds[0].parts[0]),d=>d.builds[0].parts.pop(),d=>d.new_install_prompt_erase=!d.new_install_prompt_erase,d=>d.version='0.14.0']){
   const bad=structuredClone(good);mutate(bad);assert.throws(()=>validateManifest(bad,board,mode),/manifest/);
  }
  if(mode==='factory'){const bad=structuredClone(good);bad.builds[0].parts[4].offset=p.filesystemOffset===0x810000?0xc10000:0x810000;assert.throws(()=>validateManifest(bad,board,mode));}
 }
});

test('rotated and mirrored badge fits preserve every image corner within the safe circle',()=>{
 for(const size of [240,360,466,480])for(const [w,h] of [[800,400],[400,800],[466,466],[2000,50]])for(const turns of [-1,0,1,2,3,4])for(const mirror of [false,true]){
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

test('LockPickingLawyer requires 0.13.8 without enabling it on older firmware',()=>{
  for(const v of ['0.13.6','0.13.7','0.13.06','0.13.08','invalid'])assert.equal(hasLplTheme(v),false);
  for(const v of ['0.13.8','0.13.9','0.13.10'])assert.equal(hasLplTheme(v),true);
});

test('runner and cyber themes require their appended-ID firmware',()=>{for(const v of ['0.13.9','0.13.11','0.13.012','0.14.0','invalid'])assert.equal(hasRunnerThemes(v),false);for(const v of ['0.13.12','0.13.19','0.13.20','0.13.100'])assert.equal(hasRunnerThemes(v),true);});

test('Capsule Corp and Sparrows require firmware with appended theme IDs 31 and 32',()=>{for(const v of ['0.13.14','0.13.015','0.14.0','invalid'])assert.equal(hasCapsuleSparrowsThemes(v),false);for(const v of ['0.13.15','0.13.20','0.13.100'])assert.equal(hasCapsuleSparrowsThemes(v),true);});

test('MuldersreVenge is available from firmware 0.13.24',async()=>{
  const {hasMuldersRevengeTheme}=await import('../web/firmware-compat.js');
  for(const version of ['0.13.24','0.13.25'])assert.equal(hasMuldersRevengeTheme(version),true);
  for(const version of ['0.13.23','0.12.24','0.13.024','0.13.24junk',''])assert.equal(hasMuldersRevengeTheme(version),false);
});

test('MITM theme is offered only on compatible firmware',async()=>{
  const {hasMitmTheme}=await import('../web/firmware-compat.js');
  for(const v of ['0.13.26','0.13.27'])assert.equal(hasMitmTheme(v),true);
  for(const v of ['0.13.24','0.13.25','0.13.026','0.13.26junk',''])assert.equal(hasMitmTheme(v),false);
});

test('Hypnotic theme requires its first supported firmware version',async()=>{
  const {hasHypnoticTheme}=await import('../web/firmware-compat.js');
  for(const v of ['0.13.26','0.13.27'])assert.equal(hasHypnoticTheme(v),true);
  for(const v of ['0.13.25','0.13.026','0.13.26junk',''])assert.equal(hasHypnoticTheme(v),false);
});


test('Dial accepts stable and legacy compact identities without enabling full-board tools',()=>{
 const info=parseBadgeInfo('BADGE_INFO 0.13.26 240 LITE M5Stack Dial Beta');
 assert.equal(connectedProfile(info),'m5dial');assert.equal(supportsBadge('m5dial'),false);
 for(const line of ['BADGE_INFO 0.13.26 240 READY M5Stack Dial Beta','BADGE_INFO 0.13.25 240 LITE M5Stack Dial Beta','BADGE_INFO 0.13.26 466 LITE M5Stack Dial Beta'])assert.equal(parseBadgeInfo(line),null);
 assert.equal(connectedProfile(parseBadgeInfo('BADGE_INFO 0.13.27 240 LITE M5Stack Dial')),'m5dial');
 for(const line of ['BADGE_INFO 0.13.27 240 READY M5Stack Dial','BADGE_INFO 0.13.26 240 LITE M5Stack Dial','BADGE_INFO 0.13.27 466 LITE M5Stack Dial'])assert.equal(parseBadgeInfo(line),null);
 assert.equal(boardProfiles.m5dial.filesystemOffset,0x610000);assert.equal(boardProfiles.m5dial.experimental,false);
});

test('New locksport collection is gated to matching released firmware',async()=>{
  const {hasLocksportCollection}=await import('../web/firmware-compat.js');
  for(const version of ['0.13.26','0.13.37','0.13.38-preview','0.13.038',null,undefined])assert.equal(hasLocksportCollection(version),false);
  for(const version of ['0.13.38','0.13.39','0.13.100'])assert.equal(hasLocksportCollection(version),true);
});
