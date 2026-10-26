"""Exercise the production LC76G driver through sleeps and interrupted transfers."""
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
uint32_t millis(){return now;}uint32_t micros(){return now*1000;}
dragon::wardrive::Nmea gps;
struct FakeWire {
 uint32_t speed=400000;bool missing=false,shortRead=false,oversized=false,asleep=true;
 enum Reply {None,Length,Space,Data,Wake} pending=None;
 unsigned commandCount=0,dataReads=0,wakes=0;uint8_t address=0;
 size_t requested=0,cursor=0;std::vector<uint8_t> tx,reply,fifo;
 uint32_t getClock(){return speed;}void setClock(uint32_t n){speed=n;}
 void beginTransmission(uint8_t a){assert(a==0x50||a==0x58);assert(speed==100000);address=a;tx.clear();}
 void write(uint8_t b){tx.push_back(b);}
 void write(const uint8_t* data,size_t n){tx.insert(tx.end(),data,data+n);}
 int endTransmission(){
  ++commandCount;if(missing)return 2;
  if(address==0x58){assert(tx.size()==1&&tx[0]==0);if(pending!=Wake){pending=None;return 2;}asleep=false;pending=None;++wakes;return 0;}
  assert(tx.size()==8&&tx[3]==0xaa);
  requested=tx[4]|uint32_t(tx[5])<<8|uint32_t(tx[6])<<16|uint32_t(tx[7])<<24;
  if(tx[2]==0x53){assert(tx[0]==0&&tx[1]==0x10&&requested==1);pending=Wake;}
  else {
   assert(tx[2]==0x51);
   if(tx[0]==8){assert(tx[1]==0&&requested==4);pending=Length;}
   else if(tx[0]==4){assert(tx[1]==0&&requested==4);pending=Space;}
   else{assert(tx[0]==0&&tx[1]==0x20&&requested&&requested<=128&&requested<=fifo.size());pending=Data;}
  }return 0;
 }
 size_t requestFrom(uint8_t a,size_t count){
  assert(a==0x54&&count<=128&&speed==100000);reply.clear();cursor=0;
  if(missing||asleep)return 0;
  if(count==1&&pending!=Data){pending=None;return 0;}
  if(pending==Length||pending==Space){assert(count==4);uint32_t size=pending==Space?4096:oversized?0xffffffff:unsigned(fifo.size());for(unsigned i=0;i<4;++i)reply.push_back(size>>(i*8));}
  else{assert(pending==Data&&count==requested);++dataReads;reply.assign(fifo.begin(),fifo.begin()+count);fifo.erase(fifo.begin(),fifo.begin()+count);}
  pending=None;return shortRead?count-1:count;
 }
 int available(){return int(reply.size()-cursor);}
 uint8_t read(){assert(cursor<reply.size());return reply[cursor++];}
 void otherDevice(){assert(pending==None);}
} Wire;
std::string sentence(const std::string& body){uint8_t sum=0;for(char c:body)sum^=c;char end[8];snprintf(end,sizeof(end),"*%02X\r\n",sum);return "$"+body+end;}
''' +driver+r'''
void step(uint32_t elapsed){now+=elapsed;gpsLoop();assert(Wire.speed==400000);if(!gpsReserved&&!Wire.missing&&!Wire.asleep&&!Wire.shortRead)Wire.otherDevice();}
void until(Phase target){for(unsigned i=0;i<5000&&phase!=target;++i)step(2);assert(phase==target);}
void drain(){for(unsigned i=0;i<10000&&(!Wire.fifo.empty()||gpsReserved);++i)step(2);assert(Wire.fifo.empty()&&!gpsReserved);}
int main(){
 auto rmc=sentence("GNRMC,123519,A,4807.038,N,01131.000,E,0,0,191026,,,A");
 auto gga=sentence("GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
 auto old=sentence("GNRMC,123518,A,4807.038,N,01131.000,E,0,0,191026,,,A");
 std::string input=old+std::string(9000,'\n')+rmc+gga;Wire.fifo.assign(input.begin(),input.end());
 // GNSS can remain powered and fall asleep while the host is being flashed.
 drain();assert(Wire.wakes==1&&recoveries==1&&gpsErrors==0);
 assert(Wire.dataReads>64&&gps.positions==1&&gps.sentences==2&&gps.fix.usable(now)&&present);
 // No other slave may interrupt command/reply pairs, even during idle waits.
 step(10);assert(gpsReserved&&phase==Phase::LengthRead);auto commands=Wire.commandCount;
 step(9);assert(gpsReserved&&phase==Phase::LengthRead&&commands==Wire.commandCount);
 step(1);assert(!gpsReserved&&phase==Phase::LengthCommand);
 // Failed replies recover with backoff, and old positions become unusable.
 Wire.asleep=true;unsigned wakes=Wire.wakes;
 for(unsigned i=0;i<10000&&Wire.wakes==wakes;++i)step(2);
 assert(Wire.wakes==wakes+1);until(Phase::LengthCommand);step(10);step(10);
 assert(present&&gpsErrors==3&&!gpsReserved&&!gps.fix.position);
 Wire.missing=true;step(200);assert(!present&&!gpsReserved);
 commands=Wire.commandCount;step(999);assert(Wire.commandCount==commands);
 Wire.missing=false;step(1);assert(gpsReserved&&phase==Phase::LengthRead);
 Wire.oversized=true;auto errors=gpsErrors;step(10);assert(gpsErrors==errors+1&&!gpsReserved&&remaining==0);
 Wire.oversized=false;until(Phase::LengthRead);Wire.shortRead=true;errors=gpsErrors;step(10);
 assert(gpsErrors==errors+1&&!gpsReserved&&Wire.available()==0);Wire.shortRead=false;
 until(Phase::LengthRead);step(10);assert(present&&!gps.fix.usable(now));
 // A short data reply discards any partial NMEA sentence.
 Wire.fifo.assign(rmc.begin(),rmc.end());until(Phase::DataRead);Wire.shortRead=true;errors=gpsErrors;step(10);
 assert(gpsErrors==errors+1&&remaining==0&&!gpsReserved&&!gps.fix.position);Wire.shortRead=false;Wire.pending=FakeWire::None;
 // Empty valid replies trigger a wake after a bounded number of polls.
 wakes=Wire.wakes;for(unsigned i=0;i<10000&&Wire.wakes==wakes;++i)step(2);assert(Wire.wakes==wakes+1);
 until(Phase::LengthCommand);step(10);step(10);
 now=0xfffffff8;due=now;phase=Phase::LengthCommand;step(0);assert(gpsReserved);
 step(9);assert(phase==Phase::LengthRead);step(1);assert(!gpsReserved&&phase==Phase::LengthCommand);
 puts("PASS LC76G startup wake, stale transaction recovery, exclusive bus windows, complete bounded replies, stale fix rejection, missing device, short reads, oversized length, rollover");
}
'''
path=work/'test.cpp';path.write_text(source,encoding='utf-8')
exe=work/('test.exe' if sys.platform=='win32' else 'test')
subprocess.run(compiler+['-O2','-UNDEBUG','-std=c++17','-I'+str(root/'firmware/include'),str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
