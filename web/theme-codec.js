import {crc32} from './badge-codec.js?v=2026-10-25-locksport-01338';

export const LOGO_SIDE=160,HEADER_SIZE=64,THEME_SIZE=HEADER_SIZE+LOGO_SIDE*LOGO_SIDE*4+4;
export const defaultTheme=Object.freeze({name:'My theme',layout:1,hands:0,font:0,markers:0,seconds:true,date:true,hour24:true,locksport:false,logoSize:1,sweep:0,bg:'#080d16',ink:'#f3f6fb',accent:'#68e2c1',secondary:'#72aaff'});
const choices={layout:2,hands:3,font:2,markers:3,logoSize:2,sweep:2};
export function validateTheme(theme){
  if(!theme||typeof theme.name!=='string'||!/^[\x20-\x7e]{1,23}$/.test(theme.name)||!theme.name.trim())throw new Error('Use a theme name with 1–23 printable English characters.');
  for(const [key,max] of Object.entries(choices))if(!Number.isInteger(theme[key])||theme[key]<0||theme[key]>max)throw new Error(`Invalid ${key} choice.`);
  for(const key of ['seconds','date','hour24','locksport'])if(typeof theme[key]!=='boolean')throw new Error(`Invalid ${key} setting.`);
  for(const key of ['bg','ink','accent','secondary'])if(!/^#[\da-f]{6}$/i.test(theme[key]))throw new Error(`Invalid ${key} color.`);
  return theme;
}
export function encodeTheme(theme,rgba){
  validateTheme(theme);if(!(rgba instanceof Uint8Array||rgba instanceof Uint8ClampedArray)||rgba.length!==LOGO_SIDE*LOGO_SIDE*4)throw new Error('The logo must be 160 × 160 RGBA pixels.');
  const out=new Uint8Array(THEME_SIZE),view=new DataView(out.buffer);
  out.set([68,68,84,49,1,theme.layout,theme.hands,theme.font,theme.markers,Number(theme.seconds)|Number(theme.date)<<1|Number(theme.hour24)<<2|Number(theme.locksport)<<3,theme.logoSize,theme.sweep]);
  for(const [i,key] of ['bg','ink','accent','secondary'].entries())for(let channel=0;channel<3;channel++)out[12+i*3+channel]=parseInt(theme[key].slice(1+channel*2,3+channel*2),16);
  out.set(new TextEncoder().encode(theme.name),24);view.setUint16(48,160,true);view.setUint16(50,160,true);view.setUint32(52,rgba.length,true);out.set(rgba,HEADER_SIZE);view.setUint32(THEME_SIZE-4,crc32(out.subarray(0,-4)),true);return out;
}
export function decodeTheme(bytes){
  if(!(bytes instanceof Uint8Array)||bytes.length!==THEME_SIZE)throw new Error('This is not a complete Dragon Detector theme file.');
  const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  if(bytes.slice(0,5).join(',')!=='68,68,84,49,1'||v.getUint16(48,true)!==160||v.getUint16(50,true)!==160||v.getUint32(52,true)!==102400||bytes.slice(56,64).some(Boolean)||(bytes[9]&240))throw new Error('Unsupported theme format.');
  if(v.getUint32(THEME_SIZE-4,true)!==crc32(bytes.subarray(0,-4)))throw new Error('Theme checksum failed. The file may be incomplete or damaged.');
  const nameBytes=bytes.slice(24,48),end=nameBytes.indexOf(0);
  if(end<1||nameBytes.slice(end).some(Boolean)||nameBytes.slice(0,end).some(n=>n<32||n>126))throw new Error('Invalid theme name.');
  const theme={name:new TextDecoder().decode(nameBytes.slice(0,end)),layout:bytes[5],hands:bytes[6],font:bytes[7],markers:bytes[8],seconds:!!(bytes[9]&1),date:!!(bytes[9]&2),hour24:!!(bytes[9]&4),locksport:!!(bytes[9]&8),logoSize:bytes[10],sweep:bytes[11]};
  for(const [i,key] of ['bg','ink','accent','secondary'].entries())theme[key]='#'+Array.from(bytes.slice(12+i*3,15+i*3),n=>n.toString(16).padStart(2,'0')).join('');
  validateTheme(theme);return {theme,rgba:bytes.slice(HEADER_SIZE,-4)};
}
// Only connected edge pixels are removed; enclosed white details are retained.
export function removeEdgeBackground(rgba,width,height,target,tolerance=36){
  if(rgba.length!==width*height*4||!Number.isInteger(width)||!Number.isInteger(height)||width<1||height<1||width*height>1600*1600||![0,255].includes(target))throw new Error('Invalid background removal input.');
  const pixels=new Uint8ClampedArray(rgba),seen=new Uint8Array(width*height),queue=new Uint32Array(width*height);let head=0,tail=0;
  const add=i=>{if(seen[i])return;seen[i]=1;const p=i*4;if(pixels[p+3]===0||[0,1,2].every(c=>Math.abs(pixels[p+c]-target)<=tolerance)){queue[tail++]=i;pixels[p+3]=0;}};
  for(let x=0;x<width;x++){add(x);add((height-1)*width+x);}for(let y=0;y<height;y++){add(y*width);add(y*width+width-1);}
  while(head<tail){const i=queue[head++],x=i%width,y=Math.floor(i/width);if(x)add(i-1);if(x+1<width)add(i+1);if(y)add(i-width);if(y+1<height)add(i+width);}
  return pixels;
}
