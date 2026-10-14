import {BadgeSerial} from './serial.js';
import {encodeBadge, cropGeometry, fitCircleZoom} from './badge-codec.js';
import {uploadBadge} from './badge-transfer.js';
const $ = id => document.getElementById(id);
const models = {spotpear:'Spotpear / Guition JC3636W518',waveshare:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',stopwatch:'M5Stack StopWatch C152'};
let board = '', releaseReady = false, request = 0;
function installEnabled(){ $('install-button').disabled = !board || !releaseReady || !$('hardware-confirm').checked; }
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
    $('board-status').textContent = `${data.version} · ${mode==='factory'?'Fresh installation replaces saved badge files.':'Update preserves badges and settings; requires this board’s existing 0.3.x layout.'}`;
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
let photo=null, size=466, zoom=1, offsetX=0, offsetY=0, serial=null, busy=false;
function draw(){
  ctx.fillStyle=$('background').value;ctx.fillRect(0,0,size,size);
  if(photo){const g=cropGeometry(photo.width,photo.height,size,zoom,offsetX,offsetY);ctx.drawImage(photo,g.x,g.y,g.width,g.height);}
  else {ctx.fillStyle='#163b25';ctx.beginPath();ctx.arc(size/2,size/2,size*.39,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#78a05b';ctx.lineWidth=1;for(let n=1;n<4;n++){ctx.beginPath();ctx.arc(size/2,size/2,size*.1*n,0,Math.PI*2);ctx.stroke();}ctx.fillStyle='#c3f66d';ctx.textAlign='center';ctx.font=`bold ${size*.062}px sans-serif`;ctx.fillText('YOUR BADGE',size/2,size*.49);ctx.font=`${size*.027}px sans-serif`;ctx.fillText('CHOOSE AN IMAGE TO BEGIN',size/2,size*.55);}
  $('zoom').value=zoom;$('zoom-value').textContent=Math.round(zoom*100)+'%';
  $('upload').disabled=busy||!serial||!photo;
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

async function disconnect(){const old=serial;serial=null;if(old)await old.close();$('connect').disabled=false;$('disconnect').hidden=true;$('device-label').textContent='USB NOT CONNECTED';draw();}
$('connect').onclick=async()=>{
  if(!navigator.serial){$('studio-status').textContent='Web Serial needs desktop Chrome or Edge on HTTPS or localhost.';return;}
  $('connect').disabled=true;
  let link;
  try {
    link=new BadgeSerial(await navigator.serial.requestPort());await link.open();await link.write('badge info');
    const line=await link.wait(s=>s.startsWith('BADGE_INFO '));
    const match=/^BADGE_INFO (0\.3\.\d+) (360|466) READY (.+)$/.exec(line);
    if(!match)throw new Error('This board needs a 0.3.x factory installation with badge storage.');
    serial=link;const previous=size;size=Number(match[2]);canvas.width=canvas.height=size;offsetX*=size/previous;offsetY*=size/previous;
    $('device-label').textContent=`${size} × ${size} · CONNECTED`;$('disconnect').hidden=false;
    $('studio-status').textContent=`${match[3]} is ready. Choose and position your image.`;draw();
  }catch(error){if(link)await link.close();$('studio-status').textContent=error.message;$('connect').disabled=false;}
};
$('disconnect').onclick=disconnect;
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
draw();
