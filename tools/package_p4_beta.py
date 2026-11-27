"""Package the Waveshare P4 Knob 0.14.12 Beta 2 after its PlatformIO build."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BOARD = 'waveshare_p4_knob'
NAME = 'Waveshare ESP32-P4-Knob-Touch-AMOLED-1.75'
VERSION = '0.14.12'
TAG = 'v0.14.12-p4-beta.2'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True, help='The separate P4 PlatformIO core directory')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    def git(*command):
        return subprocess.check_output(['git', *command], cwd=ROOT, text=True).strip()

    if git('status', '--porcelain'):
        parser.error('Commit the complete P4 beta source before packaging')
    version_header = (ROOT/'firmware/include/dragon_version.h').read_text()
    if not re.search(r'^#define DRAGON_VERSION "' + re.escape(VERSION) + r'"$', version_header, re.M):
        parser.error('This package requires exactly Dragon Detector 0.14.12')
    build = ROOT/'.pio/build'/BOARD
    packages = args.core.resolve()/'packages'
    suffix = '.exe' if sys.platform == 'win32' else ''
    factory = {'bootloader.bin': 0x2000, 'partitions.bin': 0x8000, 'boot_app0.bin': 0xe000,
               'firmware.bin': 0x10000, 'littlefs.bin': 0xc10000}
    sources = {name: build/name for name in ('bootloader.bin', 'partitions.bin', 'firmware.bin')}
    sources['boot_app0.bin'] = packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    sizes = {'bootloader.bin': 0x6000, 'partitions.bin': 0x1000, 'boot_app0.bin': 0x2000, 'firmware.bin': 0x600000}
    data = {name: source.read_bytes() for name, source in sources.items()}
    for name, blob in data.items():
        if not 0 < len(blob) <= sizes[name]:
            raise ValueError(f'{name} does not fit its flash region')
        if name in ('bootloader.bin', 'firmware.bin') and (blob[0] != 0xe9 or int.from_bytes(blob[12:14], 'little') != 18
                or int.from_bytes(blob[15:17], 'little') != 300 or int.from_bytes(blob[17:19], 'little') != 399):
            raise ValueError(f'{name} is not an ESP32-P4 revision 3 image')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    subprocess.run([str(packages/'tool-mklittlefs'/('mklittlefs'+suffix)), '-c', str(ROOT/'firmware/data'),
                    '-b', '4096', '-p', '256', '-s', str(0x11f0000), str(out/'littlefs.bin')], check=True)
    data['littlefs.bin'] = (out/'littlefs.bin').read_bytes()
    if len(data['littlefs.bin']) != 0x11f0000:
        raise ValueError('Factory filesystem size mismatch')
    workspace_markers = (b'C:/Users/', b'C:\\Users\\')
    for name, blob in data.items():
        if any(marker in blob for marker in workspace_markers):
            raise ValueError(f'Private workstation path in {name}')
        (out/name).write_bytes(blob)
    for key, offsets in (('factory', factory), ('update', {'firmware.bin': 0x10000}),
                         ('update-app1', {'firmware.bin': 0x610000})):
        mode = 'factory' if key == 'factory' else 'update'
        manifest = {'name': f'Dragon Detector — {NAME} ({mode})', 'version': VERSION,
                    'new_install_prompt_erase': mode == 'factory', 'new_install_improv_wait_time': 0,
                    'builds': [{'chipFamily': 'ESP32-P4', 'parts': [{'path': name, 'offset': offset} for name, offset in offsets.items()]}],
                    'release_channel': 'beta', 'beta_iteration': 2, 'application_sha256': sha(data['firmware.bin'])}
        (out/f'{key}.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    info = {'board': BOARD, 'version': VERSION, 'release_tag': TAG, 'name': NAME+' · Beta 2',
            'flash_size': 0x2000000, 'source_commit': git('rev-parse', 'HEAD'), 'source_dirty': False}
    (out/'build-info.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
    for name, path in (('LICENSE', ROOT/'LICENSE'), ('CREDITS.md', ROOT/'CREDITS.md'),
                       ('ESP32DualBandWardriver-LICENSE.txt', ROOT/'docs/licenses/ESP32DualBandWardriver-LICENSE.txt'),
                       ('BOARD-STATUS.md', ROOT/'docs/WAVESHARE-P4-KNOB-175.md')):
        (out/name).write_bytes(path.read_bytes())
    helix = ROOT/'managed_components/chmorgan__esp-libhelix-mp3'
    if not (helix/'libhelix-mp3/RPSL.txt').is_file():
        raise ValueError('Missing Helix source or license from the pinned component')
    with zipfile.ZipFile(out/'helix-mp3-source.zip', 'x', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(helix.rglob('*')):
            relative = path.relative_to(helix)
            if path.is_file() and not any(part.startswith('.') for part in relative.parts):
                archive.write(path, relative.as_posix())
    (out/'SHA256SUMS.txt').write_text(''.join(f'{sha(data[name])}  {name}\n' for name in sorted(data)), encoding='ascii')
    bundle = out.parent/f'dragon-detector-{BOARD}-{VERSION}.zip'
    with zipfile.ZipFile(bundle, 'x', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(out.iterdir()):
            archive.write(path, path.name)
    with zipfile.ZipFile(bundle) as archive:
        if archive.testzip() is not None:
            raise ValueError('P4 beta ZIP failed readback')
    with (out.parent/'SHA256SUMS.txt').open('x', encoding='ascii') as index:
        index.write(f'{sha(bundle.read_bytes())}  {bundle.name}\n')
    print(f'{bundle.name}: {bundle.stat().st_size} bytes, application {sha(data["firmware.bin"])}')


if __name__ == '__main__':
    main()
