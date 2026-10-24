export function hasRepeatingBeeper(board,version){const m=/^(\d+)\.(\d+)\.(\d+)$/.exec(version||'');return hasBleTools(board,version)&&!!m&&Number(m[3])>=22;}
export function hasBleTools(board,version){return ['watcher','lilygo_175','waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear','xiao_round'].includes(board)&&/^0\.13\.(?:2\d|[3-9]\d|[1-9]\d{2,})$/.test(version);}
export function observedAddress(value){if(!/^[0-9a-f]{2}(?::[0-9a-f]{2}){5} [0-3]$/i.test(value))throw new Error('Refresh nearby devices and choose your peripheral.');return value;}
export function iBeaconCommand(uuid,major,minor,power){
 uuid=String(uuid).trim().toLowerCase();if(!/^[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}$/.test(uuid))throw new Error('Enter a UUID in 8-4-4-4-12 format.');
 const fields=[major,minor,power],ranges=[[0,65535],[0,65535],[-128,127]];
 for(let i=0;i<3;i++){if(!/^-?\d+$/.test(String(fields[i]))||!Number.isSafeInteger(Number(fields[i]))||Number(fields[i])<ranges[i][0]||Number(fields[i])>ranges[i][1])throw new Error('Major/minor must be 0–65535; measured power must be -128–127 dBm.');}
 return `tool ibeacon ${uuid} ${Number(major)} ${Number(minor)} ${Number(power)}`;
}
export function parseIBeacon(lines){
 const row=lines.find(s=>s.startsWith('RECON_IBEACON ')),m=/^RECON_IBEACON uuid=(\S+) major=(\d+) minor=(\d+) power=(-?\d+)$/.exec(row||'');
 if(!m)throw new Error('No iBeacon configuration returned.');iBeaconCommand(...m.slice(1));return {uuid:m[1],major:m[2],minor:m[3],power:m[4]};
}
