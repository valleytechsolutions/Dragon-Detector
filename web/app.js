import {hasNetworkTools,networkProfileCommand,networkResultsCsv} from './network-tools.js?v=0.12.0';
import {normalizeHex,probeRows,probeCsv} from './recon-analysis.js?v=0.12.0';
import {hasReconTools,reconLines,exportCapture,captureName,importPortalHtml} from './recon-transfer.js?v=0.12.0';
import {decodeProfile,exportProfile,exportLog,importProfile,labLines,safeProfileName} from './ble-lab.js?v=0.12.0';
import {localClockEpoch,setClock,readWearable,setOrientation} from './wearable-transfer.js?v=0.12.0';
import {deviceQuery} from './device-query.js';
import {BadgeSerial} from './serial.js?v=0.12.0';
import {encodeBadge} from './badge-codec.js';
import {uploadBadge} from './badge-transfer.js?v=0.12.0';
import {readHistory} from './detector-transfer.js';
import {parseBadgeInfo,hasDetectorTools,hasActivityTools,hasRadarTools,hasWearableTools} from './firmware-compat.js?v=0.12.0';
import {readRadarSettings,saveRadarSettings} from './radar-transfer.js?v=0.12.0';
import {boardProfiles,connectedProfile,validateManifest,supportsBadge} from './board-profiles.js?v=0.12.0';
import {badgeTransform,fitBadgeZoom,panFromPointer,rescalePan,drawBadgeText} from './badge-editor.js?v=0.12.0';
import {hasNotebook,readNotebook,customAdvertCommand} from './notebook.js?v=0.12.0';
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
    $('board-status').textContent = `${data.version} · ${mode==='factory'?'Fresh installation replaces saved history, badges and BLE profiles.':board==='waveshare'?'32 MB update preserves badge, history, settings and BLE profiles. When upgrading older firmware, initialize new BLE storage once in the lab.':board==='waveshare_lite'?'Update requires this exact Lite profile’s 0.9.4+ layout. For an older third-party port, choose Fresh installation unless its partitions have been verified.':board==='xiao_round'?'Update preserves files; requires this XIAO + Round Display profile’s 0.9.2 or newer 8 MB layout.':'Update preserves history, badges and settings; requires this board’s existing 0.3.x / 0.4.x / 0.5.x / 0.6.x / 0.7.x / 0.8.x / 0.9.x layout.'}`;
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
  const labReady=!!serial&&deviceProfile==='waveshare'&&/^0\.(?:9\.(?:[8-9]|\d{2,})|(?:10|11|12)\.\d+)$/.test(firmwareVersion);
  document.querySelectorAll('#ble-lab button').forEach(b=>b.disabled=busy||!labReady);
  document.querySelectorAll('#radio-tools button').forEach(b=>b.disabled=busy||!serial||deviceProfile!=='waveshare'||!hasReconTools(firmwareVersion));
  document.querySelectorAll('[data-requires="0.11"]').forEach(b=>b.disabled=busy||!serial||deviceProfile!=='waveshare'||!/^0\.(?:11|12)\.\d+$/.test(firmwareVersion));
  document.querySelectorAll('[data-requires="0.11.1"]').forEach(b=>b.disabled=busy||!serial||deviceProfile!=='waveshare'||!hasNetworkTools(firmwareVersion));
  const lite=!!boardProfiles[deviceProfile]?.lite;
  for(const id of ['notebook-open','notebook-probes','notebook-adverts','notebook-memory'])$(id).disabled=busy||!serial||lite||!hasNotebook(firmwareVersion);
  $('custom-advert-save').disabled=busy||!serial||deviceProfile!=='waveshare'||!hasNotebook(firmwareVersion);
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
  for(const id of ['image','stock-bwm','background','guides','badge-text','text-size','text-y','text-color'])$(id).disabled=busy;
  for(const id of ['zoom','pan-x','pan-y','center','fill','fit','rotate-left','rotate-right','mirror','reset-art','download-badge'])$(id).disabled=busy||!photo;
  document.querySelectorAll('[data-nudge]').forEach(b=>b.disabled=busy||!photo);
}
function draw(){
  ctx.fillStyle=$('background').value;ctx.fillRect(0,0,size,size);
  if(photo){const g=badgeTransform(photo.width,photo.height,size,{zoom,x:offsetX,y:offsetY,turns,mirror});ctx.save();ctx.translate(g.cx,g.cy);ctx.rotate(g.angle);ctx.scale(g.scale*g.mirror,g.scale);ctx.drawImage(photo,-photo.width/2,-photo.height/2);ctx.restore();}
  else {ctx.fillStyle='#163b25';ctx.beginPath();ctx.arc(size/2,size/2,size*.39,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#78a05b';ctx.lineWidth=1;for(let n=1;n<4;n++){ctx.beginPath();ctx.arc(size/2,size/2,size*.1*n,0,Math.PI*2);ctx.stroke();}ctx.fillStyle='#c3f66d';ctx.textAlign='center';ctx.font=`bold ${size*.062}px sans-serif`;ctx.fillText('YOUR BADGE',size/2,size*.49);ctx.font=`${size*.027}px sans-serif`;ctx.fillText('CHOOSE AN IMAGE TO BEGIN',size/2,size*.55);}
  drawBadgeText(ctx,$('badge-text').value,size,{fontPercent:$('text-size').value,yPercent:$('text-y').value,color:$('text-color').value});
  $('text-size-value').textContent=$('text-size').value+'%';$('text-y-value').textContent=$('text-y').value+'%';
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
async function loadImage(file,stock=false){
  if(!file||busy)return;const revision=++imageRequest;
  try{if(file.size>20*1024*1024)throw new Error('Choose an image smaller than 20 MB.');
    const next=await createImageBitmap(file,{imageOrientation:'from-image'});
    if(next.width*next.height>40000000){next.close();throw new Error('Choose an image below 40 megapixels.');}
    if(revision!==imageRequest){next.close();return;}
    photo?.close();photo=next;photoName=file.name;turns=0;mirror=false;
    if(stock){zoom=1;offsetX=offsetY=0;draw();$('studio-status').textContent='BWM stock badge centered and sized for the round screen. Add text below, or upload as it is.';}
    else {fit();$('studio-status').textContent='Whole image fitted inside the circle. Drag to position, or choose Fill circle for a full-screen crop.';}
  }catch(error){$('studio-status').textContent=error.message;}
}
$('image').onchange=e=>{const file=e.target.files[0];e.target.value='';loadImage(file);};
async function loadStock(){
  if(busy)return;const revision=++imageRequest;
  try {const response=await fetch(`assets/bwm-badge-${size}.png`);if(!response.ok)throw new Error('The stock badge could not load. Choose your own image or try again.');
    const blob=await response.blob();if(revision!==imageRequest)return;
    await loadImage(new File([blob],'BWM stock badge.png',{type:'image/png'}),true);
  }catch(error){if(revision===imageRequest)$('studio-status').textContent=error.message;}
}
$('stock-bwm').onclick=loadStock;
for(const id of ['badge-text','text-size','text-y','text-color'])$(id).oninput=draw;
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
    link=new BadgeSerial(await navigator.serial.requestPort());await link.open();
    const line=await deviceQuery(link,'badge info',s=>s.startsWith('BADGE_INFO '));
    const match=parseBadgeInfo(line);
    if(!match)throw new Error('Install the current firmware using the matching board profile first.');
    const profile=connectedProfile(match);if(!profile)throw new Error('This board identity or resolution is not supported. Use its exact matching firmware.');
    serial=link;firmwareVersion=match.version;deviceProfile=profile;setBadgeProfile(profile);
    $('device-label').textContent=`${match.size} × ${match.size} · ${boardProfiles[profile].lite?'LITE RADAR':'CONNECTED'}`;$('disconnect').hidden=false;
    if(boardProfiles[profile].lite){
      $('studio-status').textContent=$('detector-status').textContent='Waveshare Lite connected. Always-on passive radar at full brightness, all contacts on one radar screen, no touch menus, badge mode, audio or settings. Use the installer above for firmware updates.';
      return;
    }
    $('studio-status').textContent=`${match.board} is ready. Choose and position your image.`;draw();
    if(hasDetectorTools(firmwareVersion)){
      await link.write(`time ${Math.floor(Date.now()/1000)}`);const clockSynced=await link.wait(s=>s==='TIME_OK'||s==='TIME_ERROR')==='TIME_OK';
      const config=await deviceQuery(link,'display status',s=>s.startsWith('DISPLAY_SETTINGS '));
      const values=/brightness=(\d+) output=-?\d+ dim=(\d+) off=(\d+) dim_level=(\d+)/.exec(config);
      if(values)for(const [i,id] of ['display-brightness','display-dim','display-off','display-level'].entries()){const value=values[i+1];if(!Array.from($(id).options).some(o=>o.value===value))$(id).add(new Option(value,value));$(id).value=value;}
      await loadAudio(link);
      if(hasWearableTools(firmwareVersion))await loadWearable(link);
      if(hasRadarTools(firmwareVersion)){const radar=await readRadarSettings(link);$('radar-theme').value=radar.theme;$('radar-labels').checked=radar.labels;$('radar-recent').checked=radar.recent;}
      $('detector-status').textContent=`${match.board} connected. ${clockSynced?'UTC clock synchronized.':'Clock sync unavailable; badge upload is ready.'} History stays on your device until you download it.`;
    }else $('detector-status').textContent='Connected to 0.3.x. Update to the current firmware for history and detector controls.';
  }catch(error){if(link)await link.close();serial=null;firmwareVersion='';deviceProfile='';$('studio-status').textContent=error.message;$('detector-status').textContent=error.message;}
  finally{busy=false;draw();}
};
$('detector-connect').onclick=()=>$('connect').onclick();
$('disconnect').onclick=$('detector-disconnect').onclick=disconnect;
$('upload').onclick=async()=>{
  if(!serial||!photo||busy||deviceProfile!==badgeProfile||!supportsBadge(deviceProfile))return;busy=true;const link=serial;
  $('disconnect').disabled=true;draw();
  $('progress').value=0;$('studio-status').textContent='Preparing your badge and waiting for the board…';
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
  $('detector-status').textContent='History downloaded. Detection remains active.';
});
$('sync-time').onclick=()=>detectorAction(async link=>{await link.write(`time ${Math.floor(Date.now()/1000)}`);if(await link.wait(s=>s==='TIME_OK'||s==='TIME_ERROR')!=='TIME_OK')throw new Error('Clock sync failed. Check the RTC.');$('detector-status').textContent='UTC clock synchronized for new history entries.';});
$('show-radar').onclick=()=>detectorAction(async link=>{await link.write('screen radar');$('detector-status').textContent='Radar view selected.';});
$('show-activity').onclick=()=>detectorAction(async link=>{await link.write('screen activity');$('detector-status').textContent='Radio Activity selected on the board: deauthentication and BLE spam evidence.';});
$('radar-save').onclick=()=>detectorAction(async link=>{await saveRadarSettings(link,{theme:$('radar-theme').value,labels:$('radar-labels').checked,recent:$('radar-recent').checked});$('detector-status').textContent='Radar theme, labels and recent-signal preferences saved on your detector.';});
$('display-save').onclick=()=>detectorAction(async link=>{const values=['display-brightness','display-dim','display-off','display-level'].map(id=>Number($(id).value));await link.write('display set '+values.join(' '));const reply=await link.wait(s=>s==='DISPLAY_OK'||s==='DISPLAY_ERROR');if(reply!=='DISPLAY_OK')throw new Error('Display settings were rejected.');$('detector-status').textContent='Display preferences saved on your detector.';});
draw();loadStock();

const toneFamilies=['Wi-Fi','BLE','Flock / ALPR + Raven focus','Axon / body cameras','Meta / camera glasses','Pineapple','Pwnagotchi','Nyanbox','HaleHound','Find My / DULT','Serial module','Flipper','Biscuit','SmartTag','Raven / acoustic sensors','Google Find Hub','Tile','Ring','Camera vendor','ALPR vendor','Drone / Remote ID','iBeacon','Disconnect burst','SSID conflict','BLE advert burst','Dragon fox'];
for(const [i,label] of toneFamilies.entries()){$('tone-family').add(new Option(label,String(i)));$('sound-focus').add(new Option(label,String(i)));}
$('tone-family').value='3';
$('tone-text').dataset.family='';
$('tone-family').onchange=()=>{$('tone-text').value='';$('tone-text').dataset.family='';$('sound-status').textContent='Load this family’s saved tone before editing.';};
$('sound-volume').oninput=()=>{$('sound-volume-label').textContent=$('sound-volume').value+'%';};
async function loadTone(link){const id=$('tone-family').value;const value=await deviceQuery(link,'tone get '+id,s=>s.startsWith(`TONE_VALUE ${id} `)||s===`TONE_VALUE ${id}`||s.startsWith('TONE_ERROR'));if(value.startsWith('TONE_ERROR'))throw new Error(value);$('tone-text').value=value.slice(('TONE_VALUE '+id).length).trim();$('tone-text').dataset.family=id;}
async function loadAudio(link){const value=await deviceQuery(link,'tone settings',s=>s.startsWith('TONE_SETTINGS '));const m=/ready=(\d+) enabled=(\d+) volume=(\d+) hunt=(\d+) focus=(-?\d+) quiet=(\d+)/.exec(value);if(!m)throw new Error('Invalid audio settings response.');$('sound-enabled').checked=m[2]==='1';$('sound-volume').value=m[3];$('sound-volume').oninput();$('sound-hunt').checked=m[4]==='1';$('sound-focus').value=m[5];$('sound-status').textContent=m[1]==='1'?(m[6]==='1'?'Speaker ready. Quiet mode is ON; automatic audio is muted.':'Speaker ready. Audio preferences loaded.'):deviceProfile==='xiao_round'?'This board has no speaker. Visual alerts and on-screen signal hunting remain available.':'The board could not initialize its speaker output.';await loadTone(link);}
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

async function labAction(fn){if(!serial||busy)return;busy=true;controls();try{await fn(serial);}catch(e){$('lab-output').textContent=e.message;}finally{busy=false;controls();}}
async function labCommand(link,command){link.lines=[];await link.write(command);const reply=await link.wait(s=>/^LAB_(OK|ERROR|LOG|DIFF)/.test(s),10000);$('lab-output').textContent=reply;if(reply.startsWith('LAB_ERROR'))throw new Error(reply);return reply;}
function showProfile(bytes){
 const p=decodeProfile(bytes),lines=[`${p.name || 'Unnamed peripheral'} · ${p.partial?'Partial capture':'Complete capture'}`,`${p.attributes.length} attributes · ${p.address}`, ''];
 const names={'1800':'Generic Access','1801':'Generic Attribute','180a':'Device Information','180f':'Battery','2a19':'Battery level','2901':'Description','2902':'Notification configuration'};
 const name=a=>names[a.uuid.toLowerCase()]||a.uuid;
 const access=a=>[[2,'Read'],[4,'Write without response'],[8,'Write'],[16,'Notify'],[32,'Indicate']].filter(([bit])=>a.properties&bit).map(([,label])=>label).join(' / ');
 $('lab-attribute').replaceChildren(new Option('Choose a characteristic',''));
 for(const a of p.attributes){
  const status=a.flags&1?'Read unavailable':a.flags&2?'Value truncated':a.flags&4?'Not read':`${a.length} bytes captured`;
  if(a.kind===0)lines.push(`SERVICE ${name(a)}`);
  else{lines.push(`${a.kind===1?'  #':'    Descriptor #'}${a.index} ${name(a)}`,`    ${a.kind===1?access(a)+' · ':''}${status}`);if(a.length)lines.push('    '+a.value.slice(0,32).map(v=>v.toString(16).padStart(2,'0')).join(' ')+(a.length>32?' …':''));}
  if(a.kind===1)$('lab-attribute').add(new Option(`#${a.index} ${name(a)} · ${access(a)} · ${status}`,String(a.index)));
 }
 $('lab-output').textContent=lines.join('\n');return p;
}
function labAttr(){const value=$('lab-attribute').value;if(!/^\d+$/.test(value))throw new Error('Read the workspace and choose a characteristic first.');return value;}
function labSaved(){return safeProfileName($('lab-saved').value);}
$('lab-open').onclick=()=>labAction(async l=>{await l.write('screen lab');$('lab-output').textContent='BLE Lab opened on the display.';});
$('lab-status').onclick=()=>labAction(async l=>{$('lab-output').textContent=(await labLines(l,'lab status')).join('\n');});
$('lab-stop').onclick=()=>labAction(l=>labCommand(l,'lab stop'));
$('lab-refresh').onclick=()=>labAction(async l=>{const rows=await labLines(l,'lab nearby');$('lab-device').replaceChildren(new Option('Choose your peripheral',''));for(const row of rows){const m=/^LAB_DEVICE ([0-9a-f:]{17}) (\d) (-?\d+) (.*)$/i.exec(row);if(m)$('lab-device').add(new Option(`${m[4]||m[1]} · ${m[3]} dBm · ${m[1]}`,`${m[1]} ${m[2]}`));}$('lab-output').textContent='Select your peripheral, then connect. Radar remains active until you connect.';});
$('lab-inspect').onclick=()=>labAction(async l=>{const d=$('lab-device').value;if(!/^[0-9a-f:]{17} [0-3]$/i.test(d))throw new Error('Choose your device first.');await labCommand(l,'lab inspect '+d);$('lab-output').textContent+='\nDiscovery runs on the device. Use Status, then Read workspace when connected.';});
$('lab-workspace').onclick=()=>labAction(async l=>showProfile(await exportProfile(l)));
$('lab-read').onclick=()=>labAction(l=>labCommand(l,'lab read '+labAttr()));
$('lab-subscribe').onclick=()=>labAction(l=>labCommand(l,'lab subscribe '+labAttr()));
$('lab-notify').onclick=()=>labAction(l=>labCommand(l,'lab notify '+labAttr()));
$('lab-events').onclick=()=>labAction(async l=>{$('lab-output').textContent=(await labLines(l,'lab events')).join('\n');});
$('lab-log').onclick=()=>labAction(l=>labCommand(l,'lab log on'));$('lab-log-stop').onclick=()=>labAction(l=>labCommand(l,'lab log off'));
$('lab-demo').onclick=()=>labAction(l=>labCommand(l,'lab demo'));
$('lab-save').onclick=()=>labAction(l=>labCommand(l,'lab save '+safeProfileName($('lab-name').value)));
$('lab-export').onclick=()=>labAction(async l=>{const bytes=await exportProfile(l);showProfile(bytes);const url=URL.createObjectURL(new Blob([bytes],{type:'application/octet-stream'})),a=document.createElement('a');a.href=url;a.download='dragon-gatt-profile.ddg';a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);});
$('lab-import').onclick=()=>labAction(async l=>{const f=$('lab-file').files[0];if(!f||f.size>72136)throw new Error('Choose a valid .ddg profile under 72 KB.');const bytes=new Uint8Array(await f.arrayBuffer());decodeProfile(bytes);await importProfile(l,bytes,p=>{$('lab-output').textContent=`Importing ${p}%`;});showProfile(bytes);});
$('lab-library').onclick=()=>labAction(async l=>{const rows=await labLines(l,'lab list');$('lab-saved').replaceChildren(new Option('Choose a saved profile',''));for(const row of rows){const m=/^LAB_PROFILE ([a-z0-9_-]{1,32})\.ddg (\d+)$/i.exec(row);if(m)$('lab-saved').add(new Option(`${m[1]} · ${m[2]} bytes`,m[1]));}$('lab-output').textContent='Profile library refreshed.';});
$('lab-load').onclick=()=>labAction(l=>labCommand(l,'lab load '+labSaved()));$('lab-compare').onclick=()=>labAction(l=>labCommand(l,'lab compare '+labSaved()));
$('lab-clone').onclick=()=>{if(confirm('Start a local GATT peripheral from the workspace? Radar reception pauses until Stop. The original device is not modified.'))labAction(l=>labCommand(l,'lab clone'));};
$('lab-initialize').onclick=()=>{if(confirm('Initialize the new upper 16 MB BLE Lab partition? Badge, history and settings stay in the lower half. Old data in the upper half will be replaced.'))labAction(l=>labCommand(l,'lab storage initialize'));};

$('lab-log-download').onclick=()=>labAction(async l=>{const csv=await exportLog(l,$('lab-log-file').value==='previous'),url=URL.createObjectURL(new Blob([csv],{type:'text/csv'})),a=document.createElement('a');a.href=url;a.download='dragon-ble-events.csv';a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);$('lab-output').textContent='Saved event log downloaded.';});

async function reconAction(fn){if(!serial||busy||deviceProfile!=='waveshare'||!hasReconTools(firmwareVersion))return;busy=true;controls();try{await fn(serial);}catch(e){$('recon-output').textContent=e.message;}finally{busy=false;controls();}}
async function reconDo(link,command){const rows=await reconLines(link,command);$('recon-output').textContent=rows.join('\n');return rows;}
for(const [id,command] of Object.entries({'recon-open':'screen modes','recon-status':'tool status','recon-stop':'tool stop','recon-record-wifi':'recon capture wifi','recon-record-ble':'recon capture ble','recon-record-stop':'recon capture stop','recon-save':'recon capture save'}))$(id).onclick=()=>reconAction(async l=>{if(command.startsWith('screen ')){await l.write(command);$('recon-output').textContent='Modes opened on the display.';}else await reconDo(l,command);});
$('recon-mode-save').onclick=()=>reconAction(l=>reconDo(l,'mode set '+Number($('recon-mode').value)));
$('recon-tool-list').onclick=()=>reconAction(async l=>{const rows=await reconDo(l,'tool list');$('recon-tool').replaceChildren(new Option('Choose a test',''));for(const row of rows){const m=/^TOOL (\d+) ble=(\d) ap=(\d) client=(\d) (.+)$/.exec(row);if(m)$('recon-tool').add(new Option(m[5],m[1]));}});
$('recon-ap-list').onclick=()=>reconAction(async l=>{await reconDo(l,'mode set 4');const rows=await reconDo(l,'tool aps');$('recon-ap').replaceChildren(new Option('Choose a fresh network',''));for(const row of rows){const m=/^TOOL_AP (\d+) (\S+) ch=(\d+) rssi=(-?\d+) age=(\d+) security=(\d+) name=(.*)$/.exec(row);if(m&&Number(m[5])<30)$('recon-ap').add(new Option(`${m[7]||'Hidden SSID'} · ${m[2]} · CH ${m[3]} · ${m[4]} dBm`,m[2]+' '+m[3]));}});
$('recon-ap-select').onclick=()=>reconAction(async l=>{if($('recon-ap').value==='')throw new Error('Choose a network first.');await reconDo(l,'tool ap '+$('recon-ap').value);$('recon-client').replaceChildren(new Option('AP scope / no individual client',''));});
$('recon-client-list').onclick=()=>reconAction(async l=>{const rows=await reconDo(l,'tool clients');$('recon-client').replaceChildren(new Option('AP scope / no individual client',''));for(const row of rows){const m=/^TOOL_CLIENT (\d+) (\S+) rssi=(-?\d+) age=(\d+)$/.exec(row);if(m&&Number(m[4])<30)$('recon-client').add(new Option(`${m[2]} · ${m[3]} dBm`,m[2]));}});
$('recon-client-select').onclick=()=>reconAction(l=>reconDo(l,$('recon-client').value===''?'tool client clear':'tool client '+$('recon-client').value));
$('recon-start').onclick=()=>reconAction(async l=>{const id=$('recon-tool').value;if(id==='')throw new Error('Load and choose a test first.');const target=await reconLines(l,'tool target'),label=$('recon-tool').selectedOptions[0].textContent;if(!confirm(`Start ${label}?\n${target.filter(s=>s.startsWith('RECON_TARGET')).join('\n')}\n${$('recon-seconds').value} seconds at ${$('recon-rate').value} requested frames/sec. Radar pauses until Stop or timeout.`))return;for(const [name,max] of [['seconds',60],['rate',100]]){const n=Number($('recon-'+name).value);if(!Number.isInteger(n)||n<1||n>max)throw new Error('Invalid duration or rate.');await reconDo(l,`tool ${name} ${n}`);}await reconDo(l,'tool start '+Number(id));});
$('recon-ssid-add').onclick=()=>reconAction(l=>{const name=$('recon-ssid').value;if(!/^[ -~]{1,32}$/.test(name))throw new Error('SSID must be 1–32 printable ASCII characters.');return reconDo(l,'tool ssid add '+name);});
$('recon-discard').onclick=()=>{if(confirm('Discard the unsaved capture in RAM? Saved captures stay intact.'))reconAction(l=>reconDo(l,'recon capture discard'));};
$('recon-files').onclick=()=>reconAction(async l=>{const rows=await reconDo(l,'recon files');$('recon-file').replaceChildren(new Option('Choose a saved capture',''));for(const row of rows){const m=/^RECON_SAVED (\S+) (\d+)$/.exec(row);if(m){try{captureName(m[1]);$('recon-file').add(new Option(`${m[1]} · ${m[2]} bytes`,m[1]));}catch{}}}});
$('recon-download').onclick=()=>reconAction(async l=>{const name=captureName($('recon-file').value),data=await exportCapture(l,name,p=>{$('recon-output').textContent=`Downloading ${p}%`;});const url=URL.createObjectURL(new Blob([data])),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);$('recon-output').textContent='Capture downloaded; length and CRC verified.';});
$('recon-remove').onclick=()=>{const name=$('recon-file').value;if(name&&confirm(`Delete saved capture ${name}?`))reconAction(l=>reconDo(l,'recon remove '+captureName(name)));};

$('recon-target-status').onclick=()=>reconAction(l=>reconDo(l,'tool target'));
$('recon-target-clear').onclick=()=>reconAction(async l=>{await reconDo(l,'tool target clear');await reconDo(l,'tool target');});
$('recon-local-save').onclick=()=>reconAction(async l=>{const name=$('recon-local-name').value,ch=Number($('recon-local-channel').value);if(!/^[ -~]{1,32}$/.test(name)||!Number.isInteger(ch)||ch<1||ch>11)throw new Error('Use a printable SSID and channel 1–11.');await reconDo(l,'tool target clear');await reconDo(l,'tool name '+name);await reconDo(l,'tool channel '+ch);await reconDo(l,'tool target');});
$('recon-destination-save').onclick=()=>reconAction(l=>{const ch=Number($('recon-destination').value);if(!Number.isInteger(ch)||ch<1||ch>11)throw new Error('Use channel 1–11.');return reconDo(l,'tool destination '+ch);});
$('recon-ssid-list').onclick=()=>reconAction(l=>reconDo(l,'tool ssid list'));
$('recon-ssid-clear').onclick=()=>{if(confirm('Clear the saved SSID playlist?'))reconAction(l=>reconDo(l,'tool ssid clear'));};

function downloadText(text,name,type='text/plain'){const url=URL.createObjectURL(new Blob([text],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);}
for(const [id,command] of Object.entries({'lab-subscriptions':'lab subscriptions','lab-hid':'lab hid','lab-fingerprints':'lab fingerprints','lab-security':'lab security','lab-bonds':'lab bonds'}))$(id).onclick=()=>labAction(async l=>{$('lab-output').textContent=(await labLines(l,command)).join('\n');});
$('lab-unsubscribe').onclick=()=>labAction(l=>labCommand(l,'lab unsubscribe '+labAttr()));
$('lab-write').onclick=()=>labAction(async l=>{const index=labAttr(),value=normalizeHex($('lab-write-value').value),nr=$('lab-write-nr').checked;if(!confirm(`Write ${value.length/2} bytes to the connected peripheral's characteristic #${index}${nr?' without response':''}?\n${value}`))return;await labCommand(l,`lab ${nr?'writenr':'write'} ${index} ${value}`);});
$('lab-mtu-save').onclick=()=>labAction(l=>{const n=Number($('lab-mtu').value);if(!Number.isInteger(n)||n<23||n>517)throw new Error('MTU must be 23–517.');return labCommand(l,'lab mtu '+n);});
$('lab-delete').onclick=()=>labAction(async l=>{const n=labSaved();if(confirm(`Delete saved profile ${n}?`))await labCommand(l,'lab delete '+n);});
$('lab-pairing-save').onclick=()=>labAction(l=>labCommand(l,`lab pairing ${$('lab-pairing').checked?1:0} ${$('lab-mitm').checked?1:0} ${$('lab-bond').checked?1:0}`));
$('lab-confirm-yes').onclick=()=>labAction(l=>labCommand(l,'lab confirm yes'));
$('lab-confirm-no').onclick=()=>labAction(l=>labCommand(l,'lab confirm no'));
$('lab-passkey-save').onclick=()=>labAction(l=>{const code=$('lab-passkey').value;if(!/^\d{6}$/.test(code))throw new Error('Enter the six digits displayed by your peripheral.');$('lab-passkey').value='';return labCommand(l,'lab passkey '+code);});
$('lab-forget').onclick=()=>labAction(async l=>{const address=$('lab-bond-address').value.trim();if(!/^[0-9a-f]{2}(?::[0-9a-f]{2}){5} [0-3]$/i.test(address))throw new Error('Use address and type from List bonds.');if(confirm('Remove this one stored BLE bond?'))await labCommand(l,'lab forget '+address);});
$('recon-stats').onclick=()=>reconAction(l=>reconDo(l,'recon stats'));
$('recon-probes').onclick=()=>reconAction(async l=>{const rows=await reconLines(l,'recon probes');$('recon-output').textContent=probeRows(rows).map(p=>`${p.ssid||'[wildcard]'} · ${p.address} · CH ${p.channel} · ${p.rssi} dBm · ${p.age}s ago · ${p.count} observations`).join('\n')||'No probe requests observed yet.';});
$('recon-probes-download').onclick=()=>reconAction(async l=>{downloadText(probeCsv(await reconLines(l,'recon probes')),'dragon-probes.csv','text/csv');$('recon-output').textContent='Probe inventory downloaded locally.';});
$('recon-filter-save').onclick=()=>reconAction(l=>reconDo(l,`recon filter ${Number($('recon-filter').value)} ${$('recon-filter-ap').checked?1:0}`));
$('recon-filter-status').onclick=()=>reconAction(l=>reconDo(l,'recon filter'));
$('recon-hop-save').onclick=()=>reconAction(l=>reconDo(l,'recon hop '+$('recon-hop').value));

$('lab-select').onclick=()=>labAction(l=>{const d=$('lab-device').value;if(!/^[0-9a-f:]{17} [0-3]$/i.test(d))throw new Error('Choose your observed device.');return labCommand(l,'lab select '+d);});
$('lab-replay').onclick=()=>labAction(async l=>{if(confirm('Replay the selected owned peripheral’s legacy advertisement for up to 30 seconds? Radar pauses until Stop. Captured identity replay needs a random-static address.'))await labCommand(l,'lab replay'+($('lab-replay-identity').checked?' identity':''));});
$('lab-sound').onclick=()=>labAction(async l=>{if(confirm('Send one sound request to the connected Find My / DULT tracker? The tracker decides whether to allow it.'))await labCommand(l,'lab sound');});
for(const [id,command] of Object.entries({'portal-status':'portal status','portal-stop':'portal stop','portal-log':'portal log','portal-save':'portal save'}))$(id).onclick=()=>reconAction(l=>reconDo(l,command));
$('portal-name-save').onclick=()=>reconAction(l=>{const n=$('portal-name').value;if(!/^[ -~]{1,32}$/.test(n))throw new Error('Use a printable 1–32 byte SSID.');return reconDo(l,'portal name '+n);});
$('portal-start').onclick=()=>reconAction(async l=>{const rows=await reconLines(l,'portal status');if(!confirm('Start this local lab AP?\n'+rows.join('\n')+'\nRadar pauses until Stop or timeout.'))return;const n=Number($('recon-seconds').value);if(!Number.isInteger(n)||n<1||n>60)throw new Error('Duration must be 1–60 seconds.');await reconDo(l,'tool seconds '+n);await reconDo(l,'portal start');});
$('portal-probes').onclick=()=>reconAction(async l=>{const rows=await reconDo(l,'portal probes');$('portal-probe').replaceChildren(new Option('Choose a probe',''));for(const row of rows){const m=/^RECON_PORTAL_PROBE ([0-9a-f:]{17}) ([0-9a-f]{2,64}) (.*)$/i.exec(row);if(m)$('portal-probe').add(new Option(m[3],m[1]+' '+m[2]));}});
$('portal-probe-select').onclick=()=>reconAction(l=>{const value=$('portal-probe').value;if(!/^[0-9a-f:]{17} [0-9a-f]{2,64}$/i.test(value))throw new Error('Choose a recent probe first.');return reconDo(l,'portal probe '+value);});
$('portal-clear').onclick=()=>{if(confirm('Clear the in-memory portal submissions?'))reconAction(l=>reconDo(l,'portal clear'));};
$('portal-html-upload').onclick=()=>reconAction(async l=>{const file=$('portal-html').files[0];if(!file||file.size>16384)throw new Error('Choose an HTML template up to 16 KB.');await importPortalHtml(l,new Uint8Array(await file.arrayBuffer()),p=>{$('recon-output').textContent=`Uploading template ${p}%`;});$('recon-output').textContent='Template stored with a verified CRC.';});
$('portal-html-reset').onclick=()=>{if(confirm('Restore the original Dragon Detector test page?'))reconAction(l=>reconDo(l,'portal html reset'));};

for(const action of ['start','status','calibrate','stop'])$('csi-'+action).onclick=()=>reconAction(l=>reconDo(l,'csi '+action));
$('csi-download').onclick=()=>reconAction(async l=>{const rows=await reconDo(l,'csi history');const header=rows.find(x=>x.startsWith('RECON_CSI_CSV '));if(!header)throw new Error('No CSI history response');const csv=[header.slice(14),...rows.filter(x=>x.startsWith('RECON_CSI_POINT ')).map(x=>x.slice(16))].join('\n')+'\n';downloadText(csv,'dragon-csi.csv','text/csv');});

for(const [id,command] of Object.entries({'network-status':'net status','network-profiles':'net profiles','network-disconnect':'net disconnect','network-ping':'net ping gateway','network-stop':'net scan stop','network-results':'net results','network-save':'net save'}))$(id).onclick=()=>reconAction(l=>reconDo(l,command));
$('network-store').onclick=()=>reconAction(async l=>{const command=networkProfileCommand(Number($('network-slot').value),$('network-ssid').value,$('network-password').value);try{await reconDo(l,command);}finally{$('network-password').value='';}});
$('network-connect').onclick=()=>reconAction(async l=>{await reconDo(l,'net connect '+Number($('network-slot').value));await l.write('screen network');});
$('network-remove').onclick=()=>{if(confirm('Delete this saved network and password from the detector?'))reconAction(l=>reconDo(l,'net profile remove '+Number($('network-slot').value)));};
$('network-scan').onclick=()=>{if(confirm('Ping up to 254 addresses in the joined local subnet?'))reconAction(l=>reconDo(l,'net scan local'));};
$('network-clear').onclick=()=>{if(confirm('Clear current ping results? Saved CSV files stay intact.'))reconAction(l=>reconDo(l,'net clear'));};
$('network-download').onclick=()=>reconAction(async l=>{downloadText(networkResultsCsv(await reconLines(l,'net results')),'dragon-local-network.csv','text/csv');$('recon-output').textContent='Local network results downloaded.';});

async function notebookAction(action){await detectorAction(action);$('notebook-status').textContent=$('detector-status').textContent;}
$('notebook-open').onclick=()=>notebookAction(l=>l.write('screen notebook'));
for(const kind of ['probes','adverts'])$('notebook-'+kind).onclick=()=>notebookAction(async l=>{
 $('detector-status').textContent='Reading passive '+kind+' snapshot…';
 const csv=await readNotebook(l,kind);const url=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download='dragon-notebook-'+kind+'.csv';a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);
 $('detector-status').textContent='Snapshot downloaded. Radar scanning continued during export.';
});
$('notebook-memory').onclick=()=>notebookAction(async l=>{await l.write('memory status');$('detector-status').textContent=await l.wait(s=>s.startsWith('MEMORY '));});
$('custom-advert-save').onclick=async()=>{await reconAction(l=>reconDo(l,customAdvertCommand($('custom-advert-hex').value)));$('custom-advert-status').textContent=$('recon-output').textContent;};
