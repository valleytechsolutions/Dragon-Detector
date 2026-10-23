"""Validate and package the 8 MB M5Stack Dial factory image for M5Burner."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct

from esptool.bin_image import ESP32S3FirmwareImage

ROOT = Path(__file__).resolve().parents[1]
FLASH_BYTES = 0x800000
SEGMENTS = {'bootloader.bin': 0, 'partitions.bin': 0x8000,
            'boot_app0.bin': 0xe000, 'firmware.bin': 0x10000,
            'littlefs.bin': 0x610000}
PARTITIONS = [('nvs', 1, 2, 0x9000, 0x5000),
              ('otadata', 1, 0, 0xe000, 0x2000),
              ('app0', 0, 0x10, 0x10000, 0x300000),
              ('app1', 0, 0x11, 0x310000, 0x300000),
              ('spiffs', 1, 0x82, 0x610000, 0x1f0000)]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(source):
    info = json.loads((source / 'build-info.json').read_text())
    require(info['board'] == 'm5dial' and info['name'] in
            ('M5Stack Dial', 'M5Stack Dial Beta', 'M5Stack Dial v1.1'),
            'This package accepts only the M5Stack Dial profile')
    require(info['flash_size'] == FLASH_BYTES, 'Dial requires 8 MB flash')
    require(not info['source_dirty'], 'Use a clean release build')
    manifest = json.loads((source / 'factory.json').read_text())
    require(manifest['name'] == f"Dragon Detector — {info['name']} (factory)",
            'Manifest board name mismatch')
    require(manifest['version'] == info['version'], 'Manifest version mismatch')
    require(len(manifest['builds']) == 1, 'Expected one Dial build')
    build = manifest['builds'][0]
    require(build['chipFamily'] == 'ESP32-S3', 'Wrong chip family')
    require(build['parts'] == [{'path': n, 'offset': o} for n, o in SEGMENTS.items()],
            'Incorrect Dial factory segment layout')
    hashes = {n: h for h, n in (line.split() for line in
              (source / 'SHA256SUMS.txt').read_text().splitlines())}
    blobs = {n: (source / n).read_bytes() for n in [*SEGMENTS, 'factory-0x0.bin']}
    for name, data in blobs.items():
        require(hashlib.sha256(data).hexdigest() == hashes[name], f'Checksum mismatch: {name}')
    merged = blobs['factory-0x0.bin']
    require(len(merged) == FLASH_BYTES, 'Expected a complete 8 MB factory image')
    end = 0
    for name, offset in SEGMENTS.items():
        data = blobs[name]
        require(offset >= end and offset + len(data) <= FLASH_BYTES, f'Segment overlap: {name}')
        require(merged[end:offset] == b'\xff' * (offset-end), f'Non-erased gap before {name}')
        require(merged[offset:offset+len(data)] == data, f'Merged segment mismatch: {name}')
        end = offset + len(data)
    require(end == FLASH_BYTES, 'Missing filesystem tail')
    for index, expected in enumerate(PARTITIONS):
        magic, kind, subtype, offset, size, name, flags = struct.unpack_from(
            '<HBBII16sI', blobs['partitions.bin'], index*32)
        require(magic == 0x50aa and flags == 0 and
                (name.rstrip(b'\0').decode(), kind, subtype, offset, size) == expected,
                f'Incorrect partition: {expected[0]}')
    for name in ('bootloader.bin', 'firmware.bin'):
        image = ESP32S3FirmwareImage(io.BytesIO(blobs[name]))
        require(image.chip_id == 9 and image.checksum == image.calculate_checksum(),
                f'Invalid ESP32-S3 image: {name}')
        require(image.append_digest and image.stored_digest == image.calc_digest,
                f'Invalid image digest: {name}')
    require(merged[2:4] == bytes([2, 0x3f]), 'Expected DIO / 80 MHz / 8 MB boot header')
    require(b'M5Stack Dial' in blobs['firmware.bin'], 'Missing Dial application identity')
    require(merged[0x9000:0xe000] == b'\xff'*0x5000, 'NVS must be blank')
    require(merged[0x310000:0x610000] == b'\xff'*0x300000, 'Spare OTA slot must be blank')
    return info, hashes


def package(source, output):
    info, hashes = validate(source)
    output.mkdir(parents=True, exist_ok=True)
    filename = f"Dragon-Detector-{info['version']}-M5Stack-Dial-FACTORY-0x0.bin"
    target = output / filename
    if target.exists():
        require(hashlib.sha256(target.read_bytes()).hexdigest() == hashes['factory-0x0.bin'],
                'Refusing to replace a different existing package')
    shutil.copyfile(source / 'factory-0x0.bin', target)
    report = {'version': info['version'], 'device': info['name'],
              'source_commit': info['source_commit'], 'filename': filename,
              'bytes': FLASH_BYTES, 'sha256': hashes['factory-0x0.bin'],
              'chip': 'ESP32-S3', 'flash_offset': '0x0', 'flash_mode': 'DIO',
              'flash_frequency': '80MHz', 'segments_and_partitions_verified': True,
              'blank_nvs': True, 'blank_spare_ota': True,
              'hardware_validation': info['hardware_validation']}
    (output/'M5BURNER-VERIFICATION.json').write_text(json.dumps(report, indent=2)+'\n')
    (output/'M5BURNER-SHA256SUMS.txt').write_text(f"{report['sha256']}  {filename}\n")
    for name in ('FONT-LICENSE.txt', 'ASCII-FONT-NOTICE.txt'):
        shutil.copyfile(source/name, output/name)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'web/firmware/m5dial')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    package(args.source, args.output)
