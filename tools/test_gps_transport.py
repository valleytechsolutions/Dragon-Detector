"""Exercise the production cooperative LC76G transport with a fake Wire bus."""
from pathlib import Path
import argparse, subprocess, sys
root=Path(__file__).resolve().parents[1];work=root/'.work'/'gps-tests';work.mkdir(parents=True,exist_ok=True)
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
driver=(root/'firmware/src/wardrive.cpp').read_text()
driver=driver[driver.index('enum class Phase'):driver.index('#endif',driver.index('enum class Phase'))]
source=r'''
#include "wardrive_format.h"
#include <algorithm>
#include <cassert>
#include <vector>
#include <string>
uint32_t now=0,gpsErrors=0;bool present=false;
uint32_t millis(){return now;}
dragon::wardrive::Nmea gps;
struct FakeWire {
 uint32_t speed=400000;bool missing=false,shortRead=false,oversized=false;
 bool lengthRequest=false;unsigned commandCount=0,dataReads=0;
 size_t requested=0,cursor=0;std::vector<uint8_t> tx,reply,fifo;
 uint32_t getClock(){return speed;}void setClock(uint32_t n){speed=n;}
 void beginTransmission(uint8_t address){assert(address==0x50);assert(speed==100000);tx.clear();}
 void write(const uint8_t* data,size_t n){tx.assign(data,data+n);}
 int endTransmission(){
  ++commandCount;if(missing)return 2;
  assert(tx.size()==8&&tx[2]==0x51&&tx[3]==0xaa);
  uint32_t size=tx[4]|uint32_t(tx[5])<<8|uint32_t(tx[6])<<16|uint32_t(tx[7])<<24;
  lengthRequest=tx[0]==8;
  if(lengthRequest){assert(tx[1]==0&&size==4);}
  else{assert(tx[0]==0&&tx[1]==0x20&&size<=16384);requested=size;}
  return 0;
 }
 size_t requestFrom(uint8_t address,size_t count){
  assert(address==0x54&&count<=128&&speed==100000);reply.clear();cursor=0;
  if(missing)return 0;
  if(lengthRequest){assert(count==4);uint32_t size=oversized?0xffffffff:unsigned(fifo.size());for(unsigned i=0;i<4;++i)reply.push_back(size>>(i*8));}
  else{++dataReads;assert(count<=requested&&count<=fifo.size());reply.assign(fifo.begin(),fifo.begin()+count);fifo.erase(fifo.begin(),fifo.begin()+count);requested-=count;}
  return shortRead?count-1:count;
 }
 int available(){return int(reply.size()-cursor);}
 uint8_t read(){assert(cursor<reply.size());return reply[cursor++];}
} Wire;
std::string sentence(const std::string& body){uint8_t sum=0;for(char c:body)sum^=c;char end[8];snprintf(end,sizeof(end),"*%02X\r\n",sum);return "$"+body+end;}
''' +driver+r'''
void step(uint32_t elapsed){now+=elapsed;gpsLoop();assert(Wire.speed==400000);}
int main(){
 auto rmc=sentence("GNRMC,123519,A,4807.038,N,01131.000,E,0,0,191026,,,A");
 auto gga=sentence("GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
 // Old buffered positions are discarded; only the newest tail is parsed.
 auto oldRmc=sentence("GNRMC,123518,A,4807.038,N,01131.000,E,0,0,191026,,,A");
 std::string input=oldRmc+std::string(4096,'\n')+rmc+gga;Wire.fifo.assign(input.begin(),input.end());
 step(0);assert(phase==Phase::LengthRead&&Wire.commandCount==1);
 for(unsigned i=0;i<9;++i)step(10);
 assert(Wire.commandCount==1&&Wire.dataReads==0); // no premature bus read
 for(unsigned i=0;i<100&&!Wire.fifo.empty();++i)step(10);
 assert(Wire.fifo.empty()&&Wire.dataReads>32&&gps.fix.usable(now));
 assert(Wire.commandCount==2&&gps.positions==1); // one full FIFO request
 assert(gpsErrors==0&&present);
 // Missing module / NACK invalidates the fix and backs off without a reboot.
 Wire.missing=true;step(300);assert(!present&&!gps.fix.usable(now)&&gpsErrors==1);
 auto oldCount=Wire.commandCount;step(1000);assert(Wire.commandCount==oldCount);
 Wire.missing=false;step(1100);assert(phase==Phase::LengthRead);
 Wire.oversized=true;step(100);assert(gpsErrors==2&&phase==Phase::LengthCommand);
 Wire.oversized=false;step(2000);Wire.shortRead=true;step(100);
 assert(gpsErrors==3&&phase==Phase::LengthCommand&&Wire.available()==0);
 Wire.shortRead=false;step(2000);step(100);assert(present&&!gps.fix.usable(now));
 // millis wrap must not make a scheduled read run early or wait forever.
 now=0xfffffff0;due=now;phase=Phase::LengthCommand;step(0);
 assert(phase==Phase::LengthRead);step(99);assert(phase==Phase::LengthRead);
 step(1);assert(phase==Phase::LengthCommand);
 puts("PASS production LC76G transport: bounded FIFO reads, cooperative delays, shared clock restoration, absent device, short reads, oversized lengths, millis rollover");
}
'''
path=work/'test.cpp';path.write_text(source,encoding='utf-8')
exe=work/('test.exe' if sys.platform=='win32' else 'test')
subprocess.run(compiler+['-O2','-UNDEBUG','-std=c++17','-I'+str(root/'firmware/include'),str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
