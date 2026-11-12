"""Check packaging limits, rounding and the shared firmware policy."""
from pathlib import Path
import csv
import re
import unittest
from flash_budget import RESERVE_PERCENT, report

class FlashBudgetTests(unittest.TestCase):
    def budget(self, size, used):
        entries={'app0':['app0','app','ota_0','0x10000',hex(size)],
                 'app1':['app1','app','ota_1',hex(0x10000+size),hex(size)]}
        return report(entries,used,size*2+0x10000)

    def test_boundary(self):
        self.assertEqual(self.budget(100,95)['app_slot_free_bytes'],5)
        with self.assertRaises(AssertionError):self.budget(100,96)
        self.assertEqual(self.budget(101,95)['app_slot_free_bytes'],6)
        with self.assertRaises(AssertionError):self.budget(101,96)

    def test_all_release_slot_sizes(self):
        for size in (0x300000,0x400000,0x600000):
            limit=size-(size*RESERVE_PERCENT+99)//100
            self.budget(size,limit)
            with self.assertRaises(AssertionError):self.budget(size,limit+1)

    def test_single_application_slot(self):
        # Waveshare 1.43C (8 MB): one application slot, then the filesystem.
        entries={'app0':['app0','app','ota_0','0x10000','0x400000'],
                 'spiffs':['spiffs','data','spiffs','0x410000','0x3f0000']}
        limit=0x400000-(0x400000*RESERVE_PERCENT+99)//100
        budget=report(entries,limit,0x800000)
        self.assertEqual(budget['ota_slot_bytes'],0)
        self.assertEqual(budget['app_slot_bytes'],0x400000)
        self.assertEqual(budget['physical_flash_bytes'],0x800000)
        self.assertEqual(budget['filesystem_partitions'],[{'name':'spiffs','offset':0x410000,'bytes':0x3f0000}])
        with self.assertRaises(AssertionError):report(entries,limit+1,0x800000)

    def test_every_release_partition_table(self):
        # Same row parsing as build_release.py; a table without app1 must still report.
        tables=sorted((Path(__file__).resolve().parents[1]/'hardware').glob('*/partitions.csv'))
        self.assertGreaterEqual(len(tables),12)
        for table in tables:
            entries={row[0].strip():row for row in csv.reader(table.read_text().splitlines()) if row and not row[0].startswith('#')}
            budget=report(entries,1,0x800000)
            self.assertEqual(budget['ota_slot_bytes'],int(entries['app1'][4],0) if 'app1' in entries else 0,table)
            self.assertEqual(budget['app_slot_bytes'],int(entries['app0'][4],0),table)
            self.assertIn('spiffs',[part['name'] for part in budget['filesystem_partitions']],table)

    def test_firmware_uses_same_policy(self):
        source=(Path(__file__).resolve().parents[1]/'firmware/include/flash_budget.h').read_text()
        self.assertEqual(int(re.search(r'FlashReservePercent=(\d+)',source)[1]),RESERVE_PERCENT)

if __name__=='__main__':unittest.main()
