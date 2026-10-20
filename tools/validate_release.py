"""Validate current Markdown links and all generated flasher files before publication."""
from pathlib import Path
import hashlib,json,re
from flash_budget import RESERVE_PERCENT
from urllib.parse import unquote
ROOT=Path(__file__).resolve().parents[1]
VERSION=re.search(r'#define DRAGON_VERSION "([0-9.]+)"',(ROOT/'firmware/include/version.h').read_text()).group(1)
for md in [ROOT/'README.md',ROOT/'CREDITS.md',*ROOT.glob('docs/*.md')]:
    for target in re.findall(r'!?\[[^\]]*\]\(([^\s)]+)',md.read_text(encoding='utf-8')):
        if '://' in target or target.startswith('#'):continue
        file=unquote(target.split('#')[0]);assert (md.parent/file).exists(),f'{md.name}: missing {file}'
for board in ('spotpear','waveshare','stopwatch','xiao_round','waveshare_lite','waveshare_175','elecrow_21'):
    folder=ROOT/'web/firmware'/board
    records={name:sha for sha,name in (line.split() for line in (folder/'SHA256SUMS.txt').read_text().splitlines())}
    budget=json.loads((folder/'memory-budget.json').read_text(encoding='utf-8'))
    info=json.loads((folder/'build-info.json').read_text(encoding='utf-8'))
    assert info['version']==VERSION and info['board']==board and info['memory']==budget
    assert budget['app_bytes']==(folder/'firmware.bin').stat().st_size
    assert budget['reserve_percent']==RESERVE_PERCENT
    assert budget['app_slot_free_bytes']==budget['app_slot_bytes']-budget['app_bytes']
    assert budget['app_slot_free_bytes']>=(budget['app_slot_bytes']*RESERVE_PERCENT+99)//100
    assert budget['physical_flash_bytes']==info['flash_size']
    for mode in ('factory','update'):
        manifest=json.loads((folder/f'{mode}.json').read_text(encoding='utf-8'))
        assert manifest['version']==VERSION
        parts=manifest['builds'][0]['parts'];end=0
        for part in parts:
            file=folder/part['path'];assert file.parent==folder and file.exists()
            assert part['offset']>=end,'Overlapping segments'
            payload=file.read_bytes();assert hashlib.sha256(payload).hexdigest()==records[file.name]
            end=part['offset']+len(payload)
            assert end<=(0x2000000 if board=='waveshare' else 0x800000 if board=='xiao_round' else 0x1000000),'Image exceeds physical flash'
        offsets=[p['offset'] for p in parts]
        assert 0x9000 not in offsets,'NVS must not be explicitly written'
        assert len(parts)==((6 if board=='waveshare' else 5) if mode=='factory' else 4)
        if mode=='update':assert all(p['offset']<0xc10000 for p in parts),'Update must preserve both filesystems'
        if board=='waveshare' and mode=='factory':assert parts[-1]=={'path':'reconfs.bin','offset':0x1000000}
    print(f'PASS {board}: both manifests, checksums, image paths and nonoverlapping offsets')
print('PASS current documentation links')
