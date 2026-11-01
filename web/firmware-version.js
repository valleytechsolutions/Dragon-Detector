// Accept only release series whose USB and manifest contracts we have tested.
// Newer, unknown series must be reviewed before enabling installation or tools.
export function knownFirmware(version){
  return typeof version==='string'&&/^0\.(?:[3-9]|1[0-4])\.(?:0|[1-9]\d*)$/.test(version);
}
export function firmwareAtLeast(version,minimum){
  if(!knownFirmware(version))return false;
  const current=version.split('.').map(Number),required=minimum.split('.').map(Number);
  for(let i=0;i<3;i++){if(current[i]!==required[i])return current[i]>required[i];}
  return true;
}
