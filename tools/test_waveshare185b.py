"""Exercise the production 1.85B RTC and touch fault paths without hardware."""
from pathlib import Path
import argparse
import configparser
import csv
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.work' / 'waveshare185b-tests'
WORK.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
flags = ['-std=c++17', '-UNDEBUG', '-I' + str(ROOT / 'firmware/include')]
build_env = os.environ.copy()
build_env['ZIG_GLOBAL_CACHE_DIR'] = str(WORK / 'zig-global')
build_env['ZIG_LOCAL_CACHE_DIR'] = str(WORK / 'zig-local')

config = configparser.ConfigParser(interpolation=None)
config.read(ROOT / 'platformio.ini')
profile = config['env:waveshare_185b']
assert profile['board_upload.flash_size'] == '16MB'
assert '-DBOARD_WAVESHARE_185B' in profile['build_flags']
end, partitions = 0, {}
for row in csv.reader((ROOT / profile['board_build.partitions']).read_text().splitlines()):
    if not row or row[0].startswith('#'):
        continue
    offset, size = int(row[3], 0), int(row[4], 0)
    assert offset >= end and offset + size <= 0x1000000
    end = offset + size
    partitions[row[0]] = offset, size
assert partitions == {'nvs': (0x9000, 0x5000), 'otadata': (0xe000, 0x2000),
                      'app0': (0x10000, 0x400000), 'app1': (0x410000, 0x400000),
                      'spiffs': (0x810000, 0x7f0000)}

# Compile the board's actual calendar functions, using a fault-injectable I2C
# transport. This catches register offsets and failure-path oscillator restart.
driver = (ROOT / 'firmware/src/board_waveshare_185b.cpp').read_text(encoding='utf-8')
rtc = driver.split('// RTC_IMPLEMENTATION_BEGIN:', 1)[1].split('\n', 1)[1]
rtc = rtc.split('// RTC_IMPLEMENTATION_END', 1)[0]
source = r'''
#define BOARD_WAVESHARE_185B
#include "board_pins.h"
#include "wearable.h"
#include "waveshare_185b_touch.h"
#include "waveshare_185b_lcd_init.h"
#include <cassert>
#include <cstring>
#include <ctime>
#include <initializer_list>
static_assert(SCREEN_RES==360 && BOARD_HAS_SD && BOARD_HAS_AUDIO && BOARD_HAS_GPS && BOARD_GPS_UART);
static_assert(LCD_SCLK==40 && LCD_CS==21 && LCD_RESET==3 && LCD_BACKLIGHT==5 && LCD_TE==18);
static_assert(LCD_SDIO0==46 && LCD_SDIO1==45 && LCD_SDIO2==42 && LCD_SDIO3==41);
static_assert(IIC_SDA==11 && IIC_SCL==10 && TP_RESET==1 && TP_INT==4 && TP_I2C_ADDR==0x15 && BTN_PIN==0);
static_assert(SD_MMC_CLK_PIN==15 && SD_MMC_CMD_PIN==14 && SD_MMC_D0_PIN==16 && SD_MMC_D1_PIN==17 && SD_MMC_D2_PIN==12 && SD_MMC_D3_PIN==13);
constexpr int ownedPins[]={BTN_PIN,LCD_SCLK,LCD_CS,LCD_RESET,LCD_BACKLIGHT,LCD_TE,LCD_SDIO0,LCD_SDIO1,LCD_SDIO2,LCD_SDIO3,
  IIC_SDA,IIC_SCL,TP_RESET,TP_INT,SD_MMC_CLK_PIN,SD_MMC_CMD_PIN,SD_MMC_D0_PIN,SD_MMC_D1_PIN,SD_MMC_D2_PIN,SD_MMC_D3_PIN,
  AUDIO_I2S_MCK_IO,AUDIO_I2S_BCK_IO,AUDIO_I2S_WS_IO,AUDIO_I2S_DO_IO,AUDIO_MUTE_PIN,19,20,43,44};
constexpr bool uniquePins(){for(size_t i=0;i<sizeof(ownedPins)/sizeof(*ownedPins);++i)for(size_t j=0;j<i;++j)if(ownedPins[i]==ownedPins[j])return false;return true;}
static_assert(uniquePins(),"Native USB and expansion UART must not collide with onboard buses");
#ifdef _WIN32
static tm* gmtime_r(const time_t* value, tm* result){*result=*gmtime(value);return result;}
#endif
struct {template<class... T> void printf(const char*,T...) {}} Serial;
bool rtcAvailable=false;
void delay(unsigned){}
struct FakeBus {
  uint8_t registers[256]{};
  bool present=true,shortRead=false;
  int failWriteRegister=-1;
  unsigned writes=0;
} bus;
bool waveshare185bI2cRead(uint8_t address,uint8_t reg,uint8_t* data,size_t count){
  assert(address==0x51);
  if(!bus.present||bus.shortRead)return false;
  memcpy(data,bus.registers+reg,count);return true;
}
bool waveshare185bI2cWrite(uint8_t address,uint8_t reg,const uint8_t* data,size_t count){
  assert(address==0x51);++bus.writes;
  if(!bus.present||reg==bus.failWriteRegister)return false;
  memcpy(bus.registers+reg,data,count);return true;
}
''' + rtc + r'''
static void rtcChecks(){
  uint32_t result=123;
  bus.present=false;boardSensorsBegin();assert(!rtcAvailable);
  assert(!boardReadRtc(result)&&result==123&&!boardWriteRtc(1792380000));
  bus.present=true;bus.registers[4]=0x80;boardSensorsBegin();assert(rtcAvailable);
  assert(bus.writes==0&&(bus.registers[4]&0x80)); // boot must not erase lost-time state
  for(const auto& date:{dragon::Calendar{2026,11,3,23,59,59},dragon::Calendar{2028,2,29,0,0,0},dragon::Calendar{2099,12,31,23,59,59}}){
    auto epoch=dragon::dateEpoch(date);assert(boardWriteRtc(epoch));
    assert(boardReadRtc(result)&&result==epoch);assert((bus.registers[0]&0xa2)==0);
  }
  const auto epoch=dragon::dateEpoch({2028,2,29,12,34,56});
  bus.registers[0]=5;assert(boardWriteRtc(epoch));assert(bus.registers[0]==5);
  bus.registers[4]|=0x80;assert(!boardReadRtc(result));
  for(uint8_t badControl:{uint8_t(0x20),uint8_t(0x02),uint8_t(0x80)}){
    assert(boardWriteRtc(epoch));bus.registers[0]|=badControl;assert(!boardReadRtc(result));
  }
  assert(boardWriteRtc(epoch));assert(!boardWriteRtc(0)&&!boardWriteRtc(4102444800u));
  bus.registers[5]=0x6a;assert(!boardReadRtc(result)); // malformed minute BCD
  assert(boardWriteRtc(epoch));bus.registers[10]=0x27;assert(!boardReadRtc(result)); // Feb29 non-leap
  assert(boardWriteRtc(epoch));bus.registers[8]=7;assert(!boardReadRtc(result)); // invalid weekday
  assert(boardWriteRtc(epoch));bus.shortRead=true;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));bus.shortRead=false;
  bus.failWriteRegister=4;assert(!boardWriteRtc(epoch));assert(!(bus.registers[0]&0x20)); // restart after failed calendar write
  bus.failWriteRegister=0;assert(!boardWriteRtc(epoch));
  bus.failWriteRegister=-1;assert(boardWriteRtc(epoch));
  bus.present=false;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));
}
static void touchChecks(){
  using waveshare185b::TouchDecision;
  waveshare185b::TouchAgeGuard age;
  assert(waveshare185b::inputFresh(100,600)&&!waveshare185b::inputFresh(100,601));
  assert(age.check(true,100,110)==TouchDecision::Accept);
  assert(age.check(false,110,700)==TouchDecision::Reset); // old release must cancel, never click
  assert(age.check(true,700,710)==TouchDecision::Suppress); // current held finger cannot become a new press
  assert(age.check(false,710,710,false,false)==TouchDecision::Suppress&&age.waitForRelease); // I2C timeout is not physical release
  assert(age.check(false,720,720)==TouchDecision::Suppress&&!age.waitForRelease);
  assert(age.check(true,730,730)==TouchDecision::Accept);
  assert(age.check(true,740,740,true)==TouchDecision::Reset); // queue overflow or UI stall
  assert(age.check(true,750,750)==TouchDecision::Suppress);
  assert(age.check(false,760,760)==TouchDecision::Suppress);
  assert(age.check(true,770,770)==TouchDecision::Accept);
  assert(waveshare185b::inputFresh(0xfffffff0u,0x20)&&!waveshare185b::inputFresh(0xfffffff0u,0x200));
  waveshare185b::TouchState touch;
  uint8_t press[]={0,1,0,100,1,103}; // x100, y359
  assert(touch.update(press,6,true,100)&&touch.pressed&&touch.x==100&&touch.y==359);
  assert(!touch.update(press,5,true,150)&&touch.pressed); // short transfer doesn't split hold
  assert(!touch.update(nullptr,0,false,219)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,220)&&!touch.pressed); // bounded stale release
  uint8_t offscreen[]={0,1,1,104,0,10}; // x360 must be rejected, not clamped
  assert(!touch.update(offscreen,6,true,230)&&!touch.pressed);
  uint8_t unsupported[]={0,2,0,100,0,100};
  assert(!touch.update(unsupported,6,true,240)&&!touch.pressed);
  assert(touch.update(press,6,true,250)&&touch.pressed);
  uint8_t release[]={5,0,0,100,1,103}; // gesture can persist while physical finger count is zero
  assert(touch.update(release,6,true,251)&&!touch.pressed);
  assert(touch.update(press,6,true,0xfffffff0u)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,0x20)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,0x68)&&!touch.pressed); // millis rollover
  // Both vendor tables must be nonempty, bounded and finish display-on after a
  // sufficiently delayed sleep-out; prevents truncating the long initialization.
  for(auto table:{std::pair<const waveshare185b::LcdCommand*,size_t>{waveshare185b::panelV1,sizeof(waveshare185b::panelV1)/sizeof(waveshare185b::LcdCommand)},
                  {waveshare185b::panelV2,sizeof(waveshare185b::panelV2)/sizeof(waveshare185b::LcdCommand)}}){
    assert(table.second>170);
    for(size_t i=0;i<table.second;++i)assert(table.first[i].size<=sizeof(table.first[i].data));
    assert(table.first[table.second-2].command==0x11&&table.first[table.second-2].delayMs>=120);
    assert(table.first[table.second-1].command==0x29);
  }
}
int main(){rtcChecks();touchChecks();}
'''
source = source.replace('#include <initializer_list>', '#include <initializer_list>\n#include <utility>')
path = WORK / 'protocol.cpp'
path.write_text(source, encoding='utf-8')
exe = WORK / ('protocol.exe' if sys.platform == 'win32' else 'protocol')
subprocess.run(compiler + flags + [str(path), '-o', str(exe)], check=True, env=build_env)
subprocess.run([str(exe)], check=True)
print('PASS 1.85B pin/bus isolation and 16 MB partitions; production RTC UTC/leap-date/oscillator/I2C checks; touch short-read, invalid-coordinate, stale-queue cancellation, held-finger suppression, overflow resync and rollover checks; both LCD tables')
