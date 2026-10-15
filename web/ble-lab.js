// Original MIT DDG1 profile reader and acknowledged USB transport.
export const HEADER=580,ATTRIBUTE=559,MAX_PROFILE=HEADER+128*ATTRIBUTE;
export function crc32(data){let c=0xffffffff;for(const b of data){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;}
const decoder=new TextDecoder();
function string(data,offset,n){const b=data.slice(offset,offset+n),end=b.indexOf(0);if(end<0)throw new Error('Unterminated profile string.');return decoder.decode(b.slice(0,end));}
function adValid(data){for(let i=0;i<data.length;){const n=data[i++];if(!n)return data.slice(i).every(b=>b===0);if(i+n>data.length)return false;i+=n;}return true;}
export function decodeProfile(input,{withCrc=true}={}){
 const all=input instanceof Uint8Array?input:new Uint8Array(input),data=withCrc?all.slice(0,-4):all;
 if(data.length<HEADER||data.length>MAX_PROFILE)throw new Error('Invalid profile length.');
 const v=new DataView(data.buffer,data.byteOffset,data.byteLength),count=v.getUint16(6,true),advLength=v.getUint16(64,true),boundary=v.getUint16(66,true);
 if(decoder.decode(data.slice(0,4))!=='DDG1'||v.getUint16(4,true)!==1||count>128||data.length!==HEADER+ATTRIBUTE*count||data[62]>3||data[63]>1||advLength>512||boundary>advLength)throw new Error('Unsupported or malformed DDG1 profile.');
 if(withCrc&&(all.length!==data.length+4||new DataView(all.buffer,all.byteOffset).getUint32(data.length,true)!==crc32(data)))throw new Error('Profile checksum failed.');
 if(!adValid(data.slice(68,68+boundary))||!adValid(data.slice(68+boundary,68+advLength)))throw new Error('Malformed advertisement data.');
 const profile={name:string(data,8,48),address:Array.from(data.slice(56,62),b=>b.toString(16).padStart(2,'0')).join(':'),addressType:data[62],partial:!!data[63],advertisement:Array.from(data.slice(68,68+advLength)),scanBoundary:boundary,attributes:[]};let services=0,chars=0;
 for(let i=0;i<count;i++){const o=HEADER+i*ATTRIBUTE,a={index:i,kind:data[o],properties:data[o+1],flags:data[o+2],parent:v.getUint16(o+4,true),handle:v.getUint16(o+6,true),length:v.getUint16(o+8,true),uuid:string(data,o+10,37)};
  if(a.kind>2||data[o+3]||a.flags>7||a.length>512||!/^([a-f0-9]{4}|[a-f0-9]{8}|[a-f0-9]{8}(-[a-f0-9]{4}){3}-[a-f0-9]{12})$/i.test(a.uuid))throw new Error('Malformed GATT attribute.');
  if(a.kind===0){if(a.parent!==65535||a.length||a.properties||a.flags||++services>16)throw new Error('Invalid service.');}
  else if(a.parent>=i||profile.attributes[a.parent].kind!==(a.kind===1?0:1)||(a.kind===1&&++chars>64))throw new Error('Invalid attribute hierarchy.');
  if(a.kind===2&&/^(?:280[0-3]|0000280[0-3](?:-0000-1000-8000-00805f9b34fb)?)$/i.test(a.uuid))throw new Error('GATT declarations are not descriptors.');
  a.value=Array.from(data.slice(o+47,o+47+a.length));profile.attributes.push(a);
 }return profile;
}
export function profileFile(data){decodeProfile(data,{withCrc:false});const file=new Uint8Array(data.length+4);file.set(data);new DataView(file.buffer).setUint32(data.length,crc32(data),true);return file;}
export function safeProfileName(name){if(!/^[a-z0-9_-]{1,32}$/i.test(name))throw new Error('Use 1–32 letters, numbers, underscores or hyphens.');return name;}
function receiveTransfer(link,command,timeout,consume){
 return new Promise((resolve,reject)=>{
  let settled=false,off=()=>{};
  const finish=(error,result)=>{if(settled)return;settled=true;clearTimeout(timer);off();error?reject(error):resolve(result);};
  const timer=setTimeout(()=>finish(new Error('USB transfer timed out.')),timeout);
  off=link.onLine(line=>{try{if(line===null||line?.startsWith('LAB_ERROR'))throw new Error(line||'USB disconnected.');const result=consume(line);if(result!==undefined)finish(null,result);}catch(e){finish(e);}});
  Promise.resolve().then(()=>link.write(command)).catch(error=>finish(error));
 });
}
export async function exportProfile(link){
 let data=null,received=0,crc=0;
 return receiveTransfer(link,'lab export',15000,line=>{let m;
  if((m=/^LAB_FILE (\d+) ([a-f0-9]{8})$/i.exec(line))){const n=Number(m[1]);if(data||n<HEADER||n>MAX_PROFILE)throw new Error('Invalid export header.');data=new Uint8Array(n);crc=parseInt(m[2],16);}
  else if((m=/^LAB_DATA (\d+) ([a-f0-9]+)$/i.exec(line))){if(!data||Number(m[1])!==received||m[2].length%2)throw new Error('Profile transfer has missing data.');const chunk=Uint8Array.from(m[2].match(/../g),x=>parseInt(x,16));if(received+chunk.length>data.length)throw new Error('Profile transfer overflow.');data.set(chunk,received);received+=chunk.length;}
  else if(line==='LAB_FILE_END'){if(!data||received!==data.length||crc32(data)!==crc)throw new Error('Profile transfer checksum failed.');return profileFile(data);}
 });
}
export async function importProfile(link,file,onProgress=()=>{}){
 decodeProfile(file);const data=file.slice(0,-4);link.lines=[];await link.write(`lab import ${data.length} ${crc32(data).toString(16)}`);let reply=await link.wait(s=>s==='LAB_READY'||s.startsWith('LAB_ERROR'));if(reply!=='LAB_READY')throw new Error(reply);
 let offset=0;while(offset<data.length){const end=Math.min(offset+128,data.length);await link.write(data.slice(offset,end));while(true){reply=await link.wait(s=>s.startsWith('LAB_ACK ')||s.startsWith('LAB_ERROR'));if(reply.startsWith('LAB_ERROR'))throw new Error(reply);const ack=Number(reply.slice(8));if(!Number.isInteger(ack)||ack>end)throw new Error('Invalid profile acknowledgement.');if(ack===end)break;}offset=end;onProgress(Math.round(100*offset/data.length));}
 reply=await link.wait(s=>s==='LAB_OK imported'||s.startsWith('LAB_ERROR'));if(reply!=='LAB_OK imported')throw new Error(reply);
}
export async function labLines(link,command){link.lines=[];const lines=[];const off=link.onLine(line=>{if(line?.startsWith('LAB_'))lines.push(line);});try{await link.write(command);await link.wait(line=>line==='LAB_END'||line.startsWith('LAB_ERROR'),10000);if(lines.some(l=>l.startsWith('LAB_ERROR')))throw new Error(lines.join('\n'));return lines;}finally{off();}}
export async function exportLog(link,previous=false){
 let expected=-1,body='',header=false;
 return receiveTransfer(link,'lab log export '+(previous?'previous':'current'),60000,line=>{
  const m=/^LAB_LOG_BEGIN (\d+)$/.exec(line);
  if(m){if(expected>=0)throw new Error('Duplicate log header.');expected=Number(m[1]);if(expected>1024*1024)throw new Error('Log exceeds size limit.');}
  else if(line==='uptime_ms,kind,attribute,length,hex')header=true;
  else if(expected>=0&&/^\d+,\d+,\d+,\d+,(?:[a-f0-9]{2}(?: |$))*$/.test(line)){body+=line+'\n';if(body.length>expected)throw new Error('Invalid log size.');}
  else if(line==='LAB_LOG_END'){if(expected!==body.length||!header)throw new Error('Incomplete log transfer.');return 'uptime_ms,kind,attribute,length,hex\n'+body;}
 });
}
