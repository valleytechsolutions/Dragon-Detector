import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {checkRelease,usbErrorText,installDiagnostics} from '../web/install-checks.js';
import {boardProfiles} from '../web/board-profiles.js';
import {resourcePack} from '../web/sd-resources.js';
const published=JSON.parse(readFileSync(new URL('./fixtures/release-0.13.26-manifests.json',import.meta.url)));
published.m5dial=JSON.parse(readFileSync(new URL('./fixtures/dial-0.13.27-manifests.json',import.meta.url)));

published.watcher=JSON.parse(readFileSync(new URL('./fixtures/watcher-0.13.29-manifests.json',import.meta.url)));

function rig(board='waveshare',mode='factory',badFile){
  const manifest=structuredClone(published[board][mode]);
  const requests=[];
  return {manifest,requests,fetchImpl:async(url,options)=>{
    requests.push([url,options]);
    if(url.endsWith('.json'))return {ok:true,json:async()=>manifest};
    return {ok:true,headers:new Headers({'content-length':'4096','content-type':'application/octet-stream'}),...badFile};
  }};
}

test('SD-required manifests check their pack before USB; optional packs do not prevent boot',async()=>{
  const r=rig('waveshare_175');r.manifest.version='0.13.28';r.manifest.sdResources={...resourcePack,required:true};
  const fetchBase=r.fetchImpl;r.fetchImpl=async(url,opts)=>url===resourcePack.path?{ok:false,status:404}:fetchBase(url,opts);
  await assert.rejects(checkRelease('waveshare_175','factory',r),/Required SD resource pack/);
  r.manifest.sdResources.required=false;
  assert.equal((await checkRelease('waveshare_175','factory',r)).version,'0.13.28');
});

test('all board profiles and both modes check every required file before allowing USB',async()=>{
  for(const board of Object.keys(boardProfiles))for(const mode of ['factory','update']){
    const r=rig(board,mode);const data=await checkRelease(board,mode,r);
    assert.equal(data.version,r.manifest.version);
    assert.deepEqual(r.requests.slice(1).map(([url,opts])=>[url,opts.method]),r.manifest.builds[0].parts.map(p=>[`firmware/${board}/${p.path}`,'HEAD']));
  }
});

test('missing, empty, HTML fallback and oversized firmware keep installation unavailable',async()=>{
  for(const response of [{ok:false,status:404},
    {headers:new Headers({'content-length':'0'})},
    {headers:new Headers({'content-length':'4096','content-type':'text/html'})},
    {headers:new Headers({'content-length':String(64*1024*1024)})}]){
    await assert.rejects(checkRelease('waveshare','factory',rig('waveshare','factory',response)),/unavailable|valid firmware|capacity/);
  }
});

test('wrong manifest is rejected before requesting binaries',async()=>{
  const r=rig();r.manifest.builds[0].parts[0].path='https://example.com/wrong.bin';
  await assert.rejects(checkRelease('waveshare','factory',r),/does not match/);assert.equal(r.requests.length,1);
});

test('Dial rejects the old beta release manifest while supporting its USB upgrade identity',async()=>{
  const r=rig('m5dial');r.manifest.version='0.13.26';
  await assert.rejects(checkRelease('m5dial','factory',r),/0.13.27/);
  assert.equal(r.requests.length,1);
});

test('CDN HEAD responses may omit content length without making the release unavailable',async()=>{
  const data=await checkRelease('waveshare','factory',rig('waveshare','factory',{headers:new Headers({'content-type':'application/octet-stream'})}));
  assert.equal(data.version,'0.13.26');
});

test('network errors, timeouts and cancellation have bounded recovery messages',async()=>{
  await assert.rejects(checkRelease('waveshare','factory',{fetchImpl:async()=>{throw new TypeError('Failed to fetch');}}),/internet connection/);
  const fetchImpl=async(url,{signal})=>new Promise((resolve,reject)=>{
    const abort=()=>reject(new DOMException('Aborted','AbortError'));
    if(signal.aborted)abort();else signal.addEventListener('abort',abort,{once:true});
  });
  await assert.rejects(checkRelease('waveshare','factory',{fetchImpl,timeoutMs:5}),/timed out/);
  const controller=new AbortController();controller.abort();
  await assert.rejects(checkRelease('waveshare','factory',{fetchImpl,signal:controller.signal}),/cancelled/);
});

test('USB denial, empty chooser and busy-port failures have distinct next steps',()=>{
  assert.match(usbErrorText({name:'NotFoundError'}),/Nothing was installed/);
  assert.match(usbErrorText({name:'SecurityError'}),/direct flasher/);
  assert.match(usbErrorText({name:'NetworkError'}),/Close other flashers/);
  const report=installDiagnostics({board:'Waveshare Lite',mode:'factory',status:'No port selected',userAgent:'test',installerReady:true});
  assert.match(report,/Board selected: Waveshare Lite/);assert.match(report,/No port selected/);
});
