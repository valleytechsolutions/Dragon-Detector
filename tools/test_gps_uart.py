"""Exercise production optional UART GNSS transport/configuration with fake I/O."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];work=root/'.work'/'gps-uart-tests';work.mkdir(parents=True,exist_ok=True)
driver=(root/'firmware/src/wardrive.cpp').read_text()
def function(signature):
    start=driver.index(signature);brace=driver.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(driver[end]=='{')-(driver[end]=='}');end+=1
    return driver[start:end]
source=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include "wardrive_format.h"
#include "gps_uart_config.h"
#define BOARD_GPS_UART 1
using hardwareSerial_error_t=int;
constexpr int SERIAL_8N1=0;
uint32_t now=100;uint32_t millis(){return now;}
bool present=false,owned=false,upload=false,other=false,storage=false;
bool wardriveUploadBusy(){return upload;}bool reconOwnsRadio(){return other;}bool reconStorageBusy(){return storage;}
uint32_t gpsErrors=0;dragon::wardrive::Nmea gps;char notice[120]{};
bool located(uint32_t t){return gps.fix.usable(t);}
size_t strlcpy(char* dst,const char* src,size_t n){size_t len=strlen(src);memcpy(dst,src,std::min(len,n-1));dst[std::min(len,n-1)]=0;return len;}
struct Preferences {bool ready=true,fail=false;uint32_t value=0;unsigned writes=0;
 bool begin(const char*,bool){return ready;}uint32_t getUInt(const char*,uint32_t fallback){return value?value:fallback;}
 size_t putUInt(const char*,uint32_t v){if(fail)return 0;value=v;++writes;return 4;}
};
struct HardwareSerial {std::deque<char> data;bool opened=false,fail=false;unsigned starts=0,reads=0;int rx=-1,tx=-1;uint32_t baud=0;std::function<void(int)> error;
 HardwareSerial(int n){assert(n==1);}void end(){opened=false;rx=tx=-1;data.clear();error={};}
 size_t setRxBufferSize(size_t n){assert(n==4096);return n;}
 void begin(uint32_t b,int,int r,int t){++starts;baud=b;rx=r;tx=t;assert(t==-1);opened=!fail;}
 explicit operator bool()const{return opened;}void onReceiveError(std::function<void(int)> cb){error=cb;}
 unsigned available(){return data.size();}int read(){if(data.empty())return -1;int c=data.front();data.pop_front();++reads;return c;}
};
''' +(root/'firmware/src/wardrive_uart.inc').read_text()+function('dragon::gpsuart::Config gpsExternalConfig(')+function('void gpsExternalSuspend(')+function('bool gpsExternalConfigure(')+r'''
void inject(const std::string& body){uint8_t sum=0;for(char c:body)sum^=c;char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",sum);for(char c:"$"+body+tail)externalGpsSerial.data.push_back(c);}
void fix(unsigned second){char line[160];snprintf(line,sizeof(line),"GNRMC,1235%02u,A,4807.038,N,01131.000,E,0,0,261026,,,A",second);inject(line);snprintf(line,sizeof(line),"GNGGA,1235%02u,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",second);inject(line);gpsLoop();}
int main(){using namespace dragon::gpsuart;
 Config c;assert(!c.enabled&&c.rx==18&&c.baud==9600&&headerPin(18)==7);
 for(int pin:Pins)for(uint32_t baud:Bauds)for(bool enabled:{false,true}){Config next{enabled,pin,baud},read;assert(decode(encode(next),read)&&read.rx==pin&&read.baud==baud&&read.enabled==enabled);}
 assert(!decode(0,c)&&!decode(0xffffffff,c)&&!decode(0x47500138,c)&&!decode(0x47500140,c));
 externalGpsBegin();assert(!externalGpsOpen&&!externalGpsSerial.starts&&!present);gpsLoop();assert(!present);
 for(int pin:{0,1,2,3,8,9,14,15,19,20,39,40,42,43})assert(!gpsExternalConfigure({true,pin,9600}));
 assert(!gpsExternalConfigure({true,18,12345})&&!externalGpsPrefs.writes);
 assert(gpsExternalConfigure({true,18,115200})&&externalGpsOpen&&externalGpsSerial.rx==18&&externalGpsSerial.tx==-1);
 for(bool* busy:{&owned,&upload,&other,&storage}){*busy=true;assert(!gpsExternalConfigure({false,18,115200}));*busy=false;}
 unsigned writes=externalGpsPrefs.writes;externalGpsPrefs.fail=true;assert(!gpsExternalConfigure({false,18,115200}));assert(externalGpsConfig.enabled&&externalGpsPrefs.writes==writes);externalGpsPrefs.fail=false;
 fix(19);assert(present&&gps.fix.usable(now)&&gps.positions==1);
 unsigned savedWrites=externalGpsPrefs.writes;gpsExternalSuspend(true);assert(!externalGpsOpen&&!present&&!gps.fix.usable(now)&&externalGpsConfig.enabled&&externalGpsPrefs.writes==savedWrites);
 assert(!gpsExternalConfigure({false,18,115200}));gpsExternalSuspend(true);gpsExternalSuspend(false);assert(externalGpsOpen&&externalGpsSerial.baud==115200&&externalGpsSerial.tx==-1&&externalGpsPrefs.writes==savedWrites);
 fix(19);assert(present&&gps.fix.usable(now));
 now+=3001;gpsLoop();assert(!present&&!gps.fix.usable(now));
 fix(20);assert(present&&gps.fix.usable(now));
 externalGpsSerial.data.assign(1000,'x');unsigned before=externalGpsSerial.reads;gpsLoop();assert(externalGpsSerial.reads-before==512&&externalGpsSerial.available()==488);externalGpsSerial.data.clear();
 externalGpsSerial.data.assign(3000,'x');gpsLoop();assert(!gps.fix.usable(now)&&gpsErrors==1&&externalGpsSerial.available()==0);
 fix(21);assert(gps.fix.usable(now));externalGpsSerial.error(1);inject("GNRMC,123522,A,4807.038,N,01131.000,E,0,0,261026,,,A");gpsLoop();assert(!gps.fix.usable(now)&&gpsErrors==2&&externalGpsSerial.available()==0);
 fix(23);assert(gps.fix.usable(now));assert(gpsExternalConfigure({false,16,9600}));assert(!externalGpsOpen&&!present&&!gps.positions&&!gps.fix.usable(now));
 assert(gpsExternalConfigure({true,44,9600}));externalGpsConfig={};externalGpsBegin();assert(externalGpsConfig.enabled&&externalGpsConfig.rx==44&&externalGpsSerial.tx==-1);
 externalGpsSerial.fail=true;assert(!gpsExternalConfigure({true,18,9600})&&!externalGpsOpen);externalGpsSerial.fail=false;
 now=0xfffffff0;assert(gpsExternalConfigure({true,18,9600}));fix(24);now=20;gpsLoop();assert(present&&gps.fix.usable(now));now=4000;gpsLoop();assert(!present&&!gps.fix.usable(now));
 puts("PASS optional GPS default-off, pin/baud allowlist, atomic settings, receive-only UART, live NMEA, freshness, bounded reads, overflow, errors, disable/restart and rollover");
}
'''
cpp=work/'gps_uart.cpp';exe=work/'gps_uart.exe';cpp.write_text(source)
subprocess.run([sys.executable,'-m','ziglang','c++','-UNDEBUG','-std=c++17','-O1','-Ifirmware/include',str(cpp),'-o',str(exe)],cwd=root,check=True)
subprocess.run([str(exe)],check=True)
