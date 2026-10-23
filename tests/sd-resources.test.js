import test from 'node:test';
import assert from 'node:assert/strict';
import {boardProfiles} from '../web/board-profiles.js';
import {resourceSetup,resourcePack,hasSd,hasGpsAndSd} from '../web/sd-resources.js';
test('all exact board profiles gate SD resources and GPS independently',()=>{
 for(const board of [...Object.keys(boardProfiles),'t5_e1','unknown']){
  assert.equal(hasGpsAndSd(board),board==='waveshare_175_gps');
  assert.equal(resourceSetup(board,{version:'0.13.26'}),null);
  const m={version:'0.13.28',sdResources:{...resourcePack}};
  if(hasSd(board)){assert.match(resourceSetup(board,m).title,/Optional/);m.sdResources.required=true;assert.match(resourceSetup(board,m).title,/REQUIRED/);}
  else assert.throws(()=>resourceSetup(board,m),/no SD/);
 }
});
test('bad pack versions and unsafe links fail closed',()=>{
 for(const patch of [{schema:2},{required:'yes'},{path:'https://attacker.invalid/pack.zip'},{placement:'/config.txt'}])
  assert.throws(()=>resourceSetup('waveshare_175',{version:'0.13.28',sdResources:{...resourcePack,...patch}}));
 assert.throws(()=>resourceSetup('waveshare_175',{version:'0.13.27',sdResources:resourcePack}));
});
