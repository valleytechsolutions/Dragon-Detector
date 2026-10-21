import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';

test('vendored installer matches upstream hashes and every bundled import stays local',()=>{
  const base=new URL('../web/vendor/esp-web-tools-10.4.0/',import.meta.url);
  const provenance=JSON.parse(readFileSync(new URL('UPSTREAM.json',base)));
  assert.equal(provenance.version,'10.4.0');assert.ok(provenance.files_sha256.LICENSE);
  let imports=0;
  for(const [file,hash] of Object.entries(provenance.files_sha256)){
    const data=readFileSync(new URL(file,base));
    assert.equal(createHash('sha256').update(data).digest('hex'),hash,file);
    if(!file.endsWith('.js'))continue;
    for(const match of data.toString().matchAll(/(?:from\s*|import\s*\(\s*)["']([^"']+)["']/g)){
      imports++;assert.ok(match[1].startsWith('./'),`${file}: ${match[1]}`);
      assert.ok(existsSync(new URL(match[1],base)),match[1]);
    }
  }
  assert.ok(imports>20);
});
