import {firmwareAtLeast} from './firmware-version.js?v=0.14.11';
const sdBoards=new Set(['waveshare_206','waveshare_p4_knob','guition_185_beta','waveshare_185b','watcher','spotpear','waveshare_175','waveshare_175_gps','xiao_round','lilygo_175']);
export function hasSd(board){return sdBoards.has(board);}
export function hasGpsAndSd(board){return ['waveshare_185b','watcher','waveshare_175_gps'].includes(board);}
export const resourcePack=Object.freeze({schema:1,required:false,path:'resources/dragon-resources-v1-20261023.zip',placement:'/dragon/resources/vendors.ddr'});
export const labelResourcePack=Object.freeze({schema:1,required:true,path:'resources/dragon-resources-v1-20261103.zip',placement:'/dragon/resources/vendors.ddr'});
export const p4KnobResourcePack=Object.freeze({schema:1,required:false,path:'resources/p4-knob-sd-starter-0.14.8.zip',placement:'/dragon/resources/vendors.ddr'});
export function resourceSetup(board,manifest){
  const value=manifest?.sdResources;
  if(value===undefined)return null;
  if(!hasSd(board))throw new Error('This board has no SD slot; its manifest cannot require an SD pack.');
  const labelsOnSd=board==='waveshare_185b',p4=board==='waveshare_p4_knob',expected=p4?p4KnobResourcePack:labelsOnSd?labelResourcePack:resourcePack;
  if(!value||!firmwareAtLeast(manifest.version,p4?'0.14.8':board==='guition_185_beta'?'0.14.2':labelsOnSd?'0.14.1':'0.13.28')||value.schema!==expected.schema||typeof value.required!=='boolean'||value.path!==expected.path||value.placement!==expected.placement||(labelsOnSd&&value.required!==true)||(p4&&value.required!==false))
    throw new Error('Unsupported firmware/resource-pack combination.');
  if(p4)return {...value,title:'P4 Knob microSD starter pack',instructions:'Extract the ZIP to the FAT32 card root. Merge folders and keep your files. The pack includes vendor and Bluetooth lookups, /music and /audio for MP3 or WAV playback, and /recordings for Voice Recorder. Insert the card and restart.'};
  if(labelsOnSd)return {...value,title:'SD pack required for lookup names',instructions:'Extract dragon/ from the ZIP to the FAT32 SD root. Keep both /dragon/resources/vendors.ddr and /dragon/resources/ble-labels.ddb. Preserve existing files, insert the card before boot, and restart after installation or replacement. This pack supplies manufacturer, BLE service and company lookup names. Firmware and core radar detections still run without it; missing or invalid files leave those lookup names unresolved.'};
  return {...value,title:value.required?'SD resource pack REQUIRED':'Optional SD vendor resource pack',instructions:'Extract dragon/ from the ZIP to the FAT32 SD root. The resulting file must be /dragon/resources/vendors.ddr. Preserve existing files and restart. Missing or invalid data falls back to the internal registry.'};
}
