"""Vendor the pinned, prebundled ESP Web Tools browser distribution."""
import base64
import hashlib
import io
import json
from pathlib import Path
import tarfile
from urllib.request import urlopen

VERSION = '10.4.0'
ROOT = Path(__file__).resolve().parents[1]


def main():
    metadata_url = f'https://registry.npmjs.org/esp-web-tools/{VERSION}'
    metadata = json.load(urlopen(metadata_url, timeout=30))
    archive_url = f'https://registry.npmjs.org/esp-web-tools/-/esp-web-tools-{VERSION}.tgz'
    archive = urlopen(archive_url, timeout=30).read()
    integrity = 'sha512-' + base64.b64encode(hashlib.sha512(archive).digest()).decode()
    if integrity != metadata['dist']['integrity']:
        raise ValueError('Registry integrity check failed')
    destination = ROOT / 'web' / 'vendor' / f'esp-web-tools-{VERSION}'
    destination.mkdir(parents=True, exist_ok=True)
    hashes = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as package:
        for member in package.getmembers():
            path = Path(member.name)
            browser_file = path.parent.as_posix() == 'package/dist/web' and path.suffix == '.js'
            if member.isfile() and (browser_file or member.name == 'package/LICENSE'):
                data = package.extractfile(member).read()
                (destination / path.name).write_bytes(data)
                hashes[path.name] = hashlib.sha256(data).hexdigest()
    (destination / 'UPSTREAM.json').write_text(json.dumps({
        'package': 'esp-web-tools', 'version': VERSION,
        'source': 'https://github.com/esphome/esp-web-tools',
        'archive': archive_url, 'integrity': integrity,
        'license': metadata['license'], 'files_sha256': hashes,
        'modifications': 'None. Prebundled dist/web files and LICENSE copied byte-for-byte.'
    }, indent=2) + '\n', encoding='utf-8')
    print(f'Vendored {len(hashes)} verified files to {destination}')


if __name__ == '__main__':
    main()
