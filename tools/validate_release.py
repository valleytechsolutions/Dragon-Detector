"""Validate current Markdown links and all generated flasher files before publication."""
from pathlib import Path
import hashlib,json,re,zipfile
from flash_budget import RESERVE_PERCENT
from urllib.parse import unquote
ROOT=Path(__file__).resolve().parents[1]
VERSION=re.search(r'#define DRAGON_VERSION "([0-9.]+)"',(ROOT/'firmware/include/dragon_version.h').read_text()).group(1)
for md in [ROOT/'README.md',ROOT/'CREDITS.md',*ROOT.glob('docs/*.md')]:
    for target in re.findall(r'!?\[[^\]]*\]\(([^\s)]+)',md.read_text(encoding='utf-8')):
        if '://' in target or target.startswith('#'):continue
        file=unquote(target.split('#')[0]);assert (md.parent/file).exists(),f'{md.name}: missing {file}'
boards=['waveshare_185b','watcher','m5dial','spotpear','waveshare','stopwatch','xiao_round','waveshare_lite','waveshare_175','waveshare_175_gps','elecrow_21','lilygo_175']
# The separately requested beta is not part of the default stable build set.
if (ROOT/'web/firmware/guition_185_beta').exists():boards.append('guition_185_beta')
for board in boards:
    folder=ROOT/'web/firmware'/board
    records={name:sha for sha,name in (line.split() for line in (folder/'SHA256SUMS.txt').read_text().splitlines())}
    budget=json.loads((folder/'memory-budget.json').read_text(encoding='utf-8'))
    info=json.loads((folder/'build-info.json').read_text(encoding='utf-8'))
    assert re.fullmatch(r'\d+\.\d+\.\d+',info['version']) and info['board']==board and info['memory']==budget
    assert tuple(map(int,info['version'].split('.')))<=tuple(map(int,VERSION.split('.')))
    assert budget['app_bytes']==(folder/'firmware.bin').stat().st_size
    assert budget['reserve_percent']==RESERVE_PERCENT
    assert budget['app_slot_free_bytes']==budget['app_slot_bytes']-budget['app_bytes']
    assert budget['app_slot_free_bytes']>=(budget['app_slot_bytes']*RESERVE_PERCENT+99)//100
    assert budget['physical_flash_bytes']==info['flash_size']
    if board=='guition_185_beta':
        assert info['release_channel']=='beta' and info['beta_iteration']==1
        assert info['hardware_validation']=='physically-unverified'
        assert info['display_commands']==184 and info['display_qspi_hz']==20000000
        assert info['display_id_autodetection'] is False
        assert info['filesystem_offset']==0xc10000 and info['flash_size']==0x1000000
        with zipfile.ZipFile(folder/f'dragon-detector-{board}.zip') as archive:
            for name in ('SOURCE.md','NOTICE','LICENSE-APACHE-2.0.txt','LICENSE-MIT.txt','SHA256SUMS.txt'):
                expected=(ROOT/'third_party/guition-jc3636w518'/name).read_bytes()
                packaged='DISPLAY-'+name
                assert (folder/packaged).read_bytes()==expected,f'Altered beta notice: {packaged}'
                assert archive.read(packaged)==expected,f'Missing beta ZIP notice: {packaged}'
    if board=='waveshare_185b':
        with zipfile.ZipFile(folder/f'dragon-detector-{board}.zip') as archive:
            for source,prefix,names in ((ROOT/'third_party/waveshare-185b','DISPLAY-',('NOTICE.md','LICENSE')),(ROOT/'lib/es8311','ES8311-',('UPSTREAM.md','LICENSE'))):
                for name in names:
                    expected=(source/name).read_bytes();packaged=prefix+name
                    assert (folder/packaged).read_bytes()==expected,f'Altered or missing notice: {packaged}'
                    assert archive.read(packaged)==expected,f'Notice missing from board ZIP: {packaged}'
    for mode in ('factory','update'):
        manifest=json.loads((folder/f'{mode}.json').read_text(encoding='utf-8'))
        assert manifest['version']==info['version']
        if board=='guition_185_beta':
            assert manifest['release_channel']=='beta' and manifest['beta_iteration']==1
            assert manifest['hardware_validation']=='physically-unverified'
        parts=manifest['builds'][0]['parts'];end=0
        for part in parts:
            file=folder/part['path'];assert file.parent==folder and file.exists()
            assert part['offset']>=end,'Overlapping segments'
            payload=file.read_bytes();assert hashlib.sha256(payload).hexdigest()==records[file.name]
            end=part['offset']+len(payload)
            assert end<=(0x2000000 if board in ('watcher','waveshare') else 0x800000 if board in ('m5dial','xiao_round') else 0x1000000),'Image exceeds physical flash'
        offsets=[p['offset'] for p in parts]
        assert 0x9000 not in offsets,'NVS must not be explicitly written'
        assert len(parts)==((6 if board=='waveshare' else 5) if mode=='factory' else 4)
        if mode=='update':assert all(p['offset']<0xc10000 for p in parts),'Update must preserve both filesystems'
        if board=='waveshare' and mode=='factory':assert parts[-1]=={'path':'reconfs.bin','offset':0x1000000}
    print(f'PASS {board}: both manifests, checksums, image paths and nonoverlapping offsets')
print('PASS current documentation links')
