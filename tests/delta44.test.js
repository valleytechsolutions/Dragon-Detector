import test from 'node:test';
import assert from 'node:assert/strict';
import {hasDelta44Theme} from '../web/firmware-compat.js';

test('44Delta selection requires released firmware with persistent ID 45',()=>{
  for(const version of ['0.13.38','0.13.40','0.13.41-preview','0.13.041',null,undefined,''])assert.equal(hasDelta44Theme(version),false);
  for(const version of ['0.13.41','0.13.42','0.13.100'])assert.equal(hasDelta44Theme(version),true);
});
