"""Check both AMOLED pin maps and exercise the production RTC code with a fake I2C bus."""
from pathlib import Path
import argparse
import configparser
import csv
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.work' / 'amoled175-tests'
WORK.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
flags = ['-std=c++17', '-UNDEBUG', '-I' + str(ROOT / 'firmware/include')]

for board, expected in {
    'BOARD_AMOLED_175': (39, 40, 42, 1),
    'BOARD_AMOLED_175C': (1, 2, 16, 0),
}.items():
    source = WORK / (board + '.cpp')
    lcd, touch, mclk, sd = expected
    source.write_text(f'''#include "board_pins.h"
static_assert(SCREEN_RES == 466);
static_assert(LCD_RESET == {lcd} && TP_RESET == {touch});
static_assert(AUDIO_I2S_MCK_IO == {mclk} && BOARD_HAS_SD == {sd});
static_assert(IIC_SDA == 15 && IIC_SCL == 14 && TP_INT == 11);
#ifdef BOARD_AMOLED_175
static_assert(SD_MMC_CMD_PIN == 1 && SD_MMC_CLK_PIN == 2 && SD_MMC_D0_PIN == 3);
static_assert(LCD_RESET != SD_MMC_CMD_PIN && TP_RESET != SD_MMC_CLK_PIN);
#endif
''', encoding='utf-8')
    subprocess.run(compiler + flags + ['-D' + board, '-c', str(source), '-o', str(WORK / (board + '.o'))], check=True)

config = configparser.ConfigParser(interpolation=None)
config.read(ROOT / 'platformio.ini')
env = config['env:waveshare_175']
assert env['board_upload.flash_size'] == '16MB'
assert '-DBOARD_AMOLED_175 ' in env['build_flags'] and 'BOARD_AMOLED_175C' not in env['build_flags']
end = 0
rows = {}
for row in csv.reader((ROOT / env['board_build.partitions']).read_text().splitlines()):
    if not row or row[0].startswith('#'):
        continue
    offset, size = int(row[3], 0), int(row[4], 0)
    assert offset >= end and offset + size <= 0x1000000
    end = offset + size
    rows[row[0]] = (offset, size)
assert rows == {'nvs': (0x9000, 0x5000), 'otadata': (0xe000, 0x2000),
                'app0': (0x10000, 0x400000), 'app1': (0x410000, 0x400000),
                'spiffs': (0x810000, 0x7f0000)}

# Compile the actual board functions so the test cannot drift into a second RTC implementation.
driver = (ROOT / 'firmware/src/board_amoled175c.cpp').read_text(encoding='utf-8')
rtc = driver[driver.index('static bool rtcRead('):]
source = r'''
#include "wearable.h"
#include <cassert>
#include <cstring>
#include <ctime>
#include <vector>
#ifdef _WIN32
static tm* gmtime_r(const time_t* value, tm* result){*result=*gmtime(value);return result;}
#endif
struct {template<class... T> void printf(const char*,T...) {}} Serial;
bool rtcAvailable=false;
void delay(unsigned){}
struct FakeWire {
  uint8_t registers[256]{};bool present=true,shortRead=false;
  int failWriteRegister=-1;uint8_t cursor=0;std::vector<uint8_t> tx;
  void beginTransmission(uint8_t address){assert(address==0x51);tx.clear();}
  size_t write(uint8_t value){tx.push_back(value);return 1;}
  size_t write(const uint8_t* data,size_t count){tx.insert(tx.end(),data,data+count);return count;}
  uint8_t endTransmission(bool=true){
    if(!present)return 2;
    if(!tx.empty()){
      cursor=tx[0];
      if(tx.size()>1){if(cursor==failWriteRegister)return 3;memcpy(registers+cursor,tx.data()+1,tx.size()-1);}
    }
    return 0;
  }
  size_t requestFrom(uint8_t address,uint8_t count){assert(address==0x51);return !present?0:shortRead?count-1:count;}
  uint8_t read(){return registers[cursor++];}
} Wire;
''' + rtc + r'''
int main(){
  uint32_t result=0;
  Wire.present=false;boardSensorsBegin();assert(!rtcAvailable);
  assert(!boardReadRtc(result)&&!boardWriteRtc(1792380000));
  Wire.present=true;boardSensorsBegin();assert(rtcAvailable);
  for(const auto& date:{dragon::Calendar{2026,10,19,23,59,59},dragon::Calendar{2028,2,29,0,0,0},dragon::Calendar{2099,12,31,23,59,59}}){
    auto epoch=dragon::dateEpoch(date);assert(boardWriteRtc(epoch));assert(boardReadRtc(result)&&result==epoch);
    assert((Wire.registers[0]&0x22)==0);
  }
  auto epoch=dragon::dateEpoch({2028,2,29,12,34,56});Wire.registers[0]=5;assert(boardWriteRtc(epoch));assert(Wire.registers[0]==5);
  Wire.registers[4]|=0x80;assert(!boardReadRtc(result)); // oscillator-stop flag
  assert(boardWriteRtc(epoch));Wire.registers[0]|=0x20;assert(!boardReadRtc(result));
  assert(boardWriteRtc(epoch));Wire.registers[0]|=2;assert(!boardReadRtc(result));
  assert(boardWriteRtc(epoch));Wire.registers[0]|=0x80;assert(!boardReadRtc(result));
  assert(boardWriteRtc(epoch));assert(!(Wire.registers[0]&0xa2));
  assert(!boardWriteRtc(0)&&!boardWriteRtc(4102444800u));
  assert(boardWriteRtc(epoch));Wire.registers[5]=0x6a;assert(!boardReadRtc(result));
  assert(boardWriteRtc(epoch));Wire.registers[10]=0x27;assert(!boardReadRtc(result)); // Feb 29 in a non-leap year
  assert(boardWriteRtc(epoch));Wire.shortRead=true;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));Wire.shortRead=false;
  Wire.failWriteRegister=4;assert(!boardWriteRtc(epoch));assert(!(Wire.registers[0]&0x20)); // restart even after failed date write
  Wire.failWriteRegister=0;assert(!boardWriteRtc(epoch));
  Wire.failWriteRegister=-1;assert(boardWriteRtc(epoch));
  Wire.present=false;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));
}
'''
path = WORK / 'rtc.cpp'
path.write_text(source, encoding='utf-8')
exe = WORK / ('rtc.exe' if sys.platform == 'win32' else 'rtc')
subprocess.run(compiler + flags + [str(path), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print('PASS AMOLED C/non-C pin isolation, 16 MB partitions, RTC UTC round trips and I2C/oscillator failure handling')
