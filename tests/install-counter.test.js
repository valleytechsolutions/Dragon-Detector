import test from 'node:test';
import assert from 'node:assert/strict';
import {randomUUID} from 'node:crypto';
import {DatabaseSync} from 'node:sqlite';
import worker,{InstallCounter} from '../services/install-counter/worker.js';
import {createInstallCounter,isProductionOrigin} from '../web/install-counter.js';

const origin='https://valleytechsolutions.github.io';
function fixture(){
  const db=new DatabaseSync(':memory:');
  const ctx={storage:{sql:{exec(query,...args){const rows=db.prepare(query).all(...args);return {one:()=>rows[0]};}}}};
  let counter=new InstallCounter(ctx);
  return {db,env:{INSTALLS:{idFromName:x=>x,get:()=>counter}},restart(){counter=new InstallCounter(ctx);}};
}
const req=(method='GET',id=randomUUID(),site=origin,path='/installs')=>new Request('https://counter'+path,{method,headers:{Origin:site,'Content-Type':'application/json'},...(method==='POST'?{body:JSON.stringify({id})}:{})});
const count=async(f,path='/installs')=>(await (await worker.fetch(req('GET',undefined,origin,path),f.env)).json()).count;
test('installs begin at zero independently of any historical visits',async()=>{
  const f=fixture();try{
    f.db.exec('CREATE TABLE visits (count INTEGER); INSERT INTO visits VALUES (9876)');
    assert.equal(await count(f),0);f.restart();assert.equal(await count(f),0);
    assert.equal((await worker.fetch(req('POST',randomUUID(),origin,'/visits'),f.env)).status,404);
    assert.equal(f.db.prepare('SELECT count FROM visits').get().count,9876);
  }finally{f.db.close();}
});
test('both production origins can record installs; previews and unrelated origins cannot',async()=>{
  const f=fixture();try{
    for(const site of [origin,'https://valleytech-dragon-detector.pages.dev']){
      assert.equal(isProductionOrigin(site),true);assert.equal((await worker.fetch(req('POST',randomUUID(),site),f.env)).status,200);
    }
    for(const site of ['https://preview.valleytech-dragon-detector.pages.dev','https://valleytech-dragon-detector.pages.dev.evil.test','http://localhost:8149','']){
      assert.equal(isProductionOrigin(site),false);assert.equal((await worker.fetch(req('POST',randomUUID(),site),f.env)).status,403);
    }
    assert.equal(await count(f),2);
  }finally{f.db.close();}
});
test('atomic increments and retry deduplication survive object reconstruction',async()=>{
  const f=fixture();try{
    const ids=Array.from({length:40},randomUUID);
    await Promise.all(ids.flatMap(id=>[worker.fetch(req('POST',id),f.env),worker.fetch(req('POST',id),f.env)]));
    assert.equal(await count(f),40);f.restart();await worker.fetch(req('POST',ids[0].toUpperCase()),f.env);assert.equal(await count(f),40);
    f.db.prepare('UPDATE install_events SET created_at=0').run();await worker.fetch(req('POST'),f.env);
    assert.equal(await count(f),41);assert.equal(f.db.prepare('SELECT count(*) AS n FROM install_events').get().n,1);
  }finally{f.db.close();}
});
test('bad routes, methods, IDs and bounded request bodies cannot increment',async()=>{
  const f=fixture();try{
    for(const [path,method,status] of [['/else','POST',404],['/installs','DELETE',405],['/installs','OPTIONS',204]])assert.equal((await worker.fetch(req(method,undefined,origin,path),f.env)).status,status);
    for(const body of ['{}','null','oops',JSON.stringify({id:'not-an-id'}),'x'.repeat(129)]){
      const r=await worker.fetch(new Request('https://counter/installs',{method:'POST',headers:{Origin:origin},body}),f.env);assert.ok([400,413].includes(r.status));
    }
    const r=await worker.fetch(req('OPTIONS'),f.env);assert.equal(r.headers.get('Access-Control-Allow-Headers'),'Content-Type');assert.equal(await count(f),0);
  }finally{f.db.close();}
});
test('page loads only read, success reports only from production, totals format correctly',async()=>{
  for(const production of [true,false]){
    const el={textContent:'',closest:()=>null},methods=[];
    const counter=createInstallCounter(el,{production,fetcher:async(_url,options)=>{methods.push(options.method);return Response.json({count:1234});}});
    await counter.refresh();assert.deepEqual(methods,['GET']);assert.equal(el.textContent,'1,234');
    await counter.recordInstall();assert.deepEqual(methods,production?['GET','POST']:['GET']);
  }
});
test('a lost POST response retries the same ID and never double counts',async()=>{
  const f=fixture();try{
    let attempts=0;const bodies=[],el={textContent:'',closest:()=>null};
    const counter=createInstallCounter(el,{production:true,fetcher:async(url,options)=>{
      bodies.push(options.body);const r=await worker.fetch(new Request(url,{...options,headers:{...options.headers,Origin:origin}}),f.env);
      if(++attempts===1)throw new Error('response lost');return r;
    }});
    await counter.recordInstall();assert.equal(attempts,2);assert.equal(bodies[0],bodies[1]);assert.equal(await count(f),1);assert.equal(el.textContent,'1');
  }finally{f.db.close();}
});
test('slow GET cannot replace a newer success count; counter outages never escape to USB flow',async()=>{
  let resolveGet;const el={textContent:'',closest:()=>null};
  const counter=createInstallCounter(el,{production:true,fetcher:async(_url,o)=>o.method==='GET'?new Promise(resolve=>resolveGet=resolve):Response.json({count:2})});
  const reading=counter.refresh();await counter.recordInstall();resolveGet(Response.json({count:1}));await reading;assert.equal(el.textContent,'2');
  for(const fetcher of [async()=>{throw new Error('offline')},async()=>Response.json({count:-1}),async()=>Response.json({count:'9000'}),async()=>new Response('',{status:503})]){
    const element={textContent:'',closest:()=>null};const c=createInstallCounter(element,{production:true,fetcher});await c.refresh();await c.recordInstall();assert.equal(element.textContent,'—');
  }
});
