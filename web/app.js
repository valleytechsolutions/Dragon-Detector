import {localClockEpoch,setClock,readWearable,setOrientation} from './wearable-transfer.js';
import {BadgeSerial} from './serial.js';
import {encodeBadge} from './badge-codec.js';
import {uploadBadge} from './badge-transfer.js';
import {readHistory} from './detector-transfer.js';
import {parseBadgeInfo,hasDetectorTools,hasActivityTools,hasRadarTools,hasWearableTools} from './firmware-compat.js?v=0.9.4';
import {readRadarSettings,saveRadarSettings} from './radar-transfer.js';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from './board-profiles.js?v=0.9.4';
import {badgeTransform,fitBadgeZoom,panFromPointer,rescalePan} from './badge-editor.js';
const $ = id => document.getElementById(id);
const models = Object.fromEntries(Object.entries(boardProfiles).map(([id,p])=>[id,p.name]));
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
  if(!serial)setBadgeProfile(next);
  $('hardware-confirm').checked = false; $('download').hidden = true; installEnabled();
  document.querySelectorAll('[data-board]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.board===board)));
  $('selected-board').textContent = models[board];
  $('board-status').textContent = 'Checking the matching firmware release…';
  const mode = $('install-mode').value, manifest = `firmware/${board}/${mode}.json`;
  try {
    const response = await fetch(manifest, {cache:'no-cache'});
    if(!response.ok)throw new Error('The release is not published at this location yet.');
    const data = validateManifest(await response.json(),board,mode);
    if(revision !== request)return;
    $('installer').setAttribute('manifest', manifest); releaseReady = true;
    $('board-status').textContent = `${data.version} · ${mode==='factory'?'Fresh installation replaces saved history and badge files.':board==='waveshare_lite'?'Update requires this exact Lite profile’s 0.9.4+ layout. For an older third-party port, choose Fresh installation unless its partitions have been verified.':board==='xiao_round'?'Update preserves files; requires this XIAO + Round Display profile’s 0.9.2 or newer 8 MB layout.':'Update preserves history, badges and settings; requires this board’s existing 0.3.x / 0.4.x / 0.5.x / 0.6.x / 0.7.x / 0.8.x / 0.9.x layout.'}`;
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
const guideCanvas=$('preview-guides'),guideCtx=guideCanvas.getContext('2d');
let photo=null,size=466,zoom=1,offsetX=0,offsetY=0,turns=0,mirror=false,serial=null,busy=false,firmwareVersion='',deviceProfile='',badgeProfile='stopwatch',photoName='',imageRequest=0;
function setBadgeProfile(id){
  if(!supportsBadge(id))return;
  const previous=size;badgeProfile=id;size=boardProfiles[id].size;
  ({x:offsetX,y:offsetY}=rescalePan(offsetX,offsetY,previous,size));
  canvas.width=canvas.height=guideCanvas.width=guideCanvas.height=size;
  $('badge-board').value=id;draw();
}
function controls(){
  const lite=!!boardProfiles[deviceProfile]?.lite;
  for(const id of ['show-stopwatch','show-clock','clock-save','clock-sync','orientation-save'])$(id).disabled=busy||!serial||lite||!hasWearableTools(firmwareVersion);
  $('show-activity').disabled=busy||!serial||lite||!hasActivityTools(firmwareVersion);
  $('radar-save').disabled=busy||!serial||lite||!hasRadarTools(firmwareVersion);
  for(const id of ['history-export','show-radar','sync-time','display-save','sound-save','tone-load','tone-save','tone-preview','tone-reset','tone-stop'])$(id).disabled=busy||!serial||lite||!hasDetectorTools(firmwareVersion);
  if(deviceProfile==='xiao_round'||lite)for(const id of ['sound-save','tone-load','tone-save','tone-preview','tone-reset','tone-stop'])$(id).disabled=true;
  $('connect').disabled=$('detector-connect').disabled=busy||!!serial;
  $('disconnect').disabled=$('detector-disconnect').disabled=busy;
  $('disconnect').hidden=$('detector-disconnect').hidden=!serial;
  if(!serial)$('device-label').textContent='USB NOT CONNECTED';
  $('badge-board').disabled=busy||!!serial;
  for(const id of ['image','background','guides'])$(id).disabled=busy;
  for(const id of ['zoom','pan-x','pan-y','center','fill','fit','rotate-left','rotate-right','mirror','reset-art','download-badge'])$(id).disabled=busy||!photo;
  document.querySelectorAll('[data-nudge]').forEach(b=>b.disabled=busy||!photo);
}
function draw(){
  ctx.fillStyle=$('background').value;ctx.fillRect(0,0,size,size);
  if(photo){const g=badgeTransform(photo.width,photo.height,size,{zoom,x:offsetX,y:offsetY,turns,mirror});ctx.save();ctx.translate(g.cx,g.cy);ctx.rotate(g.angle);ctx.scale(g.scale*g.mirror,g.scale);ctx.drawImage(photo,-photo.width/2,-photo.height/2);ctx.restore();}
  else {ctx.fillStyle='#163b25';ctx.beginPath();ctx.arc(size/2,size/2,size*.39,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#78a05b';ctx.lineWidth=1;for(let n=1;n<4;n++){ctx.beginPath();ctx.arc(size/2,size/2,size*.1*n,0,Math.PI*2);ctx.stroke();}ctx.fillStyle='#c3f66d';ctx.textAlign='center';ctx.font=`bold ${size*.062}px sans-serif`;ctx.fillText('YOUR BADGE',size/2,size*.49);ctx.font=`${size*.027}px sans-serif`;ctx.fillText('CHOOSE AN IMAGE TO BEGIN',size/2,size*.55);}
  guideCtx.clearRect(0,0,size,size);
  if($('guides').checked){guideCtx.strokeStyle='#ffffff99';guideCtx.lineWidth=Math.max(1,size/360);guideCtx.setLineDash([size/60,size/90]);guideCtx.beginPath();guideCtx.arc(size/2,size/2,size*.47,0,Math.PI*2);guideCtx.stroke();guideCtx.setLineDash([]);guideCtx.beginPath();guideCtx.moveTo(size*.44,size/2);guideCtx.lineTo(size*.56,size/2);guideCtx.moveTo(size/2,size*.44);guideCtx.lineTo(size/2,size*.56);guideCtx.stroke();}
  $('zoom').min=photo?Math.min(.05,fitBadgeZoom(photo.width,photo.height,size,turns)):.05;
  $('zoom').value=zoom;$('zoom-value').textContent=Math.round(zoom*100)+'%';
  for(const [axis,value] of [['x',offsetX],['y',offsetY]]){$('pan-'+axis).value=value/size*100;$('pan-'+axis+'-value').textContent=`${Math.round(value)} px`;}
  $('mirror').setAttribute('aria-pressed',String(mirror));$('rotation-value').textContent=`${((turns%4+4)%4)*90}°`;
  const profile=boardProfiles[badgeProfile];
  $('preview-size').textContent=`${profile.name} · ${size} × ${size} · ${profile.display}`;
  $('image-info').textContent=photo?`${photoName} · ${photo.width} × ${photo.height} source pixels`:'Choose a photo or logo. Nothing leaves your browser until you upload to USB.';
  $('upload').disabled=busy||!serial||!photo||deviceProfile!==badgeProfile||!supportsBadge(deviceProfile);
  $('upload').textContent=boardProfiles[deviceProfile]?.lite?'Lite is radar only — no badge upload':serial?`Upload to ${size} × ${size} display ↗`:'Connect your board to upload';
  controls();
}
function fit(){if(!photo)return;zoom=fitBadgeZoom(photo.width,photo.height,size,turns);offsetX=offsetY=0;draw();}
async function loadImage(file){
  if(!file||busy)return;const revision=++imageRequest;
  try{if(file.size>20*1024*1024)throw new Error('Choose an image smaller than 20 MB.');
    const next=await createImageBitmap(file,{imageOrientation:'from-image'});
    if(next.width*next.height>40000000){next.close();throw new Error('Choose an image below 40 megapixels.');}
    if(revision!==imageRequest){next.close();return;}
    photo?.close();photo=next;photoName=file.name;turns=0;mirror=false;fit();$('studio-status').textContent='Whole image fitted inside the circle. Drag to position, or choose Fill circle for a full-screen crop.';
  }catch(error){$('studio-status').textContent=error.message;}
}
$('image').onchange=e=>loadImage(e.target.files[0]);
$('badge-board').onchange=e=>setBadgeProfile(e.target.value);
$('zoom').oninput=e=>{zoom=Number(e.target.value);draw();};
$('center').onclick=()=>{offsetX=offsetY=0;draw();};
$('fill').onclick=()=>{zoom=1;offsetX=offsetY=0;draw();};
$('fit').onclick=fit;
$('rotate-left').onclick=()=>{turns--;draw();};$('rotate-right').onclick=()=>{turns++;draw();};
$('mirror').onclick=()=>{mirror=!mirror;draw();};
$('reset-art').onclick=()=>{turns=0;mirror=false;fit();};
for(const axis of ['x','y'])$('pan-'+axis).oninput=e=>{if(axis==='x')offsetX=Number(e.target.value)*size/100;else offsetY=Number(e.target.value)*size/100;draw();};
function nudge(direction,step=1){const dx=direction==='left'?-step:direction==='right'?step:0,dy=direction==='up'?-step:direction==='down'?step:0;offsetX=Math.max(-size,Math.min(size,offsetX+dx));offsetY=Math.max(-size,Math.min(size,offsetY+dy));draw();}
document.querySelectorAll('[data-nudge]').forEach(b=>b.onclick=e=>nudge(b.dataset.nudge,e.shiftKey?10:1));
$('guides').onchange=draw;
$('background').oninput=draw;
let drag=null;
canvas.onpointerdown=e=>{if(busy||!photo)return;canvas.focus();canvas.setPointerCapture(e.pointerId);drag={x:e.clientX,y:e.clientY,ox:offsetX,oy:offsetY};};
canvas.onpointermove=e=>{if(!drag||busy)return;({x:offsetX,y:offsetY}=panFromPointer(drag,{x:e.clientX,y:e.clientY},canvas.getBoundingClientRect().width,size));draw();};
canvas.onpointerup=canvas.onpointercancel=canvas.onlostpointercapture=()=>{drag=null;};
canvas.onkeydown=e=>{const direction={ArrowLeft:'left',ArrowRight:'right',ArrowUp:'up',ArrowDown:'down'}[e.key];if(photo&&!busy&&direction){e.preventDefault();nudge(direction,e.shiftKey?10:1);}};
for(const event of ['dragenter','dragover'])$('badge-drop').addEventListener(event,e=>{e.preventDefault();if(!busy)$('badge-drop').classList.add('drag-over');});
$('badge-drop').ondragleave=()=>$('badge-drop').classList.remove('drag-over');
$('badge-drop').ondrop=e=>{e.preventDefault();$('badge-drop').classList.remove('drag-over');loadImage(e.dataTransfer.files[0]);};
$('download-badge').onclick=()=>{if(!photo)return;const output=document.createElement('canvas');output.width=output.height=size;const c=output.getContext('2d');c.beginPath();c.arc(size/2,size/2,size/2,0,Math.PI*2);c.clip();c.drawImage(canvas,0,0);output.toBlob(blob=>{const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=`dragon-badge-${badgeProfile}-${size}.png`;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);},'image/png');};

async function disconnect(){const old=serial;serial=null;firmwareVersion='';deviceProfile='';if(old)await old.close();$('disconnect').hidden=true;$('device-label').textContent='USB NOT CONNECTED';$('detector-status').textContent='USB disconnected.';draw();}
$('connect').onclick=async()=>{
  if(!navigator.serial){$('detector-status').textContent=$('studio-status').textContent='Web Serial needs desktop Chrome or Edge on HTTPS or localhost.';return;}
  if(busy||serial)return;busy=true;controls();$('studio-status').textContent=$('detector-status').textContent='Choose your detector’s USB port in the browser prompt. Cancel the prompt to keep editing.';
  let link;
  try {
    link=new BadgeSerial(await navigator.serial.requestPort());await link.open();await link.write('badge info');
    const line=await link.wait(s=>s.startsWith('BADGE_INFO '));
    const match=parseBadgeInfo(line);
    if(!match)throw new Error('Install the current firmware using the matching board profile first.');
    const profile=connectedProfile(match);if(!profile)throw new Error('This board identity or resolution is not supported. Use its exact matching firmware.');
    serial=link;firmwareVersion=match.version;deviceProfile=profile;setBadgeProfile(profile);
    $('device-label').textContent=`${match.size} × ${match.size} · ${boardProfiles[profile].lite?'LITE RADAR':'CONNECTED'}`;$('disconnect').hidden=false;
    if(boardProfiles[profile].lite){
      $('studio-status').textContent=$('detector-status').textContent='Waveshare Lite connected. Always-on passive radar at full brightness, automatic target pages, no touch menus, badge mode, audio or settings. Use the installer above for firmware updates.';
      return;
    }
    $('studio-status').textContent=`${match.board} is ready. Choose and position your image.`;draw();
    if(hasDetectorTools(firmwareVersion)){
      await link.write(`time ${Math.floor(Date.now()/1000)}`);if(await link.wait(s=>s==='TIME_OK'||s==='TIME_ERROR')!=='TIME_OK')throw new Error('Clock sync failed. Check the RTC.');
      await link.write('display status');const config=await link.wait(s=>s.startsWith('DISPLAY_SETTINGS '));
      const values=/brightness=(\d+) output=-?\d+ dim=(\d+) off=(\d+) dim_level=(\d+)/.exec(config);
      if(values)for(const [i,id] of ['display-brightness','display-dim','display-off','display-level'].entries()){const value=values[i+1];if(!Array.from($(id).options).some(o=>o.value===value))$(id).add(new Option(value,value));$(id).value=value;}
      await loadAudio(link);
      if(hasWearableTools(firmwareVersion))await loadWearable(link);
      if(hasRadarTools(firmwareVersion)){const radar=await readRadarSettings(link);$('radar-theme').value=radar.theme;$('radar-labels').checked=radar.labels;$('radar-recent').checked=radar.recent;}
      $('detector-status').textContent=`${match.board} connected. UTC clock synchronized. History stays on your device until you download it.`;
    }else $('detector-status').textContent='Connected to 0.3.x. Update to the current firmware for history and detector controls.';
  }catch(error){if(link)await link.close();serial=null;firmwareVersion='';deviceProfile='';$('studio-status').textContent=error.message;$('detector-status').textContent=error.message;}
  finally{busy=false;draw();}
};
$('detector-connect').onclick=()=>$('connect').onclick();
$('disconnect').onclick=$('detector-disconnect').onclick=disconnect;
$('upload').onclick=async()=>{
  if(!serial||!photo||busy||deviceProfile!==badgeProfile||!supportsBadge(deviceProfile))return;busy=true;const link=serial;
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
$('sync-time').onclick=()=>detectorAction(async link=>{await link.write(`time ${Math.floor(Date.now()/1000)}`);if(await link.wait(s=>s==='TIME_OK'||s==='TIME_ERROR')!=='TIME_OK')throw new Error('Clock sync failed. Check the RTC.');$('detector-status').textContent='UTC clock synchronized for new history entries.';});
$('show-radar').onclick=()=>detectorAction(async link=>{await link.write('screen radar');$('detector-status').textContent='Radar view selected.';});
$('show-activity').onclick=()=>detectorAction(async link=>{await link.write('screen activity');$('detector-status').textContent='Radio Activity selected on the board: deauthentication and BLE spam evidence.';});
$('radar-save').onclick=()=>detectorAction(async link=>{await saveRadarSettings(link,{theme:$('radar-theme').value,labels:$('radar-labels').checked,recent:$('radar-recent').checked});$('detector-status').textContent='Radar theme, labels and recent-signal preferences saved on your detector.';});
$('display-save').onclick=()=>detectorAction(async link=>{const values=['display-brightness','display-dim','display-off','display-level'].map(id=>Number($(id).value));await link.write('display set '+values.join(' '));const reply=await link.wait(s=>s==='DISPLAY_OK'||s==='DISPLAY_ERROR');if(reply!=='DISPLAY_OK')throw new Error('Display settings were rejected.');$('detector-status').textContent='Display preferences saved on your detector.';});
draw();

const toneFamilies=['Wi-Fi','BLE','Flock / ALPR + Raven focus','Axon / TASER','Meta / camera glasses','Pineapple','Pwnagotchi','Nyanbox','HaleHound','Find My / DULT','Serial module','Flipper','Biscuit','SmartTag','Raven','Google Find Hub','Tile','Ring','Camera vendor','ALPR vendor','Drone Remote ID','iBeacon','Disconnect burst','SSID conflict','BLE advert burst','Dragon fox'];
for(const [i,label] of toneFamilies.entries()){$('tone-family').add(new Option(label,String(i)));$('sound-focus').add(new Option(label,String(i)));}
$('tone-family').value='3';
$('tone-text').dataset.family='';
$('tone-family').onchange=()=>{$('tone-text').value='';$('tone-text').dataset.family='';$('sound-status').textContent='Load this family’s saved tone before editing.';};
$('sound-volume').oninput=()=>{$('sound-volume-label').textContent=$('sound-volume').value+'%';};
async function loadTone(link){const id=$('tone-family').value;await link.write('tone get '+id);const value=await link.wait(s=>s.startsWith(`TONE_VALUE ${id} `)||s===`TONE_VALUE ${id}`||s.startsWith('TONE_ERROR'));if(value.startsWith('TONE_ERROR'))throw new Error(value);$('tone-text').value=value.slice(('TONE_VALUE '+id).length).trim();$('tone-text').dataset.family=id;}
async function loadAudio(link){await link.write('tone settings');const value=await link.wait(s=>s.startsWith('TONE_SETTINGS '));const m=/ready=(\d+) enabled=(\d+) volume=(\d+) hunt=(\d+) focus=(-?\d+) quiet=(\d+)/.exec(value);if(!m)throw new Error('Invalid audio settings response.');$('sound-enabled').checked=m[2]==='1';$('sound-volume').value=m[3];$('sound-volume').oninput();$('sound-hunt').checked=m[4]==='1';$('sound-focus').value=m[5];$('sound-status').textContent=m[1]==='1'?(m[6]==='1'?'Speaker ready. Quiet mode is ON; automatic audio is muted.':'Speaker ready. Audio preferences loaded.'):deviceProfile==='xiao_round'?'This board has no speaker. Visual alerts and on-screen signal hunting remain available.':'The board could not initialize its speaker output.';await loadTone(link);}
async function toneReply(link,command){await link.write(command);const reply=await link.wait(s=>s==='TONE_OK'||s.startsWith('TONE_ERROR'));if(reply!=='TONE_OK')throw new Error(reply.replace('TONE_ERROR ','Tone rejected: '));}
async function audioAction(action){await detectorAction(async link=>{try{await action(link);}catch(error){$('sound-status').textContent=error.message;throw error;}});}
$('sound-save').onclick=()=>audioAction(async link=>{await toneReply(link,`tone config ${Number($('sound-enabled').checked)} ${$('sound-volume').value} ${Number($('sound-hunt').checked)} ${$('sound-focus').value}`);$('sound-status').textContent='Audio preferences saved on the detector.';});
$('tone-load').onclick=()=>audioAction(async link=>{await loadTone(link);$('sound-status').textContent='Saved family tone loaded.';});
function editorTone(){if($('tone-text').dataset.family!==$('tone-family').value)throw new Error('Load the selected family first.');const value=$('tone-text').value.trim();if(value.length>512||/[^\x20-\x7e]/.test(value))throw new Error('Use a single ASCII RTTTL line, up to 512 characters.');return value||'-';}
$('tone-save').onclick=()=>audioAction(async link=>{const value=editorTone();await toneReply(link,`tone set ${$('tone-family').value} ${value}`);$('sound-status').textContent=value==='-'?'This family is now silent.':'Family tone saved. It will survive a restart.';});
$('tone-preview').onclick=()=>audioAction(async link=>{await toneReply(link,`tone preview ${$('tone-family').value} ${editorTone()}`);$('sound-status').textContent='Preview requested on the board speaker at its saved volume.';});
$('tone-reset').onclick=()=>audioAction(async link=>{await toneReply(link,`tone reset ${$('tone-family').value}`);await loadTone(link);$('sound-status').textContent='Default melody restored.';});
$('tone-stop').onclick=()=>audioAction(async link=>{await toneReply(link,'tone stop');$('sound-status').textContent='Current sound stopped. Disable automatic alerts to stay silent.';});

async function loadWearable(link){
  const state=await readWearable(link);$('clock-offset').value=state.offset;
  if(state.valid)$('clock-local').value=new Date((state.epoch+state.offset*60)*1000).toISOString().slice(0,19);
  $('orientation-mode').value=state.orientation;
  $('wearable-status').textContent=`${state.rtc?'RTC detected · saved time survives restart':'Software clock · set again after restart'}. Display orientation stays at your saved angle.`;
}
$('clock-sync').onclick=()=>detectorAction(async link=>{await setClock(link,Math.floor(Date.now()/1000),-new Date().getTimezoneOffset());await loadWearable(link);$('detector-status').textContent='Computer time and current UTC offset saved.';});
$('clock-save').onclick=()=>detectorAction(async link=>{const offset=Number($('clock-offset').value);await setClock(link,localClockEpoch($('clock-local').value,offset),offset);await loadWearable(link);$('detector-status').textContent='Local clock saved. Stopwatch elapsed time is unchanged.';});
$('orientation-save').onclick=()=>detectorAction(async link=>{await setOrientation(link,$('orientation-mode').value);await loadWearable(link);$('detector-status').textContent='Orientation preference saved.';});
for(const page of ['stopwatch','clock'])$('show-'+page).onclick=()=>detectorAction(async link=>{await link.write('screen '+page);$('detector-status').textContent=page==='clock'?'Clock displayed.':'Stopwatch displayed. Tap Start, then Lap; Home leaves it running.';});
