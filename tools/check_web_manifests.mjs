// Exercise the real browser validator against generated release artifacts.
import {readFileSync} from 'node:fs';
import {boardProfiles,validateManifest} from '../web/board-profiles.js';
for(const board of process.argv.length>2?process.argv.slice(2):Object.keys(boardProfiles)){
  for(const mode of ['factory','update'])validateManifest(JSON.parse(readFileSync(new URL(`../web/firmware/${board}/${mode}.json`,import.meta.url))),board,mode);
  console.log(`PASS browser manifest validation: ${board}`);
}
