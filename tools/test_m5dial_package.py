"""Reject mismatched boards, saved device data and corrupt M5Burner packages."""
import hashlib
import json
from pathlib import Path
import unittest

from package_m5dial_m5burner import validate

ROOT = Path(__file__).resolve().parents[1]


class Files:
    def __init__(self, data, name=''):
        self.data, self.name = data, name

    def __truediv__(self, name):
        return Files(self.data, name)

    def read_bytes(self):
        return self.data[self.name]

    def read_text(self):
        return self.read_bytes().decode()


class PackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = ROOT/'web/firmware/m5dial'
        names = ['build-info.json', 'factory.json', 'SHA256SUMS.txt',
                 'bootloader.bin', 'boot_app0.bin', 'partitions.bin',
                 'firmware.bin', 'littlefs.bin', 'factory-0x0.bin']
        cls.original = {name: (source/name).read_bytes() for name in names}

    def setUp(self):
        self.data = dict(self.original)
        self.files = Files(self.data)

    def metadata(self, **changes):
        info = json.loads(self.data['build-info.json'])
        info.update(changes)
        self.data['build-info.json'] = json.dumps(info).encode()

    def rehash(self):
        self.data['SHA256SUMS.txt'] = ''.join(
            f'{hashlib.sha256(data).hexdigest()}  {name}\n'
            for name, data in self.data.items() if name.endswith('.bin')).encode()

    def test_release_accepted(self):
        self.assertEqual(validate(self.files)[0]['board'], 'm5dial')

    def test_stopwatch_rejected(self):
        self.metadata(board='stopwatch', name='M5Stack StopWatch C152')
        with self.assertRaisesRegex(ValueError, 'only the M5Stack Dial'):
            validate(self.files)

    def test_wrong_flash_size_rejected(self):
        self.metadata(flash_size=0x1000000)
        with self.assertRaisesRegex(ValueError, '8 MB'):
            validate(self.files)

    def test_wrong_manifest_name_rejected(self):
        manifest = json.loads(self.data['factory.json'])
        manifest['name'] = 'Dragon Detector — M5Stack StopWatch C152 (factory)'
        self.data['factory.json'] = json.dumps(manifest).encode()
        with self.assertRaisesRegex(ValueError, 'Manifest board name'):
            validate(self.files)

    def test_dirty_build_rejected(self):
        self.metadata(source_dirty=True)
        with self.assertRaisesRegex(ValueError, 'clean release'):
            validate(self.files)

    def test_corruption_rejected(self):
        self.data['firmware.bin'] += b'changed'
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            validate(self.files)

    def test_saved_nvs_rejected_even_with_new_checksum(self):
        data = bytearray(self.data['factory-0x0.bin'])
        data[0x9000] = 0
        self.data['factory-0x0.bin'] = bytes(data)
        self.rehash()
        with self.assertRaisesRegex(ValueError, 'Non-erased gap'):
            validate(self.files)

    def test_wrong_partition_rejected_even_with_matching_segments(self):
        table = bytearray(self.data['partitions.bin'])
        table[2*32+4] ^= 1  # Shift app0 by one byte.
        merged = bytearray(self.data['factory-0x0.bin'])
        merged[0x8000:0x8000+len(table)] = table
        self.data['partitions.bin'], self.data['factory-0x0.bin'] = bytes(table), bytes(merged)
        self.rehash()
        with self.assertRaisesRegex(ValueError, 'Incorrect partition: app0'):
            validate(self.files)


if __name__ == '__main__':
    unittest.main()
