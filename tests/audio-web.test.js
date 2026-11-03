import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {hasWebAudio,parseAudioSettings,readAudioSettings,saveAudioSettings,previewAudioTone} from '../web/audio-transfer.js';

function status(s,suffix=''){
  return `TONE_SETTINGS ready=${Number(s.ready)} enabled=${Number(s.enabled)} volume=${s.volume} hunt=${Number(s.hunt)} focus=${s.focus} quiet=${Number(s.quiet)}${suffix}`;
}
function rig(overrides={},apply=true){
  const state={ready:true,enabled:true,volume:35,hunt:false,focus:3,quiet:false,...overrides};
  const writes=[];let acknowledgement=false;
  const link={async write(command){
    writes.push(command);
    if(command.startsWith('tone config ')){
      acknowledgement=true;
      if(apply){const [enabled,volume,hunt,focus]=command.slice(12).split(' ').map(Number);Object.assign(state,{enabled:!!enabled,volume,hunt:!!hunt,focus});}
    }else if(command.startsWith('tone preview '))acknowledgement=true;
  },async wait(match){const line=acknowledgement?'TONE_OK':status(state);acknowledgement=false;assert.ok(match(line));return line;}};
  return {link,state,writes};
}

test('audio parser accepts legacy settings and appended status while retaining raw gains',()=>{
  for(const [enabled,volume,level] of [[false,100,'silent'],[true,0,'silent'],[true,1,'medium'],[true,35,'medium'],[true,50,'medium'],[true,51,'high'],[true,100,'high']]){
    const raw={ready:true,enabled,volume,hunt:true,focus:26,quiet:true};
    for(const extra of ['',` level=${level}`,` level=${level} codec=es8311`])assert.deepEqual(parseAudioSettings(status(raw,extra)),{...raw,level});
  }
  assert.equal(parseAudioSettings('TONE_SETTINGS ready=0 enabled=1 volume=100 hunt=1 focus=-1 quiet=0 level=silent').level,'silent');
});

test('malformed audio state cannot pass readback validation',()=>{
  const valid='TONE_SETTINGS ready=1 enabled=1 volume=50 hunt=1 focus=-1 quiet=0';
  for(const line of ['',valid+'\n',valid.replace('ready=1','ready=2'),valid.replace('enabled=1','enabled=-1'),valid.replace('volume=50','volume=101'),valid.replace('volume=50','volume=050'),valid.replace('focus=-1','focus=27'),valid.replace('focus=-1','focus=-2'),valid.replace('hunt=1','hunt=3'),valid.replace('quiet=0','quiet=2'),valid+' volume=100',valid+' level=high',valid+' level=medium level=medium',valid+' junk'])assert.throws(()=>parseAudioSettings(line),/Invalid audio settings/);
});

test('loading audio is read-only even for muted legacy and non-preset gains',async()=>{
  for(const overrides of [{enabled:false,volume:80},{enabled:true,volume:35},{enabled:true,volume:75},{ready:false,volume:100}]){
    const r=rig(overrides);const before={...r.state};
    const loaded=await readAudioSettings(r.link);
    assert.equal(loaded.volume,before.volume);assert.equal(loaded.enabled,before.enabled);
    assert.deepEqual(r.state,before);assert.deepEqual(r.writes,['tone settings']);
  }
});

test('three sound presets use backward-compatible commands and preserve hunt/focus',async()=>{
  for(const [level,enabled,volume] of [['silent',false,0],['medium',true,50],['high',true,100]])for(const hunt of [false,true])for(const focus of [-1,3,26]){
    const r=rig();const saved=await saveAudioSettings(r.link,{level,hunt,focus});
    assert.deepEqual(r.writes,[`tone config ${Number(enabled)} ${volume} ${Number(hunt)} ${focus}`,'tone settings']);
    assert.deepEqual(saved,{ready:true,enabled,volume,hunt,focus,quiet:false,level});
  }
});

test('invalid audio preferences never write to USB',async()=>{
  const r=rig();
  for(const level of [null,undefined,'','loud','Medium','high\ntone play 0',{},'__proto__'])await assert.rejects(saveAudioSettings(r.link,{level,hunt:true,focus:-1}),/Invalid audio preferences/);
  for(const hunt of [undefined,0,1,'true'])await assert.rejects(saveAudioSettings(r.link,{level:'high',hunt,focus:-1}),/Invalid audio preferences/);
  for(const focus of [-2,27,NaN,1.5,'3','-1\ntone play 0'])await assert.rejects(saveAudioSettings(r.link,{level:'medium',hunt:true,focus}),/Invalid audio preferences/);
  assert.deepEqual(r.writes,[]);
});

test('audio save compares exact raw settings rather than only the displayed preset',async()=>{
  for(const changed of [{enabled:false},{volume:35},{hunt:false},{focus:2}]){
    const r=rig({enabled:true,volume:50,hunt:true,focus:3,...changed},false);
    await assert.rejects(saveAudioSettings(r.link,{level:'medium',hunt:true,focus:3}),/not saved/);
    assert.deepEqual(r.writes,['tone config 1 50 1 3','tone settings','tone settings']);
  }
});

test('audio save retries only stale readback and propagates rejection or disconnect',async()=>{
  const r=rig({enabled:true,volume:100,hunt:true,focus:3});const originalWait=r.link.wait;let stale=true;
  r.link.wait=async match=>{if(r.writes.at(-1)==='tone settings'&&stale){stale=false;return 'TONE_SETTINGS ready=1 enabled=1 volume=35 hunt=1 focus=3 quiet=0';}return originalWait(match);};
  assert.equal((await saveAudioSettings(r.link,{level:'medium',hunt:true,focus:3})).volume,50);
  assert.deepEqual(r.writes,['tone config 1 50 1 3','tone settings','tone settings']);
  for(const failure of ['TONE_ERROR storage','disconnect']){
    const writes=[];const link={async write(c){writes.push(c);},async wait(){if(failure==='disconnect')throw new Error('USB disconnected');return failure;}};
    await assert.rejects(saveAudioSettings(link,{level:'silent',hunt:false,focus:-1}),failure==='disconnect'?/disconnected/:/storage/);
    assert.deepEqual(writes,['tone config 0 0 0 -1']);
  }
});

test('Silent and unready speakers cannot send previews, including legacy muted gains',async()=>{
  const selectedSilent=rig();await assert.rejects(previewAudioTone(selectedSilent.link,{level:'silent',family:3,tone:'test:d=4,o=5,b=120:c'}),/Medium or High/);assert.deepEqual(selectedSilent.writes,[]);
  for(const state of [{enabled:false,volume:100},{enabled:true,volume:0},{ready:false,enabled:true,volume:100},{enabled:true,volume:50}]){
    const r=rig(state);
    await assert.rejects(previewAudioTone(r.link,{level:'high',family:3,tone:'test:d=4,o=5,b=120:c'}),/Save your sound level|initialize/);
    assert.deepEqual(r.writes,['tone settings']);
  }
});

test('preview honors saved active levels without changing quiet, hunt, focus or legacy gain',async()=>{
  for(const [level,volume] of [['medium',35],['medium',50],['high',75],['high',100]]){
    const r=rig({enabled:true,volume,hunt:false,focus:26,quiet:true});const before={...r.state};
    await previewAudioTone(r.link,{level,family:3,tone:'test:d=4,o=5,b=120:c'});
    assert.deepEqual(r.writes,['tone settings','tone preview 3 test:d=4,o=5,b=120:c']);assert.deepEqual(r.state,before);
  }
});

test('audio capabilities keep no-speaker and compact web profiles unavailable',()=>{
  for(const board of ['waveshare_185b','watcher','waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear'])assert.ok(hasWebAudio(board,'0.14.2'));
  for(const board of ['m5dial','waveshare_lite','lilygo_175','xiao_round','elecrow_21','unknown'])assert.equal(hasWebAudio(board,'0.14.2'),false);
  assert.equal(hasWebAudio('waveshare_185b','0.14.0'),false);assert.equal(hasWebAudio('stopwatch','0.3.9'),false);assert.equal(hasWebAudio('stopwatch','0.15.0'),false);
});

test('sound UI offers exactly High, Medium and Silent without a separate master switch',()=>{
  const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
  const options=html.match(/<select[^>]*id="sound-level"[^>]*>([\s\S]*?)<\/select>/)[1];
  assert.deepEqual([...options.matchAll(/value="([^"]+)"[^>]*>([^<]+)<\/option>/g)].map(m=>m.slice(1)),[['high','High'],['medium','Medium'],['silent','Silent']]);
  assert.doesNotMatch(html,/id="sound-(?:volume|enabled|volume-label)"/);
});
