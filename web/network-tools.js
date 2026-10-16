export function hasNetworkTools(version){return /^0\.(?:11\.[1-9]\d*|12\.\d+)$/.test(version);}
export function networkProfileCommand(slot,ssid,password){
 if(!Number.isInteger(slot)||slot<0||slot>7)throw new Error('Choose profile slot 1–8.');
 if(!/^[ -~]{1,32}$/.test(ssid)||!(/^[ -~]{8,63}$/.test(password)||password===''))throw new Error('Use a 1–32 character SSID and an 8–63 character password, or leave it empty for an open network.');
 const hex=s=>Array.from(new TextEncoder().encode(s),b=>b.toString(16).padStart(2,'0')).join('');
 return `net profile put ${slot} ${hex(ssid)} ${password?hex(password):'-'}`;
}
export function networkResultsCsv(lines){return 'ipv4,rtt_ms\n'+lines.flatMap(line=>{const m=/^RECON_HOST ((?:\d{1,3}\.){3}\d{1,3}) (\d{1,5})$/.exec(line);return m&&m[1].split('.').every(x=>Number(x)<=255)&&Number(m[2])<=65535?[`${m[1]},${m[2]}\n`]:[];}).join('');}
