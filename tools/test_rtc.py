"""Exercise production StopWatch/XIAO RTC drivers with power/clock/I2C faults.

The AMOLED driver and pin maps are covered by test_amoled175.py.
No hardware or clock settings are changed by these native tests.
"""
from pathlib import Path
import argparse
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.work' / 'rtc-tests'
WORK.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
PRELUDE = r'''
#include "wearable.h"
#include <cassert>
#include <cstring>
#include <ctime>
#include <vector>
#ifdef _WIN32
static tm* gmtime_r(const time_t* value, tm* result){*result=*gmtime(value);return result;}
#endif
bool rtcAvailable=false;
void delay(unsigned){}
struct {template<class... T> void printf(const char*,T...) {} void println(const char*){}} Serial;
'''

STOPWATCH = r'''
struct FakeI2C {
  uint8_t rtc[256]{},pm[256]{};
  int failRead=-1,failWrite=-1,failedProbes=0;
  bool present=true,pmPresent=true,ignoreHold=false;
  bool readRegister(uint8_t addr,uint8_t reg,uint8_t* result,size_t count,uint32_t){
    assert(addr==0x32||addr==0x6e);
    if((addr==0x32&&!present)||(addr==0x6e&&!pmPresent)||reg==failRead)return false;
    if(addr==0x32&&failedProbes){--failedProbes;return false;}
    memcpy(result,(addr==0x32?rtc:pm)+reg,count);return true;
  }
  bool writeRegister(uint8_t addr,uint8_t reg,const uint8_t* data,size_t count,uint32_t){
    if((addr==0x32&&!present)||(addr==0x6e&&!pmPresent)||reg==failWrite)return false;
    if(addr==0x6e&&reg==7&&ignoreHold)return true;
    if(addr==0x32&&reg==0x1d){assert(count==1);rtc[reg]&=*data;} // W0C flags
    else memcpy((addr==0x32?rtc:pm)+reg,data,count);
    return true;
  }
  bool writeRegister8(uint8_t a,uint8_t r,uint8_t d,uint32_t f){return writeRegister(a,r,&d,1,f);}
};
struct {FakeI2C In_I2C;} M5;
'''

STOPWATCH_TEST = r'''
int main(){
  auto& bus=M5.In_I2C;uint32_t result=0;
  bus.present=false;boardSensorsBegin();assert(!rtcAvailable&&!boardReadRtc(result));
  bus.present=true;bus.failedProbes=2;bus.pm[6]=0x1b;bus.pm[7]=0x45;
  bus.rtc[0x1d]=0x86;bus.rtc[0x1e]=0x40;boardSensorsBegin();assert(rtcAvailable);
  assert(bus.pm[6]==0x1f&&bus.pm[7]==0x65); // Preserve unrelated PM1 configuration.
  assert(bus.rtc[0x1d]==0x86&&bus.rtc[0x1e]==0x40); // Boot never launders invalid/stopped RTCs.
  assert(bus.rtc[0x1f]==0x30&&!boardReadRtc(result));
  for(auto date:{dragon::Calendar{2000,1,1,0,0,0},dragon::Calendar{2028,2,29,23,59,59},dragon::Calendar{2099,12,31,23,59,59}}){
    auto epoch=dragon::dateEpoch(date);assert(boardWriteRtc(epoch)&&boardReadRtc(result)&&result==epoch);
  }
  auto epoch=dragon::dateEpoch({2028,2,29,12,34,56});assert(boardWriteRtc(epoch));
  bus.rtc[0x1d]=0x84;assert(boardReadRtc(result)&&result==epoch); // VBLF/RSF are not oscillator loss.
  bus.rtc[0x1d]|=2;assert(!boardReadRtc(result)); // VLF is actual data loss.
  bus.rtc[0x1d]=0xbf;assert(boardWriteRtc(epoch));assert(bus.rtc[0x1d]==0x3d); // Preserve interrupts.
  bus.rtc[0x1e]|=0xc0;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));assert(!(bus.rtc[0x1e]&0xc0));
  bus.rtc[0x10]=0x57;assert(boardReadRtc(result)&&result==epoch+1); // Hardware time, not cached UTC.
  uint8_t saved[256];memcpy(saved,bus.rtc,sizeof(saved));rtcAvailable=false;boardSensorsBegin();
  assert(!memcmp(saved,bus.rtc,sizeof(saved))&&boardReadRtc(result)&&result==epoch+1);
  bus.rtc[0x11]=0x6a;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  bus.rtc[0x13]=0;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  bus.rtc[0x13]=0x80;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  bus.rtc[0x16]=0x27;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  assert(!boardWriteRtc(0)&&!boardWriteRtc(4102444800u));
  bus.failRead=0x1d;assert(!boardReadRtc(result));bus.failRead=-1;
  bus.failWrite=0x10;bus.rtc[0x1d]|=2;assert(!boardWriteRtc(epoch));
  assert((bus.rtc[0x1d]&2)&&!(bus.rtc[0x1e]&0x40)); // Failed write neither clears loss nor leaves STOP set.
  bus.failWrite=0x1d;assert(!boardWriteRtc(epoch));assert(!(bus.rtc[0x1e]&0x40));
  bus.failWrite=0x1e;assert(!boardWriteRtc(epoch));bus.failWrite=-1;
  bus.pmPresent=false;assert(!boardWriteRtc(epoch));bus.pmPresent=true;
  bus.pm[7]&=~0x20;bus.ignoreHold=true;assert(!boardWriteRtc(epoch));bus.ignoreHold=false;
  assert(boardWriteRtc(epoch));bus.present=false;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));
}
'''

XIAO = r'''
constexpr int pdTRUE=1;
int i2cMutex=0;
bool mutexReady=true;
unsigned pdMS_TO_TICKS(unsigned n){return n;}
int xSemaphoreTake(int,unsigned){return mutexReady;}
void xSemaphoreGive(int){}
struct FakeWire {
  uint8_t registers[256]{};bool present=true,shortRead=false;
  int failWriteRegister=-1,failedProbes=0,remaining=0;uint8_t cursor=0;std::vector<uint8_t> tx;
  void beginTransmission(uint8_t a){assert(a==0x51);tx.clear();}
  size_t write(uint8_t v){tx.push_back(v);return 1;}
  size_t write(const uint8_t* p,size_t n){tx.insert(tx.end(),p,p+n);return n;}
  int endTransmission(bool=true){
    if(!present)return 2;if(failedProbes){--failedProbes;return 2;}
    if(!tx.empty()){cursor=tx[0];if(tx.size()>1){if(cursor==failWriteRegister)return 3;memcpy(registers+cursor,tx.data()+1,tx.size()-1);}}
    return 0;
  }
  size_t requestFrom(uint8_t,uint8_t n){remaining=!present?0:shortRead?n-1:n;return remaining;}
  bool available(){return remaining>0;}
  uint8_t read(){--remaining;return registers[cursor++];}
} Wire;
'''
XIAO_TEST = r'''
int main(){
  uint32_t result=0;Wire.present=false;boardSensorsBegin();assert(!rtcAvailable);
  Wire.present=true;Wire.failedProbes=2;boardSensorsBegin();assert(rtcAvailable);
  for(auto date:{dragon::Calendar{2000,1,1,0,0,0},dragon::Calendar{2028,2,29,23,59,59},dragon::Calendar{2099,12,31,23,59,59}}){
    auto epoch=dragon::dateEpoch(date);assert(boardWriteRtc(epoch)&&boardReadRtc(result)&&result==epoch);
  }
  auto epoch=dragon::dateEpoch({2028,2,29,12,34,56});assert(boardWriteRtc(epoch));
  Wire.registers[2]|=0x80;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  Wire.registers[0]=0xa8;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch)&&Wire.registers[0]==0);
  Wire.registers[2]=0x57;boardSensorsBegin();assert(boardReadRtc(result)&&result==epoch+1);
  Wire.registers[3]=0x6a;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  Wire.registers[8]=0x27;assert(!boardReadRtc(result));assert(boardWriteRtc(epoch));
  mutexReady=false;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));mutexReady=true;
  Wire.shortRead=true;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));Wire.shortRead=false;
  Wire.failWriteRegister=2;assert(!boardWriteRtc(epoch));assert(!(Wire.registers[0]&0x20));
  Wire.failWriteRegister=0;assert(!boardWriteRtc(epoch));Wire.failWriteRegister=-1;
  assert(!boardWriteRtc(0)&&!boardWriteRtc(4102444800u));assert(boardWriteRtc(epoch));
  Wire.present=false;assert(!boardReadRtc(result)&&!boardWriteRtc(epoch));
}
'''

for board, fake, test in [('stopwatch', STOPWATCH, STOPWATCH_TEST), ('xiao_round', XIAO, XIAO_TEST)]:
    driver = (ROOT / f'firmware/src/board_{board}.cpp').read_text(encoding='utf-8')
    source = WORK / f'{board}.cpp'
    source.write_text(PRELUDE + fake + driver[driver.index('static bool rtcRead('):] + test, encoding='utf-8')
    exe = WORK / (board + ('.exe' if sys.platform == 'win32' else ''))
    subprocess.run(compiler + ['-std=c++17', '-UNDEBUG',
                    '-I' + str(ROOT / 'firmware/include'), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print('PASS', board, 'RTC retention, flags, UTC, calendar and I2C failure checks', flush=True)
