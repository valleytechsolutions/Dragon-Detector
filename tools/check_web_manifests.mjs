// Exercise the real browser validator against generated release artifacts.
import {readFileSync} from 'node:fs';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {parseArgs} from 'node:util';
import {boardProfiles,validateManifest} from '../web/board-profiles.js';

const {values, positionals} = parseArgs({
  options: {'firmware-dir': {type: 'string', default: fileURLToPath(new URL('../web/firmware/', import.meta.url))}},
  allowPositionals: true,
});
const firmware = values['firmware-dir'];
const boards = positionals.length ? positionals : Object.keys(boardProfiles);
for (const board of boards) {
  const manifest = (file) => JSON.parse(readFileSync(join(firmware, board, file)));
  for (const mode of ['factory', 'update']) validateManifest(manifest(`${mode}.json`), board, mode);
  if (board === 'waveshare_p4_knob') validateManifest(manifest('update-app1.json'), board, 'update', 1);
  console.log(`PASS browser manifest validation: ${board}`);
}
