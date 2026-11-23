"""Copy the Waveshare P4 Knob 0.14.8 package from the live web flasher into web/firmware.

The P4 Knob's firmware is not built from this repository. Until its source is
added, every web flasher upload carries the published 0.14.8 package over
unchanged, so the board stays installable. Run this before
tools/build_static_site.py. Each file is checked against the package's
SHA256SUMS.txt, whose own SHA-256 is pinned below; a mismatch stops with an
error and leaves nothing half-written. Files already present and correct are
not downloaded again.
"""
import hashlib
import importlib
import pathlib
import ssl
import sys
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
BOARD = 'waveshare_p4_knob'
SOURCE = 'https://valleytech-dragon-detector.pages.dev/firmware/' + BOARD + '/'
TARGET = ROOT / 'web' / 'firmware' / BOARD
# SHA-256 of the published 0.14.8 SHA256SUMS.txt (November 23, 2026).
SUMS_SHA256 = '1caeb414b75221509866735ba7a30201bf627ed0912ac1e26ac77d70d151353f'


def contexts():
    """System certificates first, then certifi's list (PlatformIO's Python
    carries it), as tools/deploy_web_flasher.py does."""
    found = [ssl.create_default_context()]
    for module in ('certifi', 'pip._vendor.certifi'):
        try:
            found.append(ssl.create_default_context(cafile=importlib.import_module(module).where()))
            break
        except Exception:
            continue
    return found


def fetch(name):
    request = urllib.request.Request(SOURCE + name + '?carryover=%d' % time.time_ns(),
                                     headers={'Cache-Control': 'no-cache', 'User-Agent': 'dragon-detector-p4-carryover'})
    options = contexts()
    for number, context in enumerate(options):
        try:
            with urllib.request.urlopen(request, timeout=60, context=context) as response:
                return response.read()
        except urllib.error.URLError as error:
            if number == len(options) - 1 or not isinstance(getattr(error, 'reason', None), ssl.SSLCertVerificationError):
                raise


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    local_sums = TARGET / 'SHA256SUMS.txt'
    sums = local_sums.read_bytes() if local_sums.is_file() else b''
    if sha256(sums) != SUMS_SHA256:
        sums = fetch('SHA256SUMS.txt')
    if sha256(sums) != SUMS_SHA256:
        sys.exit('P4 Knob SHA256SUMS.txt does not match the pinned 0.14.8 package; nothing was copied.')
    expected = {name: digest for digest, name in (line.split() for line in sums.decode().splitlines() if line.strip())}
    files = {}
    for name, digest in sorted(expected.items()):
        path = TARGET / name
        data = path.read_bytes() if path.is_file() else b''
        if sha256(data) != digest:
            data = fetch(name)
        if sha256(data) != digest:
            sys.exit(f'P4 Knob {name} does not match SHA256SUMS.txt; nothing was copied.')
        files[name] = data
    TARGET.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (TARGET / name).write_bytes(data)
    local_sums.write_bytes(sums)
    print(f'P4 Knob 0.14.8 carry-over ready in web/firmware/{BOARD}: {len(files)} files plus SHA256SUMS.txt verified.')


if __name__ == '__main__':
    main()
