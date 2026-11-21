import test from 'node:test';
import assert from 'node:assert/strict';
import {hasDagNaztyDarkTheme} from '../web/firmware-compat.js';

test('DagNazty Dark (theme 46) requires firmware 0.14.8',()=>{
  for(const version of ['0.13.41','0.14.7','0.14.8-preview','0.14.08',null,undefined,''])assert.equal(hasDagNaztyDarkTheme(version),false);
  for(const version of ['0.14.8','0.14.9','0.14.10'])assert.equal(hasDagNaztyDarkTheme(version),true);
});
