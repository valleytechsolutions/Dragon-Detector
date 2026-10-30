import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const html=readFileSync(new URL('../hosting/github-pages/index.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
function forward(input){
  const original=new URL(input),link={};let target;
  runInNewContext(script,{URL,location:{pathname:original.pathname,search:original.search,hash:original.hash,replace:value=>target=value},document:{getElementById:()=>link}});
  assert.equal(link.href,target);
  return target;
}
test('old entry links open the primary installer and retain tab and query links',()=>{
  const host='https://valleytechsolutions.github.io';
  for(const path of ['/Dragon-Detector','/Dragon-Detector/','/Dragon-Detector/index.html'])assert.equal(forward(host+path),'https://valleytech-dragon-detector.pages.dev/#install');
  assert.equal(forward(host+'/Dragon-Detector/?board=stopwatch#theme-studio'),'https://valleytech-dragon-detector.pages.dev/?board=stopwatch#theme-studio');
});
test('custom 404 forwards old guide paths while keeping the destination host fixed',()=>{
  assert.equal(forward('https://valleytechsolutions.github.io/Dragon-Detector/guides/FLASHING.html#recovery'),'https://valleytech-dragon-detector.pages.dev/guides/FLASHING.html#recovery');
  assert.equal(new URL(forward('https://valleytechsolutions.github.io/Dragon-Detector//example.com/?redirect=https://example.com')).origin,'https://valleytech-dragon-detector.pages.dev');
  assert.match(html,/<noscript><meta http-equiv="refresh" content="0;url=https:\/\/valleytech-dragon-detector.pages.dev\/#install"><\/noscript>/);
});
