import test from 'node:test';
import assert from 'node:assert/strict';
import {firmwareBase} from '../web/release-source.js';
import {checkRelease} from '../web/install-checks.js';
import {readFileSync} from 'node:fs';

const official='https://valleytechsolutions.github.io/Dragon-Detector-Flasher/';
const board='waveshare_185b',version='0.14.11';
const path=`firmware/v${version}/${board}`;

test('stable boards follow the matching official release catalog',async()=>{
  const requests=[];
  const base=await firmwareBase(board,{fetchImpl:async(url,options)=>{
    requests.push([url,options.cache]);
    return {ok:true,json:async()=>({tag:`v${version}`,boards:[{id:board,version,path}]})};
  }});
  assert.equal(base,`${official}${path}/`);
  assert.deepEqual(requests,[[`${official}catalog.json`,'no-cache']]);
  const fixture=JSON.parse(readFileSync(new URL('./fixtures/waveshare-185b-0.14.1-manifests.json',import.meta.url)));
  const manifest=structuredClone(fixture.update);manifest.version=version;
  const fetched=[];
  await checkRelease(board,'update',{base,fetchImpl:async(url,options)=>{
    fetched.push([url,options.method||'GET']);
    return url.endsWith('.json')?{ok:true,json:async()=>manifest}:{ok:true,headers:new Headers({'content-length':'4096','content-type':'application/octet-stream'})};
  }});
  assert.deepEqual(fetched[0],[`${base}update.json`,'GET']);
  assert.ok(fetched.some(([url,method])=>url===`${base}firmware.bin`&&method==='HEAD'));
});

test('separate beta packages stay local and malformed catalogs cannot select firmware',async()=>{
  assert.equal(await firmwareBase('guition_185_beta'), 'firmware/guition_185_beta/');
  assert.equal(await firmwareBase('waveshare_p4_knob'), 'firmware/waveshare_p4_knob/');
  for(const catalog of [{tag:'v0.14.11',boards:[{id:board,version,path:'firmware/other'}]},
    {tag:'../0.14.11',boards:[{id:board,version,path}]},{tag:42,boards:[]}]){
    await assert.rejects(firmwareBase(board,{fetchImpl:async()=>({ok:true,json:async()=>catalog})}),/matching build/);
  }
});
