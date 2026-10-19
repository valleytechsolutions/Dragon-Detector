import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {hasCanCommanderTheme} from '../web/firmware-compat.js';
import {readRadarSettings,saveRadarSettings} from '../web/radar-transfer.js';

test('Can Commander requires its firmware while preserving the personal slot',async()=>{
  for(const v of ['0.13.17','0.13.9','0.13.018','0.12.99','garbage'])assert.equal(hasCanCommanderTheme(v),false);
  for(const v of ['0.13.18','0.13.19','0.13.100'])assert.equal(hasCanCommanderTheme(v),true);
  const catalog=readFileSync(new URL('../firmware/include/radar_theme_catalog.h',import.meta.url),'utf8');
  const names=JSON.parse('['+catalog.split('radarThemeCommands[]={')[1].split('};')[0]+']');
  assert.equal(names[34],'custom');assert.equal(names[35],'can-commander');
  let theme=34;
  const link={async write(command){if(command==='radar theme can-commander')theme=35;},async wait(){return `RADAR_SETTINGS theme=${theme} labels=1 recent=1 pages=1 plotted=0 primary=-1`;}};
  assert.equal((await readRadarSettings(link)).theme,'custom');
  await saveRadarSettings(link,{theme:'can-commander',labels:true,recent:true});
  assert.equal((await readRadarSettings(link)).theme,'can-commander');
});
