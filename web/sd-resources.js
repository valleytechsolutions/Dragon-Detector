const sdBoards=new Set(['spotpear','waveshare_175','waveshare_175_gps','xiao_round','lilygo_175']);
export function hasSd(board){return sdBoards.has(board);}
export function hasGpsAndSd(board){return board==='waveshare_175_gps';}
export const resourcePack=Object.freeze({schema:1,required:false,path:'resources/dragon-resources-v1-20261023.zip',placement:'/dragon/resources/vendors.ddr'});
export function resourceSetup(board,manifest){
  const value=manifest?.sdResources;
  if(value===undefined)return null;
  if(!hasSd(board))throw new Error('This board has no SD slot; its manifest cannot require an SD pack.');
  if(!/^0\.13\.(?:2[8-9]|[3-9]\d|[1-9]\d{2,})$/.test(manifest.version)||value.schema!==1||typeof value.required!=='boolean'||value.path!==resourcePack.path||value.placement!==resourcePack.placement)
    throw new Error('Unsupported firmware/resource-pack combination.');
  return {...value,title:value.required?'SD resource pack REQUIRED':'Optional SD vendor resource pack',instructions:'Extract dragon/ from the ZIP to the FAT32 SD root. The resulting file must be /dragon/resources/vendors.ddr. Preserve existing files and restart. Missing or invalid data falls back to the internal registry.'};
}
