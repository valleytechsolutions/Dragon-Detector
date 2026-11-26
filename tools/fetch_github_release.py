"""Stage the exact stable board packages published by a tagged GitHub release."""
from io import BytesIO
from pathlib import Path
import hashlib
import json
import re
import shutil
import ssl
import urllib.error
import urllib.request
import zipfile

from build_release import BOARDS
from fetch_p4_knob_carryover import contexts


STABLE_BOARDS = tuple(board for board in BOARDS if not board.endswith('_beta'))
RELEASES = 'https://github.com/valleytechsolutions/Dragon-Detector/releases/download'
MAX_ASSET_BYTES = 25 * 1024 * 1024  # Cloudflare Pages also serves the original ZIP.
MAX_IMAGE_BYTES = 32 * 1024 * 1024  # Largest stable board's physical flash.
FILENAME = re.compile(r'[A-Za-z0-9][A-Za-z0-9._-]*[A-Za-z0-9_-]')
WINDOWS_DEVICES = {'CON', 'PRN', 'AUX', 'NUL'} | {
    f'{device}{number}' for device in ('COM', 'LPT') for number in range(1, 10)
}


def fetch(url, limit, tls_contexts):
    request = urllib.request.Request(url, headers={'User-Agent': 'dragon-detector-release-import'})
    for index, context in enumerate(tls_contexts):
        try:
            with urllib.request.urlopen(request, timeout=60, context=context) as response:
                data = response.read(limit + 1)
            break
        except urllib.error.URLError as error:
            if index == len(tls_contexts) - 1 or not isinstance(error.reason, ssl.SSLCertVerificationError):
                raise ValueError(f'Could not download {url}: {error}') from error
    if len(data) > limit:
        raise ValueError(f'Release asset exceeds its size limit: {url}')
    return data


def read_checksums(data, label):
    checksums = {}
    for line in data.decode('ascii').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (\S+)', line)
        if not match:
            raise ValueError(f'Invalid SHA-256 entry in {label}: {line!r}')
        digest, filename = match.groups()
        if filename in checksums:
            raise ValueError(f'Duplicate checksum in {label}: {filename}')
        checksums[filename] = digest
    return checksums


def check_digest(data, expected, label):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f'SHA-256 mismatch for {label}')


def archive_members(archive, label):
    members = archive.infolist()
    names = [member.orig_filename for member in members]
    if len({name.casefold() for name in names}) != len(names):
        raise ValueError(f'Duplicate ZIP entry in {label}')
    if any(not FILENAME.fullmatch(name) or name.lower().endswith('.zip')
           or name.split('.', 1)[0].upper() in WINDOWS_DEVICES for name in names):
        raise ValueError(f'Expected flat board files in {label}')
    if any(member.file_size > MAX_IMAGE_BYTES for member in members):
        raise ValueError(f'Oversized ZIP entry in {label}')
    return set(names)


def check_manifest(archive, board, mode, version, checksums):
    filename = f'{mode}.json'
    manifest = json.loads(archive.read(filename))
    name = f'Dragon Detector — {BOARDS[board][0]} ({mode})'
    if manifest.get('version') != version or manifest.get('name') != name:
        raise ValueError(f'{board}/{filename} does not identify {board} at {version}')
    builds = manifest.get('builds', [])
    if len(builds) != 1 or builds[0].get('chipFamily') != 'ESP32-S3':
        raise ValueError(f'Invalid chip family in {board}/{filename}')
    parts = [part.get('path') for part in builds[0].get('parts', [])]
    if 'firmware.bin' not in parts or any(part not in checksums for part in parts):
        raise ValueError(f'Unverified binary in {board}/{filename}')


def stage_board(data, digest, asset, board, version, destination):
    check_digest(data, digest, asset)
    with zipfile.ZipFile(BytesIO(data)) as archive:
        entries = archive_members(archive, asset)
        required = {'SHA256SUMS.txt', 'build-info.json', 'memory-budget.json', 'factory.json', 'update.json'}
        if not required <= entries:
            raise ValueError(f'Missing board files in {asset}: {sorted(required - entries)}')
        info = json.loads(archive.read('build-info.json'))
        if info.get('board') != board or info.get('version') != version:
            raise ValueError(f'{asset} does not identify {board} at {version}')

        sum_bytes = archive.read('SHA256SUMS.txt')
        checksums = read_checksums(sum_bytes, f'{asset}/SHA256SUMS.txt')
        if checksums.keys() != {name for name in entries if name.lower().endswith('.bin')}:
            raise ValueError(f'Binary checksum list does not match the files in {asset}')
        for mode in ('factory', 'update'):
            check_manifest(archive, board, mode, version, checksums)
        for name, expected in checksums.items():
            check_digest(archive.read(name), expected, f'{asset}/{name}')

        folder = destination / board
        folder.mkdir()
        for member in archive.infolist():
            with archive.open(member) as source, (folder / member.filename).open('wb') as target:
                shutil.copyfileobj(source, target)

    bundle_name = f'dragon-detector-{board}.zip'
    (folder / bundle_name).write_bytes(data)
    with (folder / 'SHA256SUMS.txt').open('ab') as output:
        if not sum_bytes.endswith(b'\n'):
            output.write(b'\n')
        output.write(f'{digest}  {bundle_name}\n'.encode('ascii'))


def stage_release(version: str, destination: Path):
    """Fill an empty staging directory with verified ZIPs and their board files."""
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
        raise ValueError(f'Invalid release version: {version}')
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise ValueError(f'Release staging directory is not empty: {destination}')

    base = f'{RELEASES}/v{version}'
    tls_contexts = contexts()
    assets = {f'dragon-detector-{board}-{version}.zip': board for board in STABLE_BOARDS}
    checksums = read_checksums(fetch(f'{base}/SHA256SUMS.txt', 16 * 1024, tls_contexts), f'v{version}/SHA256SUMS.txt')
    if checksums.keys() != assets.keys():
        missing = sorted(assets.keys() - checksums.keys())
        unexpected = sorted(checksums.keys() - assets.keys())
        raise ValueError(f'Wrong asset list in v{version}/SHA256SUMS.txt (missing: {missing}; unexpected: {unexpected})')

    for asset, board in assets.items():
        data = fetch(f'{base}/{asset}', MAX_ASSET_BYTES, tls_contexts)
        stage_board(data, checksums[asset], asset, board, version, destination)
        print(f'Verified GitHub release v{version}: {board}')
