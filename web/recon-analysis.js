export function normalizeHex(value,max=512){
 if(typeof value!=='string'||!value.trim())throw new Error('Enter hex bytes.');
 const compact=value.trim().replace(/(?:[0-9a-f]{2})[ :-]*/gi,m=>m.slice(0,2));
 if(!/^(?:[0-9a-f]{2})+$/i.test(compact)||compact.length>max*2)throw new Error(`Use complete hex bytes, at most ${max}.`);
 return compact.toLowerCase();
}
export function probeRows(lines){return lines.flatMap(line=>{const m=/^RECON_PROBE ([0-9a-f:]{17}) ch=(\d+) rssi=(-?\d+) age=(\d+) count=(\d+) ssid_hex=([a-f0-9]*)$/i.exec(line);if(!m)return [];if(m[6].length>64||m[6].length%2)throw new Error('Malformed probe SSID.');const hex=m[6],ssid=(hex.match(/../g)||[]).map(v=>{const n=parseInt(v,16);return n>=32&&n<=126?String.fromCharCode(n):'\\x'+v;}).join('');return [{address:m[1],channel:Number(m[2]),rssi:Number(m[3]),age:Number(m[4]),count:Number(m[5]),ssid,hex}];});}
export function probeCsv(lines){const quote=v=>{let s=String(v);if(/^[=+@\-\t\r]/.test(s))s="'"+s;return '"'+s.replaceAll('"','""')+'"';};return 'address,channel,rssi,age_seconds,count,requested_ssid,ssid_hex\r\n'+probeRows(lines).map(p=>[p.address,p.channel,p.rssi,p.age,p.count,p.ssid,p.hex].map(quote).join(',')).join('\r\n')+'\r\n';}
