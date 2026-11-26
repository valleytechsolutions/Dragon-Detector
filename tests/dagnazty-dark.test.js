import test from 'node:test';
import assert from 'node:assert/strict';
import {hasDagNaztyDarkTheme} from '../web/firmware-compat.js';

// The published 0.14.8 is the node release and has no DagNazty Dark or digital
// themes; they first ship in 0.14.9 with the combined release.
test('DagNazty Dark and digital themes (46-48) require firmware 0.14.9',()=>{
  for(const version of ['0.13.41','0.14.7','0.14.8','0.14.9-preview','0.14.09',null,undefined,''])assert.equal(hasDagNaztyDarkTheme(version),false);
  for(const version of ['0.14.9','0.14.10','0.14.11'])assert.equal(hasDagNaztyDarkTheme(version),true);
});
