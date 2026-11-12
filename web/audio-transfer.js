import {deviceQuery} from './device-query.js?v=0.14.7-143c';
import {firmwareAtLeast} from './firmware-version.js?v=0.14.7-143c';

const levels=Object.freeze({silent:0,medium:50,high:100});
const audioBoards=new Set(['waveshare_143c','guition_185_beta','waveshare_185b','watcher','waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear']);
export function hasWebAudio(board,version){return audioBoards.has(board)&&firmwareAtLeast(version,board==='waveshare_143c'?'0.14.7':board==='guition_185_beta'?'0.14.2':board==='waveshare_185b'?'0.14.1':'0.4.0');}
function levelFor(enabled,volume){return !enabled||volume===0?'silent':volume<=50?'medium':'high';}
function validLevel(level){return typeof level==='string'&&Object.hasOwn(levels,level);}

export function parseAudioSettings(line){
  const m=/^TONE_SETTINGS ready=([01]) enabled=([01]) volume=(0|[1-9]\d?|100) hunt=([01]) focus=(-1|0|[1-9]\d?) quiet=([01])((?: [a-z_][a-z0-9_]*=[^\s=]+)*)$/.exec(line);
  if(!m||Number(m[5])>26)throw new Error('Invalid audio settings response.');
  const state={ready:m[1]==='1',enabled:m[2]==='1',volume:Number(m[3]),hunt:m[4]==='1',focus:Number(m[5]),quiet:m[6]==='1'};
  state.level=state.ready?levelFor(state.enabled,state.volume):'silent';
  // Appended status fields are allowed; never let them replace the actual gain
  // and enable fields used to verify a save on both old and current firmware.
  const seen=new Set(['ready','enabled','volume','hunt','focus','quiet']);
  for(const field of m[7].trim().split(' ').filter(Boolean)){
    const [key,value]=field.split('=');
    if(seen.has(key)||(key==='level'&&value!==state.level))throw new Error('Invalid audio settings response.');
    seen.add(key);
  }
  return state;
}

export async function readAudioSettings(link){
  return parseAudioSettings(await deviceQuery(link,'tone settings',s=>s.startsWith('TONE_SETTINGS ')));
}

async function toneReply(link,command){
  await link.write(command);
  const reply=await link.wait(s=>s==='TONE_OK'||s.startsWith('TONE_ERROR'));
  if(reply!=='TONE_OK')throw new Error(reply.replace('TONE_ERROR ','Tone rejected: '));
}

export async function saveAudioSettings(link,{level,hunt,focus}){
  if(!validLevel(level)||typeof hunt!=='boolean'||!Number.isInteger(focus)||focus<-1||focus>26)throw new Error('Invalid audio preferences.');
  const expected={enabled:level!=='silent',volume:levels[level],hunt,focus};
  // The established config command also works on older firmware. Silent writes
  // both disabled and zero gain, including when its old preview bypasses alerts.
  await toneReply(link,`tone config ${Number(expected.enabled)} ${expected.volume} ${Number(hunt)} ${focus}`);
  const matches=state=>Object.entries(expected).every(([key,value])=>state[key]===value);
  let saved=await readAudioSettings(link);
  if(!matches(saved))saved=await readAudioSettings(link);
  if(!matches(saved))throw new Error('Audio settings were not saved. Reconnect to reload saved preferences.');
  return saved;
}

export async function previewAudioTone(link,{level,family,tone}){
  if(!validLevel(level)||level==='silent')throw new Error('Choose Medium or High and save audio settings before previewing.');
  if(!Number.isInteger(family)||family<0||family>26||typeof tone!=='string'||tone.length>512||/[^\x20-\x7e]/.test(tone))throw new Error('Invalid tone preview.');
  const saved=await readAudioSettings(link);
  if(!saved.ready)throw new Error('The board could not initialize its speaker output.');
  if(saved.level==='silent'||saved.level!==level)throw new Error('Save your sound level before previewing. Silent does not play previews.');
  await toneReply(link,`tone preview ${family} ${tone||'-'}`);
  return saved;
}
