import {BadgeSerial} from './serial.js';
import {encodeBadge, cropGeometry, fitCircleZoom} from './badge-codec.js';
import {uploadBadge} from './badge-transfer.js';
import {readHistory} from './detector-transfer.js';
import {parseBadgeInfo,hasDetectorTools} from './firmware-compat.js';
const $ = id => document.getElementById(id);
const models = {spotpear:'Spotpear / Guition JC3636W518',waveshare:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',stopwatch:'M5Stack StopWatch C152'};
let board = '', releaseReady = false, request = 0;
let installerReady=false,installerFailed=false;
function installEnabled(){
  const supported=!!navigator.serial && window.isSecureContext;
  $('install-button').disabled = !board || !releaseReady || !$('hardware-confirm').checked || !installerReady || !supported;
  if(!supported)$('install-message').textContent='Use desktop Chrome or Edge over HTTPS or localhost for USB installation.';
  else if(!installerReady)$('install-message').textContent=installerFailed?'The USB installer could not load. Check your connection and reload this page.':'Loading the USB installer…';
  else if(releaseReady)$('install-message').textContent='Confirm your board, then connect its USB port.';
}
const installerTimer=setTimeout(()=>{installerFailed=true;installEnabled();},10000);
customElements.whenDefined('esp-web-install-button').then(()=>{clearTimeout(installerTimer);installerReady=true;installEnabled();});
installEnabled();
async function chooseBoard(next) {
  board = next; releaseReady = false; const revision = ++request;
  $('hardware-confirm').checked = false; $('download').hidden = true; installEnabled();
  document.querySelectorAll('[data-board]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.board===board)));
  $('selected-board').textContent = models[board];
  $('board-status').textContent = 'Checking the matching firmware release…';
  const mode = $('install-mode').value, manifest = `firmware/${board}/${mode}.json`;
  try {
    const response = await fetch(manifest, {cache:'no-cache'});
    if(!response.ok)throw new Error('The release is not published at this location yet.');
    const data = await response.json();
    if(revision !== request)return;
    if(!data.builds?.length || data.builds[0].chipFamily!=='ESP32-S3')throw new Error('Invalid release manifest.');
    $('installer').setAttribute('manifest', manifest); releaseReady = true;
    $('board-status').textContent = `${data.version} · ${mode==='factory'?'Fresh installation replaces saved history and badge files.':'Update preserves history, badges and settings; requires this board’s existing 0.3.x / 0.4.x / 0.5.x layout.'}`;
    $('download').href = `firmware/${board}/SHA256SUMS.txt`; $('download').textContent='Firmware file checksums'; $('download').hidden=false;
    $('install-message').textContent='Confirm your board, then connect its USB port.';
  } catch(error) {
    if(revision!==request)return;
    $('board-status').textContent=error.message;
    $('install-message').textContent='Installation stays disabled until matching build files are available.';
  }
  installEnabled();
}
document.querySelectorAll('[data-board]').forEach(b=>b.onclick=()=>chooseBoard(b.dataset.board));
$('hardware-confirm').onchange=installEnabled;
$('install-mode').onchange=()=>{if(board)chooseBoard(board);};

const canvas=$('preview'), ctx=canvas.getContext('2d',{willReadFrequently:true});
let photo=null, size=466, zoom=1, offsetX=0, offsetY=0, serial=null, busy=false, firmwareVersion='';
function controls(){
  for(const id of ['history-export','show-radar','sync-time','display-save','sound-save','tone-load','tone-save','tone-preview','tone-reset','tone-stop'])$(id).disabled=busy||!serial||!hasDetectorTools(firmwareVersion);
  $('connect').disabled=$('detector-connect').disabled=busy||!!serial;
  $('disconnect').disabled=$('detector-disconnect').disabled=busy;
  $('disconnect').hidden=$('detector-disconnect').hidden=!serial;
  if(!serial)$('device-label').textContent='USB NOT CONNECTED';
}
function draw(){
  ctx.fillStyle=$('background').value;ctx.fillRect(0,0,size,size);
  if(photo){const g=cropGeometry(photo.width,photo.height,size,zoom,offsetX,offsetY);ctx.drawImage(photo,g.x,g.y,g.width,g.height);}
  else {ctx.fillStyle='#163b25';ctx.beginPath();ctx.arc(size/2,size/2,size*.39,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#78a05b';ctx.lineWidth=1;for(let n=1;n<4;n++){ctx.beginPath();ctx.arc(size/2,size/2,size*.1*n,0,Math.PI*2);ctx.stroke();}ctx.fillStyle='#c3f66d';ctx.textAlign='center';ctx.font=`bold ${size*.062}px sans-serif`;ctx.fillText('YOUR BADGE',size/2,size*.49);ctx.font=`${size*.027}px sans-serif`;ctx.fillText('CHOOSE AN IMAGE TO BEGIN',size/2,size*.55);}
  $('zoom').value=zoom;$('zoom-value').textContent=Math.round(zoom*100)+'%';
  $('upload').disabled=busy||!serial||!photo;
  controls();
}
function reset(){zoom=1;offsetX=offsetY=0;draw();}
$('image').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  try{if(file.size>20*1024*1024)throw new Error('Choose an image smaller than 20 MB.');
    const next=await createImageBitmap(file,{imageOrientation:'from-image'});
    if(next.width*next.height>40000000){next.close();throw new Error('Choose an image below 40 megapixels.');}
    photo?.close();photo=next;reset();$('studio-status').textContent='Drag your image to center it. The round preview shows the visible display.';
  }catch(error){$('studio-status').textContent=error.message;}
};
$('zoom').oninput=e=>{zoom=Number(e.target.value);draw();};
$('center').onclick=()=>{offsetX=offsetY=0;draw();};
$('fill').onclick=reset;
$('fit').onclick=()=>{if(photo){zoom=fitCircleZoom(photo.width,photo.height,size);$('zoom').min=Math.min(.2,zoom);offsetX=offsetY=0;draw();}};
$('background').oninput=draw;
let drag=null;
canvas.onpointerdown=e=>{canvas.setPointerCapture(e.pointerId);drag={x:e.clientX,y:e.clientY,ox:offsetX,oy:offsetY};};
canvas.onpointermove=e=>{if(!drag)return;const scale=size/canvas.getBoundingClientRect().width;offsetX=drag.ox+(e.clientX-drag.x)*scale;offsetY=drag.oy+(e.clientY-drag.y)*scale;draw();};
canvas.onpointerup=canvas.onpointercancel=()=>{drag=null;};

async function disconnect(){const old=serial;serial=null;firmwareVersion='';if(old)await old.close();$('disconnect').hidden=true;$('device-label').textContent='USB NOT CONNECTED';$('detector-status').textContent='USB disconnected.';draw();}
$('connect').onclick=async()=>{
  if(!navigator.serial){$('detector-status').textContent=$('studio-status').textContent='Web Serial needs desktop Chrome or Edge on HTTPS or localhost.';return;}
  if(busy||serial)return;busy=true;controls();
  let link;
  try {
    link=new BadgeSerial(await navigator.serial.requestPort());await link.open();await link.write('badge info');
    const line=await link.wait(s=>s.startsWith('BADGE_INFO '));
    const match=parseBadgeInfo(line);
    if(!match)throw new Error('Install the current firmware using the matching board profile first.');
    serial=link;firmwareVersion=match.version;const previous=size;size=match.size;canvas.width=canvas.height=size;offsetX*=size/previous;offsetY*=size/previous;
    $('device-label').textContent=`${size} × ${size} · CONNECTED`;$('disconnect').hidden=false;
    $('studio-status').textContent=`${match.board} is ready. Choose and position your image.`;draw();
    if(hasDetectorTools(firmwareVersion)){
      await link.write(`time ${Math.floor(Date.now()/1000)}`);await link.wait(s=>s==='TIME_OK');
      await link.write('display status');const config=await link.wait(s=>s.startsWith('DISPLAY_SETTINGS '));
      const values=/brightness=(\d+) output=-?\d+ dim=(\d+) off=(\d+) dim_level=(\d+)/.exec(config);
      if(values)for(const [i,id] of ['display-brightness','display-dim','display-off','display-level'].entries()){const value=values[i+1];if(!Array.from($(id).options).some(o=>o.value===value))$(id).add(new Option(value,value));$(id).value=value;}
      await loadAudio(link);
      $('detector-status').textContent=`${match.board} connected. UTC clock synchronized. History stays on your device until you download it.`;
    }else $('detector-status').textContent='Connected to 0.3.x. Update to the current firmware for history and detector controls.';
  }catch(error){if(link)await link.close();serial=null;firmwareVersion='';$('studio-status').textContent=error.message;$('detector-status').textContent=error.message;}
  finally{busy=false;draw();}
};
$('detector-connect').onclick=()=>$('connect').onclick();
$('disconnect').onclick=$('detector-disconnect').onclick=disconnect;
$('upload').onclick=async()=>{
  if(!serial||!photo||busy)return;busy=true;const link=serial;
  $('disconnect').disabled=true;draw();
  try{
    const bytes=encodeBadge(ctx.getImageData(0,0,size,size).data,size);
    await uploadBadge(link,bytes,percent=>{
      $('progress').value=percent;$('studio-status').textContent=`Uploading ${percent}% · keep USB connected`;
    });
    $('studio-status').textContent='Saved and displayed. Your badge will return after a restart. Hold the image to open tools.';
  }catch(error){$('studio-status').textContent=error.message+' Your previous saved badge is preserved.';await disconnect();}
  finally{busy=false;$('disconnect').disabled=false;draw();}
};
async function detectorAction(action){
  if(!serial||busy)return;busy=true;draw();
  try{await action(serial);}
  catch(error){$('detector-status').textContent=error.message;}
  finally{busy=false;draw();}
}
$('history-export').onclick=()=>detectorAction(async link=>{
  $('detector-status').textContent='Reading local detection history…';
  const csv=await readHistory(link);
  if(!csv.startsWith('radio,address_type,mac,'))throw new Error('The board returned an invalid CSV header.');
  const url=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download=`dragon-detections-${new Date().toISOString().slice(0,10)}.csv`;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);
  $('detector-status').textContent='History downloaded. Scanning has resumed.';
});
$('sync-time').onclick=()=>detectorAction(async link=>{await link.write(`time ${Math.floor(Date.now()/1000)}`);await link.wait(s=>s==='TIME_OK');$('detector-status').textContent='UTC clock synchronized for new history entries.';});
$('show-radar').onclick=()=>detectorAction(async link=>{await link.write('screen radar');$('detector-status').textContent='Radar view selected.';});
$('display-save').onclick=()=>detectorAction(async link=>{const values=['display-brightness','display-dim','display-off','display-level'].map(id=>Number($(id).value));await link.write('display set '+values.join(' '));const reply=await link.wait(s=>s==='DISPLAY_OK'||s==='DISPLAY_ERROR');if(reply!=='DISPLAY_OK')throw new Error('Display settings were rejected.');$('detector-status').textContent='Display preferences saved on your detector.';});
draw();

const toneFamilies=['Wi-Fi','BLE','Flock / ALPR + Raven focus','Axon / TASER','Meta / camera glasses','Pineapple','Pwnagotchi','Nyanbox','HaleHound','Find My / DULT','Serial module','Flipper','Biscuit','SmartTag','Raven','Google Find Hub','Tile','Ring','Camera vendor','ALPR vendor','Drone Remote ID','iBeacon','Disconnect burst','SSID conflict','BLE advert burst','Dragon fox'];
for(const [i,label] of toneFamilies.entries()){$('tone-family').add(new Option(label,String(i)));$('sound-focus').add(new Option(label,String(i)));}
$('tone-family').value='3';
$('tone-text').dataset.family='';
$('tone-family').onchange=()=>{$('tone-text').value='';$('tone-text').dataset.family='';$('sound-status').textContent='Load this family’s saved tone before editing.';};
$('sound-volume').oninput=()=>{$('sound-volume-label').textContent=$('sound-volume').value+'%';};
async function loadTone(link){const id=$('tone-family').value;await link.write('tone get '+id);const value=await link.wait(s=>s.startsWith(`TONE_VALUE ${id} `)||s===`TONE_VALUE ${id}`||s.startsWith('TONE_ERROR'));if(value.startsWith('TONE_ERROR'))throw new Error(value);$('tone-text').value=value.slice(('TONE_VALUE '+id).length).trim();$('tone-text').dataset.family=id;}
async function loadAudio(link){await link.write('tone settings');const value=await link.wait(s=>s.startsWith('TONE_SETTINGS '));const m=/ready=(\d+) enabled=(\d+) volume=(\d+) hunt=(\d+) focus=(-?\d+) quiet=(\d+)/.exec(value);if(!m)throw new Error('Invalid audio settings response.');$('sound-enabled').checked=m[2]==='1';$('sound-volume').value=m[3];$('sound-volume').oninput();$('sound-hunt').checked=m[4]==='1';$('sound-focus').value=m[5];$('sound-status').textContent=m[1]==='1'?(m[6]==='1'?'Speaker ready. Quiet mode is ON; automatic audio is muted.':'Speaker ready. Audio preferences loaded.'):'The board could not initialize its speaker output.';await loadTone(link);}
async function toneReply(link,command){await link.write(command);const reply=await link.wait(s=>s==='TONE_OK'||s.startsWith('TONE_ERROR'));if(reply!=='TONE_OK')throw new Error(reply.replace('TONE_ERROR ','Tone rejected: '));}
async function audioAction(action){await detectorAction(async link=>{try{await action(link);}catch(error){$('sound-status').textContent=error.message;throw error;}});}
$('sound-save').onclick=()=>audioAction(async link=>{await toneReply(link,`tone config ${Number($('sound-enabled').checked)} ${$('sound-volume').value} ${Number($('sound-hunt').checked)} ${$('sound-focus').value}`);$('sound-status').textContent='Audio preferences saved on the detector.';});
$('tone-load').onclick=()=>audioAction(async link=>{await loadTone(link);$('sound-status').textContent='Saved family tone loaded.';});
function editorTone(){if($('tone-text').dataset.family!==$('tone-family').value)throw new Error('Load the selected family first.');const value=$('tone-text').value.trim();if(value.length>512||/[^\x20-\x7e]/.test(value))throw new Error('Use a single ASCII RTTTL line, up to 512 characters.');return value||'-';}
$('tone-save').onclick=()=>audioAction(async link=>{const value=editorTone();await toneReply(link,`tone set ${$('tone-family').value} ${value}`);$('sound-status').textContent=value==='-'?'This family is now silent.':'Family tone saved. It will survive a restart.';});
$('tone-preview').onclick=()=>audioAction(async link=>{await toneReply(link,`tone preview ${$('tone-family').value} ${editorTone()}`);$('sound-status').textContent='Preview requested on the board speaker at its saved volume.';});
$('tone-reset').onclick=()=>audioAction(async link=>{await toneReply(link,`tone reset ${$('tone-family').value}`);await loadTone(link);$('sound-status').textContent='Default melody restored.';});
$('tone-stop').onclick=()=>audioAction(async link=>{await toneReply(link,'tone stop');$('sound-status').textContent='Current sound stopped. Disable automatic alerts to stay silent.';});
