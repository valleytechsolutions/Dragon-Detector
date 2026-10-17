import test from 'node:test';
import assert from 'node:assert/strict';
import {startRadarBackground} from '../web/radar-background.js';
test('background pauses on request, hidden tabs and reduced motion, with bounded backing size',()=>{
  const original=new Map(),events={},frames=new Map(),saved=new Map();let id=0,paints=0;
  const ctx=new Proxy({clearRect(){paints++;}},{get:(obj,key)=>key in obj?obj[key]:()=>{}});
  const canvas={getContext:()=>ctx},button={addEventListener:(name,f)=>events[name]=f,setAttribute(){}},motion={matches:false,addEventListener:(_,f)=>events.motion=f};
  const globals={innerWidth:1000,innerHeight:800,devicePixelRatio:3,matchMedia:()=>motion,localStorage:{getItem:k=>saved.get(k),setItem:(k,v)=>saved.set(k,v)},document:{hidden:false,addEventListener:(n,f)=>events[n]=f},addEventListener:(n,f)=>events[n]=f,requestAnimationFrame:f=>{frames.set(++id,f);return id;},cancelAnimationFrame:n=>frames.delete(n)};
  for(const [key,value] of Object.entries(globals)){original.set(key,Object.getOwnPropertyDescriptor(globalThis,key));Object.defineProperty(globalThis,key,{configurable:true,writable:true,value});}
  try{
    startRadarBackground(canvas,button);assert.equal(canvas.width,1500);assert.equal(canvas.height,1200);assert.equal(frames.size,1);
    const [frame,run]=frames.entries().next().value;frames.delete(frame);run(50);assert.ok(paints>=2);assert.equal(frames.size,1);
    events.click();assert.equal(frames.size,0);assert.equal(button.textContent,'Resume radar');assert.equal(saved.get('radar-motion'),'paused');
    events.click();assert.equal(frames.size,1);document.hidden=true;events.visibilitychange();assert.equal(frames.size,0);
    document.hidden=false;events.visibilitychange();assert.equal(frames.size,1);
    motion.matches=true;events.motion();assert.equal(frames.size,0);assert.equal(button.disabled,true);
    motion.matches=false;events.motion();assert.equal(frames.size,1);
  }finally{for(const [key,descriptor] of original){if(descriptor)Object.defineProperty(globalThis,key,descriptor);else delete globalThis[key];}}
});
