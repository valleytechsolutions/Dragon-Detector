export const hasNotebook=version=>/^0\.(?:12|13)\.\d+$/.test(version);
export function customAdvertCommand(text){
 const hex=String(text).replace(/[ :\-]/g,'');
 if(!/^(?:[a-f0-9]{2}){1,31}$/i.test(hex))throw new Error('Enter 1–31 bytes of advertisement data in hex.');
 const bytes=Uint8Array.from(hex.match(/../g),b=>parseInt(b,16));
 for(let i=0;i<bytes.length;){const length=bytes[i++];if(!length){if(i!==bytes.length)throw new Error('Zero-length AD field must end the payload.');break;}if(length>bytes.length-i)throw new Error('An AD field extends past the payload.');i+=length;}
 return 'tool payload '+hex.toLowerCase();
}
export function readNotebook(link,kind){
 if(!['probes','adverts'].includes(kind))return Promise.reject(new Error('Unknown notebook table.'));
 const header=kind==='probes'?'mac,channel,rssi,seen_ms,count,ssid_hex':'mac,address_type,rssi,seen_ms,count,changes,adv_length,company,appearance,tx_power,service,protocol,payload_hex';
 return new Promise((resolve,reject)=>{let collecting=false,rows=[],settled=false,off=()=>{};
  const done=(error)=>{if(settled)return;settled=true;clearTimeout(timer);off();if(error)reject(error);else resolve(rows.join('\n')+'\n');};
  const timer=setTimeout(()=>done(new Error('Notebook download timed out.')),30000);
  off=link.onLine(line=>{if(line.startsWith('NOTEBOOK_ERROR'))return done(new Error(line));if(line==='NOTEBOOK_BEGIN'){collecting=true;return;}if(line==='NOTEBOOK_END'){if(!collecting||rows[0]!==header)return done(new Error('Invalid notebook CSV header.'));return done();}if(!collecting)return;
   if(!rows.length){if(line!==header)return done(new Error('Invalid notebook CSV header.'));}
   else {const fields=line.split(',');if(rows.length>96||line.length>1600||fields.length!==header.split(',').length||!/^[a-f0-9]{2}(?::[a-f0-9]{2}){5}$/i.test(fields[0])||!/^[a-f0-9]*$/i.test(fields.at(-1))||fields.some(s=>/^[\s]*[=+@]/.test(s)||(/^\s*-/.test(s)&&!/^-[0-9]+$/.test(s))))return done(new Error('Malformed notebook row.'));}
   rows.push(line);
  });
  Promise.resolve().then(()=>link.write('notebook '+kind)).catch(done);
 });
}
