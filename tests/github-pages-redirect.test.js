import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const html=readFileSync(new URL('../hosting/github-pages/index.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const host='https://valleytechsolutions.github.io';
const release='https://valleytechsolutions.github.io/Dragon-Detector-Flasher/';
const companion='https://valleytech-dragon-detector.pages.dev/';
function forward(input){
  const original=new URL(input),link={};let target;
  runInNewContext(script,{URL,URLSearchParams,location:{pathname:original.pathname,search:original.search,hash:original.hash,replace:value=>target=value},document:{getElementById:()=>link}});
  assert.equal(link.href,target);
  return target;
}
test('installation entry points use the release flasher and retain board selection',()=>{
  for(const path of ['/Dragon-Detector','/Dragon-Detector/','/Dragon-Detector/index.html']){
    for(const hash of ['', '#install', '#clock-sync-panel'])assert.equal(forward(host+path+hash),release);
  }
  assert.equal(forward(host+'/Dragon-Detector/?board=stopwatch#install'),release+'?board=stopwatch');
});
test('badge, theme, hardware and separate board links stay on the companion site',()=>{
  for(const hash of ['#studio','#theme-studio','#buy-hardware','#esp32-drivers','#locksport']){
    assert.equal(forward(host+'/Dragon-Detector/?board=stopwatch'+hash),companion+'?board=stopwatch'+hash);
  }
  for(const board of ['waveshare_p4_knob','guition_185_beta','t5-e1']){
    assert.equal(forward(host+'/Dragon-Detector/?board='+board),companion+'?board='+board+'#install');
  }
});
test('old guide and file paths keep working without allowing an external redirect',()=>{
  assert.equal(forward(host+'/Dragon-Detector/guides/FLASHING.html#recovery'),companion+'guides/FLASHING.html#recovery');
  assert.equal(forward(host+'/Dragon-Detector/firmware/waveshare_p4_knob/factory.json'),companion+'firmware/waveshare_p4_knob/factory.json');
  assert.equal(new URL(forward(host+'/Dragon-Detector//example.com/?redirect=https://example.com')).origin,new URL(companion).origin);
  assert.equal(new URL(forward(host+'/Dragon-Detector/?redirect=https://example.com')).origin,new URL(release).origin);
  assert.ok(html.includes('<noscript><meta http-equiv="refresh" content="0;url='+release+'"></noscript>'));
});
test('README exposes the release installer and both artwork editors',()=>{
  const readme=readFileSync(new URL('../README.md',import.meta.url),'utf8');
  assert.ok(readme.includes('**[Open the web flasher →]('+release+')**'));
  assert.ok(readme.includes(companion+'#studio'));
  assert.ok(readme.includes(companion+'#theme-studio'));
  assert.ok(!readme.includes('Cloudflare address above remains the primary'));
});
