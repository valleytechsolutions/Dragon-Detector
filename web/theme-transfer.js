import {crc32} from './badge-codec.js?v=2026-10-25-locksport-01338';
import {decodeTheme,THEME_SIZE} from './theme-codec.js';
import {deviceQuery} from './device-query.js?v=0.13.17';
export function hasPersonalThemes(version){const m=/^0\.13\.(0|[1-9]\d*)$/.exec(version);return !!m&&Number(m[1])>=17;}
export async function themeInfo(link){
  const line=await deviceQuery(link,'theme info',s=>s.startsWith('THEME_INFO '));
  const m=/^THEME_INFO format=1 saved=([01]) bytes=(\d+) crc=([a-f\d]{8}) storage=([01])$/.exec(line);
  if(!m||Number(m[2])!==(m[1]==='1'?THEME_SIZE:0))throw new Error('Invalid personal-theme response.');
  return {saved:m[1]==='1',bytes:Number(m[2]),crc:parseInt(m[3],16),storage:m[4]==='1'};
}
export async function uploadTheme(link,bytes,onProgress=()=>{}){
  decodeTheme(bytes);link.lines=[];
  await link.write(`theme upload ${bytes.length} ${crc32(bytes).toString(16)}`);
  const wait=predicate=>link.wait(s=>predicate(s)||s.startsWith('THEME_ERROR'));
  let reply=await wait(s=>s==='THEME_READY');if(reply!=='THEME_READY')throw new Error(reply);
  for(let at=0;at<bytes.length;at+=127){const end=Math.min(at+127,bytes.length);await link.write(bytes.subarray(at,end));reply=await wait(s=>s===`THEME_ACK ${end}`);if(reply.startsWith('THEME_ERROR'))throw new Error(reply);onProgress(Math.floor(end*95/bytes.length));}
  reply=await wait(s=>s==='THEME_OK');if(reply!=='THEME_OK')throw new Error(reply);
  const saved=await themeInfo(link);if(!saved.saved||saved.crc!==crc32(bytes.subarray(0,-4)))throw new Error('The saved theme did not match. Reconnect and read it before retrying.');
  onProgress(100);return saved;
}
export async function readTheme(link,onProgress=()=>{}){
  const info=await themeInfo(link);if(!info.saved)throw new Error('No personal theme is saved on this device yet.');
  const bytes=new Uint8Array(THEME_SIZE);
  for(let offset=0;offset<bytes.length;offset+=512){
    const count=Math.min(512,bytes.length-offset);const line=await deviceQuery(link,`theme read ${offset} ${count}`,s=>s.startsWith(`THEME_DATA ${offset} `)||s.startsWith('THEME_ERROR'));
    const hex=line.slice(`THEME_DATA ${offset} `.length);if(!line.startsWith(`THEME_DATA ${offset} `)||hex.length!==count*2||!/^[a-f\d]+$/.test(hex))throw new Error('The theme could not be read completely.');
    for(let i=0;i<count;i++)bytes[offset+i]=parseInt(hex.slice(i*2,i*2+2),16);onProgress(Math.floor((offset+count)*100/bytes.length));
  }
  if(crc32(bytes.subarray(0,-4))!==info.crc)throw new Error('Theme readback checksum failed.');decodeTheme(bytes);return bytes;
}
