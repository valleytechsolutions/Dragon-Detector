import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=readFileSync(new URL('../firmware/include/ui_palette.h',import.meta.url),'utf8');
const palettes=[...source.matchAll(/\{(0x[\da-f]+(?:,0x[\da-f]+){7}),(true|false)\}/g)].map(m=>m[1].split(',').map(Number));
const luminance=c=>[c>>16,(c>>8)&255,c&255].map(v=>{v/=255;return v<=.04045?v/12.92:((v+.055)/1.055)**2.4;}).reduce((sum,v,i)=>sum+v*[.2126,.7152,.0722][i],0);
const contrast=(a,b)=>{a=luminance(a);b=luminance(b);return (Math.max(a,b)+.05)/(Math.min(a,b)+.05);};
test('every GUI palette keeps normal, secondary and action text legible',()=>{
  assert.ok(palettes.length>0,'No palette definitions parsed');
  for(const [index,[bg,surface,raised,ink,muted,accent,border,onAccent]] of palettes.entries()){
    for(const fill of [bg,surface,raised])assert.ok(contrast(ink,fill)>=4.5,`theme ${index}: primary text`);
    for(const fill of [bg,surface])assert.ok(contrast(muted,fill)>=4.5,`theme ${index}: secondary text`);
    assert.ok(contrast(onAccent,accent)>=4.5,`theme ${index}: filled action`);
  }
});
