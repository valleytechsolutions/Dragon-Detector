"""Validate and package only the StopWatch C152 merged image for M5Burner."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil

from esptool.bin_image import ESP32S3FirmwareImage

ROOT = Path(__file__).resolve().parents[1]
FLASH_BYTES = 0x1000000
SEGMENTS = {
    'bootloader.bin': 0,
    'partitions.bin': 0x8000,
    'boot_app0.bin': 0xe000,
    'firmware.bin': 0x10000,
    'littlefs.bin': 0x810000,
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(source):
    info = json.loads((source / 'build-info.json').read_text())
    require(info['board'] == 'stopwatch' and info['name'] == 'M5Stack StopWatch C152',
            'M5Burner package must be the StopWatch C152 profile')
    require(info['flash_size'] == FLASH_BYTES, 'C152 requires a 16 MB image')
    require(not info['source_dirty'], 'Use firmware from a clean release build')
    manifest = json.loads((source / 'factory.json').read_text())
    require(manifest['version'] == info['version'], 'Manifest version mismatch')
    require(len(manifest['builds']) == 1, 'Expected one C152 build')
    build = manifest['builds'][0]
    require(build['chipFamily'] == 'ESP32-S3', 'Wrong chip family')
    require(build['parts'] == [{'path': n, 'offset': o} for n, o in SEGMENTS.items()],
            'Incorrect StopWatch factory segment layout')
    hashes = {n: h for h, n in (line.split() for line in
              (source / 'SHA256SUMS.txt').read_text().splitlines())}
    blobs = {name: (source / name).read_bytes() for name in
             [*SEGMENTS, 'factory-0x0.bin']}
    for name, data in blobs.items():
        require(hashlib.sha256(data).hexdigest() == hashes[name], f'Checksum mismatch: {name}')
    merged = blobs['factory-0x0.bin']
    require(len(merged) == FLASH_BYTES, 'Factory image must cover the complete 16 MB flash')
    end = 0
    for name, offset in SEGMENTS.items():
        data = blobs[name]
        require(offset >= end and offset + len(data) <= FLASH_BYTES, f'Segment overlap: {name}')
        require(merged[end:offset] == b'\xff' * (offset - end), f'Non-erased gap before {name}')
        require(merged[offset:offset + len(data)] == data, f'Merged segment mismatch: {name}')
        end = offset + len(data)
    require(end == FLASH_BYTES, 'Missing filesystem tail')
    for name in ('bootloader.bin', 'firmware.bin'):
        image = ESP32S3FirmwareImage(io.BytesIO(blobs[name]))
        require(image.chip_id == 9, f'Wrong ESP32-S3 chip ID: {name}')
        require(image.checksum == image.calculate_checksum(), f'Invalid image checksum: {name}')
        require(image.append_digest and image.stored_digest == image.calc_digest,
                f'Invalid image digest: {name}')
    require(merged[2:4] == bytes([2, 0x4f]), 'Expected DIO / 80 MHz / 16 MB boot header')
    require(b'M5Stack StopWatch C152' in blobs['firmware.bin'], 'Missing C152 application identity')
    require(merged[0x9000:0xe000] == b'\xff' * 0x5000, 'NVS must be blank')
    require(merged[0x410000:0x810000] == b'\xff' * 0x400000, 'Spare OTA slot must be blank')
    return info, hashes


def package(source, output):
    info, hashes = validate(source)
    output.mkdir(parents=True, exist_ok=True)
    filename = f"Dragon-Detector-{info['version']}-M5Stack-StopWatch-C152-FACTORY-0x0.bin"
    target = output / filename
    if target.exists():
        require(hashlib.sha256(target.read_bytes()).hexdigest() == hashes['factory-0x0.bin'],
                'Refusing to replace a different existing package')
    shutil.copyfile(source / 'factory-0x0.bin', target)
    report = {
        'version': info['version'], 'device': info['name'], 'source_commit': info['source_commit'],
        'filename': filename, 'bytes': FLASH_BYTES, 'sha256': hashes['factory-0x0.bin'],
        'chip': 'ESP32-S3', 'flash_offset': '0x0', 'flash_mode': 'DIO', 'flash_frequency': '80MHz',
        'segments_verified': True, 'bootloader_and_app_checksums_valid': True,
        'bootloader_and_app_digests_valid': True, 'blank_nvs': True, 'blank_spare_ota': True,
        'hardware_validation': 'Complete M5Burner factory installation requires a physical C152 test.',
    }
    (output / 'M5BURNER-VERIFICATION.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    (output / 'M5BURNER-SHA256SUMS.txt').write_text(f"{report['sha256']}  {filename}\n", encoding='ascii')
    for name in ('FONT-LICENSE.txt', 'ASCII-FONT-NOTICE.txt'):
        shutil.copyfile(source / name, output / name)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'web/firmware/stopwatch')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    package(args.source, args.output)
