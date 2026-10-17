"""Check packaging limits, rounding and the shared firmware policy."""
from pathlib import Path
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

    def test_firmware_uses_same_policy(self):
        source=(Path(__file__).resolve().parents[1]/'firmware/include/flash_budget.h').read_text()
        self.assertEqual(int(re.search(r'FlashReservePercent=(\d+)',source)[1]),RESERVE_PERCENT)

if __name__=='__main__':unittest.main()
