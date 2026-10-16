import {crc32} from './ble-lab.js';
export function hasReconTools(version){return /^0\.(?:10|11)\.\d+$/.test(version);}
export function captureName(value){if(!/^(wifi-[a-f0-9]{8}\.pcap|(?:ble|portal|net)-[a-f0-9]{8}\.csv)$/.test(value))throw new Error('Choose a saved capture.');return value;}
export async function reconLines(link,command){
 link.lines=[];const lines=[];const off=link.onLine(line=>{if(line&&/^(RECON_|TOOL_|MODE_STATUS|MODE |TOOL )/.test(line))lines.push(line);});
 try{await link.write(command);const end=await link.wait(line=>/^RECON_(OK|ERROR)|^MODE_STATUS/.test(line),12000);if(end.startsWith('RECON_ERROR'))throw new Error(end);return lines;}finally{off();}
}
export function exportCapture(link,name,onProgress=()=>{}){
 name=captureName(name);
 return new Promise((resolve,reject)=>{
  let data=null,received=0,settled=false,off=()=>{};
  const finish=(error,result)=>{if(settled)return;settled=true;clearTimeout(timer);off();error?reject(error):resolve(result);};
  const timer=setTimeout(()=>finish(new Error('Capture download timed out.')),180000);
  off=link.onLine(line=>{try{let m;if(line===null||line?.startsWith('RECON_ERROR'))throw new Error(line||'USB disconnected.');
   if((m=/^RECON_FILE (\d+)$/.exec(line))){const n=Number(m[1]);if(data||n<1||n>2*1024*1024)throw new Error('Invalid capture size.');data=new Uint8Array(n);}
   else if((m=/^RECON_DATA (\d+) ([a-f0-9]+)$/i.exec(line))){if(!data||Number(m[1])!==received||m[2].length%2||m[2].length>256)throw new Error('Capture chunk is missing or malformed.');const b=Uint8Array.from(m[2].match(/../g),s=>parseInt(s,16));if(received+b.length>data.length)throw new Error('Capture exceeds announced size.');data.set(b,received);received+=b.length;onProgress(Math.floor(received*100/data.length));}
   else if((m=/^RECON_END (\d+) ([a-f0-9]{8})$/i.exec(line))){if(!data||received!==data.length||received!==Number(m[1])||crc32(data)!==parseInt(m[2],16))throw new Error('Incomplete capture or checksum mismatch.');finish(null,data);}
  }catch(error){finish(error);}});
  Promise.resolve().then(()=>link.write('recon get '+name)).catch(error=>finish(error));
 });
}

export async function importPortalHtml(link,input,onProgress=()=>{}){const data=input instanceof Uint8Array?input:new Uint8Array(input);if(!data.length||data.length>16384||data.includes(0))throw new Error('HTML must be 1–16,384 bytes with no NUL bytes.');link.lines=[];await link.write(`portal html import ${data.length} ${crc32(data).toString(16).padStart(8,'0')}`);let reply=await link.wait(s=>s==='RECON_READY'||s.startsWith('RECON_ERROR'));if(reply!=='RECON_READY')throw new Error(reply);let offset=0;while(offset<data.length){const end=Math.min(offset+128,data.length);await link.write(data.slice(offset,end));while(true){reply=await link.wait(s=>s.startsWith('RECON_ACK ')||s.startsWith('RECON_ERROR'));if(reply.startsWith('RECON_ERROR'))throw new Error(reply);const ack=Number(reply.slice(10));if(!Number.isInteger(ack)||ack>end||ack<0)throw new Error('Invalid HTML acknowledgement.');if(ack===end)break;}offset=end;onProgress(Math.round(100*offset/data.length));}reply=await link.wait(s=>s.startsWith('RECON_OK')||s.startsWith('RECON_ERROR'));if(reply.startsWith('RECON_ERROR'))throw new Error(reply);}
