import {resourceSetup} from "./sd-resources.js?v=0.14.9";
import {startRadarBackground} from './radar-background.js?v=0.14.9&ui=studio1';
import {completeInstallSession,syncInstalledClock} from './install-clock.js?v=0.14.9';
import {checkRelease,usbErrorText,installDiagnostics} from './install-checks.js?v=0.14.9';
import {createInstallCounter} from './install-counter.js?v=0.14.9';
import {syncVerifiedClock,clockResultText} from './clock-verification.js?v=0.14.9';
import {clockInstallDialogClass} from './clock-install-dialog.js?v=0.14.9';
import {deviceQuery} from './device-query.js';
import {readP4UpdateSlot} from './p4-install.js?v=0.14.9';
import {BadgeSerial} from './serial.js?v=0.14.9';
import {encodeBadge} from './badge-codec.js?v=0.14.9';
import {uploadBadge} from './badge-transfer.js?v=0.14.9';
import {boardProfiles,connectedProfile,supportsBadge} from './board-profiles.js?v=0.14.9';
import {badgeTransform,fitBadgeZoom,panFromPointer,rescalePan,drawBadgeText} from './badge-editor.js?v=0.14.9';
import {revealLocation} from './navigation.js?v=0.14.9&ui=studio2';
import {parseBadgeInfo,hasClockTools} from './firmware-compat.js?v=0.14.9';

import {setupWorkspaceTabs} from './workspace-tabs.js?v=0.14.9';
import {createThemeStudio} from './theme-studio.js?v=0.14.9&ui=studio1';
import {installPersonalTheme} from './theme-install.js?v=0.14.9';

setupWorkspaceTabs();
revealLocation();
const $ = id => document.getElementById(id);
const installCounter=createInstallCounter($('install-count'));
void installCounter.refresh();
startRadarBackground($('radar-background'),$('radar-motion'));
const models = Object.fromEntries(Object.entries(boardProfiles).map(([id,p])=>[id,p.name]));
let serial=null,busy=false;
let board = '', releaseReady = false, request = 0;
let releaseController,releaseVersion='';
let installerReady=false,installerFailed=false,installActive=false;
function installEnabled(){
  const supported=!!navigator.serial && window.isSecureContext;
  $('install-button').disabled = installActive || busy || !!serial || !board || !releaseReady || !$('hardware-confirm').checked || !installerReady || !supported;
  if(!supported)$('install-message').textContent='Use desktop Chrome or Edge over HTTPS or localhost for USB installation.';
  else if(!installerReady)$('install-message').textContent=installerFailed?'The USB installer could not load. Check your connection and reload this page. See connection help below.':'Loading the USB installer…';
  else if(serial)$('install-message').textContent='Disconnect Badge Studio before opening the installer.';
  else if(installActive)$('install-message').textContent='USB is in use by the installer or clock setup.';
  else if(!board)$('install-message').textContent='Select a board to load its release.';
  else if(releaseReady)$('install-message').textContent='Confirm your board, then connect its USB port.';
}
const installerTimer=setTimeout(()=>{installerFailed=true;installEnabled();},10000);
customElements.whenDefined('ewt-install-dialog').then(()=>{
  customElements.define('dragon-clock-install-dialog',clockInstallDialogClass(customElements.get('ewt-install-dialog')));
  clearTimeout(installerTimer);installerReady=true;installEnabled();
}).catch(error=>{clearTimeout(installerTimer);installerFailed=true;installEnabled();$('install-clock-status').textContent=error.message;});
installEnabled();
async function chooseBoard(next) {
  if(installActive)return;
  releaseController?.abort();releaseController=new AbortController();
  board = next; releaseReady = false; releaseVersion='';const revision = ++request;
  $('retry-release').hidden=true;
  if(!serial)setBadgeProfile(next);
  $('hardware-confirm').checked = false; $('download').hidden = true; installEnabled();
  document.querySelectorAll('[data-board]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.board===board)));
  $('selected-board').textContent = models[board];
  $('board-storage').textContent = boardProfiles[board].storage;
  const details = $('board-features');
  details.textContent = boardProfiles[board].features;
  if (boardProfiles[board].guide) {
    const guide = document.createElement('a');
    guide.href = boardProfiles[board].guide;
    guide.textContent = 'P4 beta capabilities and controls';
    details.append(' ', guide);
  }
  $('board-watcher-setup').hidden=board!=='watcher';
  $('board-143c-setup').hidden=board!=='waveshare_143c';
  $('board-185b-setup').hidden=board!=='waveshare_185b';
  $('board-guition-beta-setup').hidden=board!=='guition_185_beta';
  $('board-gps-guide').hidden=board!=='waveshare_175_gps';
  $('board-sd-setup').hidden = !['waveshare_175','waveshare_175_gps'].includes(board);
  $('board-dial-setup').hidden = board!=='m5dial';
  $('board-rotary-setup').hidden = board!=='elecrow_21';
  $('board-resource-setup').hidden=true;
  $('board-lilygo-setup').hidden = board!=='lilygo_175';
  $('auto-clock-label').hidden=false;
  controls();
  $('clock-setup-status').textContent='Ready to sync '+models[board]+'.';
  $('install-clock-status').textContent='';
  $('board-status').textContent = 'Checking the matching firmware release…';
  const mode = $('install-mode').value, manifest = `firmware/${board}/${mode}.json`;
  try {
    const data = await checkRelease(board,mode,{signal:releaseController.signal});
    if(revision !== request)return;
    $('installer').setAttribute('manifest', manifest); releaseReady = true;releaseVersion=data.version;
    const pack=resourceSetup(board,data);
    $('board-resource-setup').hidden=!pack;
    if(pack){$('resource-title').textContent=pack.title;$('resource-instructions').textContent=pack.instructions;$('resource-download').href=pack.path;$('resource-download').textContent=board==='waveshare_p4_knob'?'Download P4 microSD starter ZIP':'Download SD resource pack v1';}
    $('board-status').textContent = `${data.version}${board==='waveshare_143c'?' · Initial port — physical validation pending':board==='guition_185_beta'?' · DISPLAY BETA 1 — physical compatibility unverified':board==='watcher'?' · BETA — SD capture/resources and RTC tested; touch and rotary confirmed':board==='waveshare_185b'?' · BETA — display/touch, SD captures and RTC restart verified; external GPS field testing pending':data.sdResources?' · SD RESOURCE BETA — hardware validation pending':board==='m5dial'?' · Dial v1.1 verified':board==='elecrow_21'?' · EXPERIMENTAL — initial hardware checks passed':board==='lilygo_175'?' · BETA — working baseline reported; new RTC/SD checks await hardware testing':boardProfiles[board].experimental?' · BETA — new SD/GPS features await field testing':''} · ${mode==='factory'?'Fresh installation replaces saved history, badges and BLE profiles.':board==='waveshare_143c'?'Update requires this exact 8 MB AMOLED 1.43C profile, version 0.14.7 or newer. Use Fresh installation for factory firmware or any other profile.':board==='waveshare_185b'?'Update requires this exact LCD 1.85B profile, version 0.14.1 or newer. Use Fresh installation for the factory demo or another firmware.':board==='waveshare'?'32 MB update preserves badge, history, settings and BLE profiles.':['waveshare_175','waveshare_175_gps'].includes(board)?'Update requires the exact selected non-C profile (1.75: 0.13.19+; 1.75-G GPS: 0.13.21+). Use Fresh installation for the factory demo or another firmware.':board==='elecrow_21'?'Update requires this exact Elecrow 2.1 rotary profile, version 0.13.20 or newer. Use Fresh installation for the factory demo or another firmware.':board==='lilygo_175'?'Update matches the provided 0.13.20 LILYGO 1.75 layout and this profile’s 0.13.25+ builds. Other firmware or another LILYGO size requires Fresh installation with its correct profile.':board==='watcher'?'Update requires this exact Watcher 0.13.29+ partition layout. Choose Fresh installation for Seeed factory firmware.':board==='m5dial'?'Update requires this Dial profile’s 0.13.26 or newer partition layout. Use Fresh installation for the factory demo or any other firmware.':board==='waveshare_lite'?'Update requires this exact Lite profile’s 0.9.4+ layout. For an older third-party port, choose Fresh installation unless its partitions have been verified.':board==='xiao_round'?'Update preserves files; requires this XIAO + Round Display profile’s 0.9.2 or newer 8 MB layout.':'Update preserves history, badges and settings; requires this board’s existing 0.3.x–0.13.x layout.'}`;
    if(board==='waveshare_p4_knob'){
      const installation=mode==='factory'?'Fresh installation erases internal flash. Verify that your board is the ESP32-P4 revision 3.x model and back up saved files first.':'Update verifies the active slot on an existing P4 Dragon Detector install and preserves files and settings.';
      $('board-status').textContent=data.version+' · Hardware beta. Wi-Fi, Bluetooth LE and alternating reception were tested on one board. Field testing is in progress. '+installation;
    }
    $('download').href = `firmware/${board}/SHA256SUMS.txt`; $('download').textContent='Firmware file checksums'; $('download').hidden=false;
    $('install-message').textContent='Confirm your board, then connect its USB port.';
  } catch(error) {
    if(revision!==request)return;
    $('board-status').textContent=error.message;
    $('retry-release').hidden=false;
    $('install-message').textContent='Installation stays disabled until matching build files are available.';
  }
  installEnabled();
}
document.querySelectorAll('[data-board]').forEach(b=>b.onclick=()=>chooseBoard(b.dataset.board));
$('hardware-confirm').onchange=installEnabled;
$('install-mode').onchange=()=>{if(board)chooseBoard(board);};
$('retry-release').onclick=()=>{if(board)chooseBoard(board);};
$('copy-install-details').onclick=async()=>{
  const report=installDiagnostics({board:models[board],mode:$('install-mode').value,version:releaseVersion,
    installerReady,serialSupported:!!navigator.serial&&window.isSecureContext,userAgent:navigator.userAgent,
    status:[$('board-status').textContent,$('install-message').textContent,$('install-clock-status').textContent,$('clock-setup-status').textContent].join('\n')});
  const output=$('install-details');output.hidden=false;output.value=report;
  try{await navigator.clipboard.writeText(report);$('copy-details-status').textContent='Copied. Add the exact board model and installer error when reporting a problem.';}
  catch{output.focus();output.select();$('copy-details-status').textContent='Select and copy the details below.';}
};

const canvas=$('preview'), ctx=canvas.getContext('2d',{willReadFrequently:true});
const guideCanvas=$('preview-guides'),guideCtx=guideCanvas.getContext('2d');
let photo=null,size=466,zoom=1,offsetX=0,offsetY=0,turns=0,mirror=false,firmwareVersion='',deviceProfile='',badgeProfile='stopwatch',photoName='',imageRequest=0;

const themeStudio=createThemeStudio({
  state:()=>({serial,busy,version:firmwareVersion,board:models[deviceProfile]||'Device',profile:deviceProfile}),
  connect:()=>connectDevice(),
  disconnect:()=>disconnect(),
  run:async action=>{
    if(!serial||busy)return;
    busy=true;draw();installEnabled();
    try{await action(serial);}
    finally{busy=false;draw();installEnabled();}
  }
});

function installLock(active){
  installActive=active;busy=active;draw();installEnabled();
  for(const element of document.querySelectorAll('[data-board],#install-mode,#hardware-confirm,#auto-clock,#retry-release'))element.disabled=active;
}
$('install-button').onclick=async()=>{
  if($('install-button').disabled||busy||serial)return;
  let customTheme;
  try{
    customTheme=themeStudio.installation();
    if(customTheme&&board==='m5dial')throw new Error('Dial supports built-in themes only. Turn off “Install this personal theme” to continue.');
  }catch(error){location.hash='theme-studio';$('theme-status').textContent=error.message;return;}
  const selected=board,autoClock=$('auto-clock').checked;
  installLock(true);
  let port;
  try{
    port=await navigator.serial.requestPort();
    let manifestPath=$('installer').getAttribute('manifest');
    if(selected==='waveshare_p4_knob'&&$('install-mode').value==='update'){
      const appSlot=await readP4UpdateSlot(port);
      await checkRelease(selected,'update',{appSlot});
      if(appSlot===1)manifestPath='firmware/waveshare_p4_knob/update-app1.json';
    }
    await port.open({baudRate:115200,bufferSize:8192});
    const dialog=document.createElement('dragon-clock-install-dialog');
    dialog.onFirmwareInstalled=()=>{void installCounter.recordInstall();};
    dialog.installMode=$('install-mode').value;dialog.autoClock=autoClock;dialog.autoTheme=!!customTheme;
    dialog.port=port;dialog.manifestPath=manifestPath;
    dialog.addEventListener('closed',async()=>{
      const succeeded=dialog.installSucceeded;
      $('install-clock-status').textContent=succeeded&&autoClock?'Step 2 of 2: setting date, time and timezone; verifying the clock…':'Closing installer USB connection…';
      try{
        const result=await completeInstallSession(port,selected,{succeeded,autoClock});
        if(result.cancelled){
          $('install-clock-status').textContent=dialog.installError?`Installation did not complete: ${dialog.installError} See connection help below, then retry.`:'Installer closed without completing an installation. Clock setup was skipped.';
          return;
        }
        $('install-clock-status').textContent=autoClock
          ? (result.reason || clockResultText(result))
          : 'Firmware installed. Automatic clock setup was turned off.';
        if(customTheme){
          const clockStatus=$('install-clock-status').textContent;
          try{
            await installPersonalTheme(port,selected,customTheme,percent=>{$('install-clock-status').textContent='Installing personal theme · '+percent+'%';});
            $('install-clock-status').textContent=clockStatus+' Personal theme saved and verified.';
            themeStudio.installed();
          }catch(error){$('install-clock-status').textContent=clockStatus+' Personal theme setup failed: '+error.message+' Open Theme creator to retry.';}
        }
      }catch(error){
        $('install-clock-status').textContent='Firmware installed. Clock verification failed: '+error.message+(selected==='waveshare_p4_knob'?' Reconnect the running P4 knob on its native USB port (reverse the USB-C plug at the board if needed), then use Sync device clock below.':' Reconnect the running board and use Sync device clock below.');
      }
      finally{installLock(false);$('install-button').focus();}
    },{once:true});
    document.body.appendChild(dialog);
    $('install-clock-status').textContent=autoClock?'Step 1 of 2: installing firmware. Keep USB connected for automatic clock setup and verification.':'Close the installer window when finished.';
  }catch(error){
    if(port)try{await port.close();}catch{}
    $('install-clock-status').textContent=usbErrorText(error);
    installLock(false);
  }
};
$('finish-clock').onclick=async()=>{
  if(busy||!board||(serial&&deviceProfile!==board))return;
  const selected=board;
  installLock(true);
  try{
    $('clock-setup-status').textContent='Reading the board and verifying its clock…';
    const result=serial ? await syncVerifiedClock(serial,selected,firmwareVersion)
                        : await syncInstalledClock(await navigator.serial.requestPort(),selected);
    $('clock-setup-status').textContent=result.reason || clockResultText(result);
  }catch(error){
    $('clock-setup-status').textContent=error.name==='NotFoundError'?'Clock sync cancelled.':('Clock sync failed: '+error.message);
  }finally{
    if(serial?.failure)await disconnect();
    installLock(false);
  }
};

function setBadgeProfile(id){
  if(!supportsBadge(id))return;
  const previous=size;badgeProfile=id;size=boardProfiles[id].size;
  ({x:offsetX,y:offsetY}=rescalePan(offsetX,offsetY,previous,size));
  canvas.width=canvas.height=guideCanvas.width=guideCanvas.height=size;
  $('badge-board').value=id;draw();
}
function controls(){
  themeStudio.update();
  const usb=!!navigator.serial && window.isSecureContext;
  $('connect').disabled=busy||!!serial;
  $('disconnect').disabled=busy;
  $('disconnect').hidden=!serial;
  $('finish-clock').disabled=busy||!board||!usb||!!serial&&(board!==deviceProfile||!hasClockTools(deviceProfile,firmwareVersion));
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
    if(stock){zoom=1;offsetX=offsetY=0;draw();$('studio-status').textContent='Dragon Detector logo centered and sized for the round screen. Add text below, or upload as it is.';}
    else {fit();$('studio-status').textContent='Whole image fitted inside the circle. Drag to position, or choose Fill circle for a full-screen crop.';}
  }catch(error){if(revision===imageRequest)$('studio-status').textContent=error.name==='InvalidStateError'?'Could not open that image. Choose a PNG, JPEG or WebP file.':error.message;}
}
$('image').onchange=e=>{const file=e.target.files[0];e.target.value='';loadImage(file);};
async function loadStock(){
  if(busy)return;const revision=++imageRequest;
  try {const response=await fetch('assets/dragon-detector.png');if(!response.ok)throw new Error('The stock badge could not load. Choose your own image or try again.');
    const blob=await response.blob();if(revision!==imageRequest)return;
    await loadImage(new File([blob],'Dragon Detector logo.png',{type:'image/png'}),true);
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

async function disconnect(){
  const link=serial;
  serial=null;firmwareVersion='';deviceProfile='';
  if(link)await link.close();
  $('device-label').textContent='USB NOT CONNECTED';
  draw();installEnabled();
}
async function connectDevice(){
  if(!navigator.serial||!window.isSecureContext){
    $('studio-status').textContent='Use desktop Chrome or Edge over HTTPS for USB. You can still edit and download your preview.';
    return;
  }
  if(busy||serial)return;
  busy=true;draw();installEnabled();
  $('studio-status').textContent='Choose your detector’s USB port.';
  let link;
  try{
    link=new BadgeSerial(await navigator.serial.requestPort());
    await link.open();
    const line=await deviceQuery(link,'badge info',s=>s.startsWith('BADGE_INFO '));
    if(/^BADGE_INFO \S+ \d+ NO_STORAGE /.test(line))throw new Error('The device could not mount its badge storage. Restart it and reconnect. Your preview can still be downloaded.');
    const info=parseBadgeInfo(line),profile=connectedProfile(info);
    if(!profile)throw new Error('Unrecognized board or firmware. Select its exact hardware profile in the installer.');
    serial=link;deviceProfile=profile;firmwareVersion=info.version;
    setBadgeProfile(profile);
    $('device-label').textContent=info.size+' × '+info.size+' · CONNECTED';
    $('studio-status').textContent=boardProfiles[profile].name+(supportsBadge(profile)?' connected. Your badge is ready to edit or upload.':' has no badge mode. Use Clock sync to set the time.');
    link.onLine(line=>{
      if(line===null&&serial===link&&!busy){
        void disconnect();
        $('studio-status').textContent='USB disconnected. Reconnect to upload your badge.';
      }
    });
  }catch(error){
    if(link)await link.close();
    serial=null;deviceProfile='';firmwareVersion='';
    $('studio-status').textContent=error.name==='NotFoundError'?'USB selection cancelled. Your preview is ready to edit.':error.message;
  }finally{
    if(serial?.failure)await disconnect();
    busy=false;draw();installEnabled();
  }
}
$('connect').onclick=connectDevice;
$('disconnect').onclick=async()=>{
  if(busy)return;
  busy=true;
  try{await disconnect();$('studio-status').textContent='USB disconnected. Your badge preview is still available.';}
  finally{busy=false;draw();installEnabled();}
};
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
  }catch(error){$('studio-status').textContent=error.message+' Reconnect to check the saved badge.';await disconnect();}
  finally{busy=false;draw();installEnabled();}
};

draw();loadStock();
