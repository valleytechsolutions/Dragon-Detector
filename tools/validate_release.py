"""Validate current Markdown links and all generated flasher files before publication."""
from pathlib import Path
import hashlib,json,re
from urllib.parse import unquote
ROOT=Path(__file__).resolve().parents[1]
for md in [ROOT/'README.md',ROOT/'CREDITS.md',*ROOT.glob('docs/*.md')]:
    for target in re.findall(r'!?\[[^\]]*\]\(([^\s)]+)',md.read_text(encoding='utf-8')):
        if '://' in target or target.startswith('#'):continue
        file=unquote(target.split('#')[0]);assert (md.parent/file).exists(),f'{md.name}: missing {file}'
for board in ('spotpear','waveshare','stopwatch'):
    folder=ROOT/'web/firmware'/board
    records={name:sha for sha,name in (line.split() for line in (folder/'SHA256SUMS.txt').read_text().splitlines())}
    for mode in ('factory','update'):
        manifest=json.loads((folder/f'{mode}.json').read_text(encoding='utf-8'))
        assert manifest['version']=='0.5.0'
        parts=manifest['builds'][0]['parts'];end=0
        for part in parts:
            file=folder/part['path'];assert file.parent==folder and file.exists()
            assert part['offset']>=end,'Overlapping segments'
            payload=file.read_bytes();assert hashlib.sha256(payload).hexdigest()==records[file.name]
            end=part['offset']+len(payload)
        offsets=[p['offset'] for p in parts]
        assert 0x9000 not in offsets,'NVS must not be explicitly written'
        assert len(parts)==(5 if mode=='factory' else 4)
    print(f'PASS {board}: both manifests, checksums, image paths and nonoverlapping offsets')
print('PASS current documentation links')
