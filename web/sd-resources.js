import {firmwareAtLeast} from './firmware-version.js?v=0.14.2-guition-beta1';
const sdBoards=new Set(['guition_185_beta','waveshare_185b','watcher','spotpear','waveshare_175','waveshare_175_gps','xiao_round','lilygo_175']);
export function hasSd(board){return sdBoards.has(board);}
export function hasGpsAndSd(board){return ['waveshare_185b','watcher','waveshare_175_gps'].includes(board);}
export const resourcePack=Object.freeze({schema:1,required:false,path:'resources/dragon-resources-v1-20261023.zip',placement:'/dragon/resources/vendors.ddr'});
export const labelResourcePack=Object.freeze({schema:1,required:true,path:'resources/dragon-resources-v1-20261103.zip',placement:'/dragon/resources/vendors.ddr'});
export function resourceSetup(board,manifest){
  const value=manifest?.sdResources;
  if(value===undefined)return null;
  if(!hasSd(board))throw new Error('This board has no SD slot; its manifest cannot require an SD pack.');
  const labelsOnSd=board==='waveshare_185b',expected=labelsOnSd?labelResourcePack:resourcePack;
  if(!value||!firmwareAtLeast(manifest.version,board==='guition_185_beta'?'0.14.2':labelsOnSd?'0.14.1':'0.13.28')||value.schema!==expected.schema||typeof value.required!=='boolean'||value.path!==expected.path||value.placement!==expected.placement||(labelsOnSd&&value.required!==true))
    throw new Error('Unsupported firmware/resource-pack combination.');
  if(labelsOnSd)return {...value,title:'SD pack required for lookup names',instructions:'Extract dragon/ from the ZIP to the FAT32 SD root. Keep both /dragon/resources/vendors.ddr and /dragon/resources/ble-labels.ddb. Preserve existing files, insert the card before boot, and restart after installation or replacement. This pack supplies manufacturer, BLE service and company lookup names. Firmware and core radar detections still run without it; missing or invalid files leave those lookup names unresolved.'};
  return {...value,title:value.required?'SD resource pack REQUIRED':'Optional SD vendor resource pack',instructions:'Extract dragon/ from the ZIP to the FAT32 SD root. The resulting file must be /dragon/resources/vendors.ddr. Preserve existing files and restart. Missing or invalid data falls back to the internal registry.'};
}
