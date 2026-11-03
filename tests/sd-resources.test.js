import test from 'node:test';
import assert from 'node:assert/strict';
import {boardProfiles} from '../web/board-profiles.js';
import {resourceSetup,resourcePack,labelResourcePack,hasSd,hasGpsAndSd} from '../web/sd-resources.js';
test('all exact board profiles gate SD resources and GPS independently',()=>{
 for(const board of [...Object.keys(boardProfiles),'t5_e1','unknown']){
  assert.equal(hasGpsAndSd(board),['waveshare_185b','watcher','waveshare_175_gps'].includes(board));
  assert.equal(resourceSetup(board,{version:'0.13.26'}),null);
  if(board==='waveshare_185b')continue;
  const m={version:board==='guition_185_beta'?'0.14.2':'0.13.28',sdResources:{...resourcePack}};
  if(hasSd(board)){assert.match(resourceSetup(board,m).title,/Optional/);m.sdResources.required=true;assert.match(resourceSetup(board,m).title,/REQUIRED/);}
  else assert.throws(()=>resourceSetup(board,m),/no SD/);
 }
});

test('1.85B requires its SD lookup pack while explaining that radar still operates without labels',()=>{
 const setup=resourceSetup('waveshare_185b',{version:'0.14.1',sdResources:{...labelResourcePack}});
 assert.equal(setup.required,true);
 assert.match(setup.title,/required for lookup names/);
 assert.match(setup.instructions,/\/dragon\/resources\/vendors\.ddr/);
 assert.match(setup.instructions,/\/dragon\/resources\/ble-labels\.ddb/);
 assert.match(setup.instructions,/Preserve existing files/);
 assert.match(setup.instructions,/restart after installation or replacement/);
 assert.match(setup.instructions,/core radar detections still run without it/);
 assert.match(setup.instructions,/lookup names unresolved/);
 assert.doesNotMatch(setup.instructions,/falls back to the internal registry/);
 const legacy=resourceSetup('waveshare_175',{version:'0.14.1',sdResources:resourcePack});
 assert.equal(legacy.required,false);
 assert.match(legacy.instructions,/falls back to the internal registry/);
});

test('SD label packs cannot cross board profiles, versions or safety contracts',()=>{
 for(const patch of [{required:false},{path:resourcePack.path},{path:'https://attacker.invalid/labels.zip'},
                     {path:'resources/../labels.zip'},{placement:'/dragon/resources/ble-labels.ddb'},{schema:2}])
  assert.throws(()=>resourceSetup('waveshare_185b',{version:'0.14.1',sdResources:{...labelResourcePack,...patch}}));
 for(const version of ['0.14.0','0.13.42','0.15.0','0.14.01'])
  assert.throws(()=>resourceSetup('waveshare_185b',{version,sdResources:labelResourcePack}));
 for(const board of ['watcher','waveshare_175','waveshare_175_gps','spotpear','xiao_round','lilygo_175','stopwatch'])
  assert.throws(()=>resourceSetup(board,{version:'0.14.1',sdResources:labelResourcePack}));
});
test('bad pack versions and unsafe links fail closed',()=>{
 for(const patch of [{schema:2},{required:'yes'},{path:'https://attacker.invalid/pack.zip'},{placement:'/config.txt'}])
  assert.throws(()=>resourceSetup('waveshare_175',{version:'0.13.28',sdResources:{...resourcePack,...patch}}));
 assert.throws(()=>resourceSetup('waveshare_175',{version:'0.13.27',sdResources:resourcePack}));
});
