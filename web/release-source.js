import {knownFirmware} from './firmware-version.js?v=0.14.11';

const official='https://valleytechsolutions.github.io/Dragon-Detector-Flasher/';

export async function firmwareBase(board,{fetchImpl=fetch,signal}={}){
  if(board==='guition_185_beta'||board==='waveshare_p4_knob')return `firmware/${board}/`;
  const response=await fetchImpl(`${official}catalog.json`,{cache:'no-cache',signal});
  if(!response.ok)throw new Error('The official firmware release catalog is unavailable. Retry shortly.');
  const catalog=await response.json(),version=typeof catalog?.tag==='string'?catalog.tag.slice(1):'';
  const path=`firmware/${catalog?.tag}/${board}`;
  if(!knownFirmware(version)||!Array.isArray(catalog.boards)||!catalog.boards.some(item=>item?.id===board&&item.version===version&&item.path===path))
    throw new Error('The official release catalog has no matching build for this board.');
  return `${official}${path}/`;
}
