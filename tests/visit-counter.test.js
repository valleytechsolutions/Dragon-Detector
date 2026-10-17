import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import worker,{VisitCounter} from '../services/visit-counter/worker.js';
import {updateVisitCounter} from '../web/visit-counter.js';

const origin='https://valleytechsolutions.github.io';
function fixture(){
  const db=new DatabaseSync(':memory:');
  const ctx={storage:{sql:{exec(query){const rows=db.prepare(query).all();return {one:()=>rows[0]};}}}};
  let counter=new VisitCounter(ctx);
  return {db,env:{VISITS:{idFromName:x=>x,get:()=>counter}},restart(){counter=new VisitCounter(ctx);}};
}
test('shared visits increment atomically and survive object reconstruction',async()=>{
  const f=fixture();try{
    const requests=Array.from({length:40},()=>worker.fetch(new Request('https://counter/visits',{method:'POST',headers:{Origin:origin}}),f.env));
    const counts=await Promise.all(requests.map(async p=>(await (await p).json()).count));
    assert.deepEqual(counts.sort((a,b)=>a-b),Array.from({length:40},(_,i)=>i+1));
    f.restart();const r=await worker.fetch(new Request('https://counter/visits'),f.env);assert.equal((await r.json()).count,40);
    assert.equal(r.headers.get('Access-Control-Allow-Origin'),'*');assert.equal(r.headers.get('Cache-Control'),'no-store');
  }finally{f.db.close();}
});
test('invalid routes, methods and foreign origins cannot increment',async()=>{
  const f=fixture();try{
    for(const [path,method,site,status] of [['/visits','POST','https://example.org',403],['/visits','POST','',403],['/else','POST',origin,404],['/visits','DELETE',origin,405],['/visits','OPTIONS',origin,204]])assert.equal((await worker.fetch(new Request('https://counter'+path,{method,headers:{Origin:site}}),f.env)).status,status);
    assert.equal((await (await worker.fetch(new Request('https://counter/visits'),f.env)).json()).count,0);
  }finally{f.db.close();}
});
test('frontend counts only production and formats the real shared total',async()=>{
  for(const production of [true,false]){
    const el={textContent:'',closest:()=>null};let method;
    await updateVisitCounter(el,{production,fetcher:async(_url,options)=>{method=options.method;return Response.json({count:1234});}});
    assert.equal(method,production?'POST':'GET');assert.equal(el.textContent,'1,234');
  }
});
test('failed or malformed counter responses never display a fabricated total',async()=>{
  for(const fetcher of [async()=>{throw new Error('offline')},async()=>Response.json({count:-1}),async()=>Response.json({count:'9000'}),async()=>new Response('',{status:503})]){
    const el={textContent:'',closest:()=>null};await updateVisitCounter(el,{production:false,fetcher});assert.equal(el.textContent,'—');
  }
});
