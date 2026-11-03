"""Run production wardrive lifecycle/recording and UI exit logic with fake I/O."""
from pathlib import Path
import argparse
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
root = Path(__file__).resolve().parents[1]
work = root / '.work' / 'wardrive-tests'
work.mkdir(parents=True, exist_ok=True)
driver = (root / 'firmware/src/wardrive.cpp').read_text()
ui = (root / 'firmware/src/recon_ui.cpp').read_text()

def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

snapshot = (root / 'firmware/include/wardrive.h').read_text()
snapshot = snapshot[snapshot.index('struct WardriveSnapshot'):snapshot.index('#if BOARD_HAS_GPS')]
source = r'''
#include "wardrive_format.h"
#include <algorithm>
#include <cassert>
#include <string>
#include <vector>
#define BOARD_HAS_GPS 1
#define BOARD_GPS_LC76G 1
#define BOARD_AMOLED_175 1
#define DRAGON_VERSION "test"
uint32_t now=1000;uint32_t millis(){return now;}
size_t strlcpy(char* out,const char* in,size_t size){size_t n=strlen(in);if(size){memcpy(out,in,std::min(n,size-1));out[std::min(n,size-1)]=0;}return n;}
void* ps_calloc(size_t n,size_t size){return calloc(n,size);}
struct {bool running=false;} stopwatch;
bool standby=false,radioEnabled=true,otherRadio=false,storageBusy=false,captureEnabled=false,fox=false;
bool radioStandby(){return standby;}bool bleLabOwnsRadio(){return otherRadio;}
bool reconOwnsRadio(){return otherRadio;}bool reconStorageBusy(){return storageBusy;}
bool missionActive(){return false;}bool recipeActive(){return false;}bool foxTransmit(){return fox;}
namespace dragon::toolkit {struct {bool recording=false;} status;}
unsigned audioStops=0,gpsCalls=0;std::vector<bool> transitions;
void audioStop(){++audioStops;}bool radioWardrive(bool active){transitions.push_back(active);return true;}
void gpsLoop(){++gpsCalls;}uint16_t remaining=0;dragon::wardrive::Fix stableFix;
uint32_t softwareClock=1792380000;unsigned clockWrites=0;
uint32_t wallTime(){return softwareClock;}bool wardriveI2cBusy(){return false;}
bool configureClock(uint32_t epoch){softwareClock=epoch;++clockWrites;return true;}
struct SdRecords {
 bool accepting=false,closing=false,error=false,missing=false,failNext=false;
 uint32_t dropped=0;uint64_t bytes=0;size_t queued=0;char message[120]{};
 bool begin(const char*,const char*,const char*,const char*,size_t n){
   error=false;bytes=queued=dropped=0;
   if(missing){strlcpy(message,"Mounted SD required",sizeof(message));return false;}
   accepting=true;closing=false;bytes=n;return true;
 }
 bool append(const void*,size_t n){if(!accepting)return false;queued+=n;return true;}
 void stop(){accepting=false;closing=true;}
 void loop(){if(failNext){error=true;accepting=closing=false;queued=0;++dropped;strlcpy(message,"SD write failed; .part retained",sizeof(message));failNext=false;return;}bytes+=queued;queued=0;if(closing){closing=false;strlcpy(message,"Saved /dragon/wardrive",sizeof(message));}}
 bool active(){return accepting;}bool busy(){return accepting||closing;}bool failed(){return error;}
 size_t pending(){return queued;}const char* path(){return "/wardrive/test.csv";}
};
''' + snapshot
source += driver[driver.index('dragon::wardrive::Nmea gps;'):driver.index('#ifdef BOARD_GPS_LC76G\nenum class Phase')]
for name in ['const dragon::wardrive::Fix& recordingFix(', 'bool located(', 'bool wardriveSyncClock(', 'void record(', 'bool wardriveActive(', 'bool wardriveBusy(', 'bool wardriveAvailable(',
             'WardriveSnapshot wardriveSnapshot(', 'bool wardriveStart(', 'void wardriveStop(', 'void wardriveLoop(']:
    source += '\n' + function(driver, name)
source += r'''
// The actual dashboard exit controller must wait for writer finalization.
using String=std::string;
enum View {Modes,Tools,Wardrive,Running,Capture,Statistics,Portal,Csi,Network,Missions,Recipes,Uploads,GpsSetup};
bool toolsWardriveAvailable=false,inputReleased=true;bool gpsMenuInputReleased(){return inputReleased;}
bool wardriveExitRequested=false,redraw=false,pageRequested=false;void* root=(void*)1;
View view=Modes,requested=Modes;uint32_t updated=0;unsigned menuReturns=0,errorDraws=0;
void change(View next){view=next;redraw=true;}
void reconPage(const String&){requested=Wardrive;pageRequested=true;}
void uiShow(const char* name){assert(std::string(name)=="menu");++menuReturns;}
void draw(){++errorDraws;redraw=false;}void updateWardrive(){}void updateStatus(){}void updateUploads(){}void updateGpsSetup(){}
''' + function(ui, 'void reconEndWardrive(') + '\n' + function(ui, 'void reconViewLoop(')
source += r'''
void fix(){gps.fix.position=true;gps.fix.quality=1;gps.fix.epoch=1793030400;
 gps.fix.positionAt=gps.fix.qualityAt=now;gps.fix.latitude=1;gps.fix.longitude=2;stableFix=gps.fix;}
int main(){
 assert(wardriveSnapshot().elapsedMs==0);
 driveLog.missing=true;assert(!wardriveStart()&&!wardriveBusy()&&transitions.empty());
 driveLog.missing=false;stopwatch.running=true;assert(!wardriveStart());stopwatch.running=false;
 for(bool* conflict:{&standby,&otherRadio,&storageBusy,&captureEnabled,&fox}){*conflict=true;assert(!wardriveStart());*conflict=false;}
 radioEnabled=false;assert(!wardriveStart());radioEnabled=true;
 assert(wardriveStart()&&wardriveActive()&&wardriveBusy()&&audioStops==1&&transitions.back());
 assert(!wardriveStart()&&audioStops==1);
 uint8_t mac[6]={1,2,3,4,5,6};
 record(mac,nullptr,0,"",-40,1,false,0,now,false);
 auto s=wardriveSnapshot();assert(s.seenWifi==1&&s.wifi==0&&s.noFix==1);
 fix();record(mac,nullptr,0,"",-40,1,false,0,now,false);record(mac,nullptr,0,"[BLE]",-40,0,true,1,now);
 s=wardriveSnapshot();assert(s.wifi==1&&s.ble==1&&s.queued>0&&s.seenBle==1);
 record(mac,nullptr,0,"[BLE]",-40,0,true,1,now);assert(wardriveSnapshot().duplicates==1);
 record(mac,nullptr,0,"",-40,1,false,0,now-1001);assert(wardriveSnapshot().late==1);
 now+=5001;record(mac,nullptr,0,"",-40,1,false,0,now,false);assert(wardriveSnapshot().noFix==2);
 fix();record(mac,nullptr,0,"",-40,1,false,0,now,false);assert(wardriveSnapshot().wifi==2);
 remaining=1;record(mac,nullptr,0,"",-40,1,false,0,now,false);assert(wardriveSnapshot().wifi==3);remaining=0;
 stableFix.position=false;record(mac,nullptr,0,"",-40,1,false,0,now,false);assert(wardriveSnapshot().noFix==3);
 reconEndWardrive();assert(!wardriveActive()&&wardriveBusy());reconViewLoop();assert(menuReturns==0);
 s=wardriveSnapshot();now+=1000;assert(wardriveSnapshot().elapsedMs==s.elapsedMs);
 record(mac,nullptr,0,"",-40,1,false,0,now);assert(wardriveSnapshot().seenWifi==s.seenWifi);
 wardriveLoop();assert(!wardriveBusy()&&!transitions.back()&&!driveLog.pending());
 reconViewLoop();assert(menuReturns==1&&!wardriveExitRequested&&!pageRequested);
 // Failed finalization releases radio ownership but stays on the error page.
 assert(wardriveStart());reconEndWardrive();driveLog.failNext=true;wardriveLoop();reconViewLoop();
 assert(menuReturns==1&&!wardriveBusy()&&wardriveSnapshot().failed);
 assert(strstr(wardriveSnapshot().message,".part retained"));
 // Mid-session card failure also freezes elapsed time and releases the radios.
 assert(wardriveStart());now+=25;driveLog.failNext=true;wardriveLoop();assert(!wardriveActive()&&!wardriveBusy());
 assert(wardriveSnapshot().elapsedMs==25);
 now=0xfffffff0;assert(wardriveStart());now=0x20;assert(wardriveSnapshot().elapsedMs==48);
 wardriveStop();now+=500;assert(wardriveSnapshot().elapsedMs==48);wardriveLoop();
 now=10000;softwareClock=0;fix();wardriveLoop();assert(clockWrites==1&&softwareClock==stableFix.epoch);
 now+=6000;fix();wardriveLoop();assert(clockWrites==1); // Never rewrite a valid clock automatically.
 softwareClock=0;stableFix.position=false;wardriveLoop();assert(clockWrites==1&&!softwareClock);
 fix();assert(wardriveSyncClock()&&clockWrites==2); // Explicit fresh-fix recovery also works.
 free(seen);
 puts("PASS wardrive conflicts, counters, fresh fixes, deduplication, async stop, safe UI exit, SD failure, elapsed rollover");
}
'''
cpp = work / 'session.cpp'
exe = work / 'session.exe'
cpp.write_text(source)
subprocess.run(compiler + ['-UNDEBUG', '-std=c++17', '-O1',
                '-Ifirmware/include', str(cpp), '-o', str(exe)], cwd=root, check=True)
subprocess.run([str(exe)], cwd=root, check=True)

# Exercise the same production start/stop functions for optional UART hardware.
# A disappearing receiver must block new sessions without trapping an active one.
optional = source[:source.index('int main(){')]
optional = optional.replace('#define BOARD_GPS_LC76G 1', '#define BOARD_GPS_UART 1\n#define BOARD_WAVESHARE_185B 1\n#include "gps_uart_config.h"\ndragon::gpsuart::Config externalGpsConfig;bool externalGpsOpen=false,externalGpsSuspended=false;')
optional = optional.replace('#define BOARD_AMOLED_175 1', '')
optional = optional.replace('char message[120]{};', 'char message[120]{};std::string header;')
optional = optional.replace('const char*,size_t n){\n   error=false;', 'const char* text,size_t n){\n   header.assign(text,n);error=false;')
optional += r"""
void receiver(){
 std::string body="GNRMC,123519,V,,,,,,,031126,,,N";uint8_t sum=0;for(char c:body)sum^=uint8_t(c);
 char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",sum);for(char c:"$"+body+tail)gps.feed(c,now);
 present=gps.receiverPresent(now);
}
int main(){
 assert(!externalGpsConfig.enabled&&externalGpsConfig.rx==44&&!wardriveAvailable());
 view=Wardrive;inputReleased=false;reconViewLoop();assert(view==Wardrive);
 inputReleased=true;reconViewLoop();assert(view==GpsSetup);view=Modes;
 assert(!wardriveStart()&&!wardriveBusy()&&transitions.empty());
 externalGpsConfig.enabled=true;assert(!wardriveStart());externalGpsOpen=true;
 assert(!wardriveAvailable()&&!wardriveStart());
 receiver();assert(wardriveAvailable()&&!located(now));
 driveLog.missing=true;assert(!wardriveStart()&&!wardriveBusy());driveLog.missing=false;
 assert(wardriveStart()&&wardriveBusy()&&wardriveActive());
 assert(driveLog.header.find("model=ESP32-S3-Touch-LCD-1.85B")!=std::string::npos);
 assert(driveLog.header.find("display=LCD,board=waveshare_185b")!=std::string::npos);
 uint8_t mac[6]={1,2,3,4,5,6};fix();record(mac,nullptr,0,"",-40,1,false,0,now,false);
 assert(wardriveSnapshot().wifi==1);
 now+=3001;assert(!wardriveAvailable()&&!located(now)&&wardriveActive());
 record(mac,nullptr,0,"",-40,1,false,0,now,false);assert(wardriveSnapshot().wifi==1&&wardriveSnapshot().noFix==1);
 view=Wardrive;reconViewLoop();assert(menuReturns==0&&wardriveActive());
 reconEndWardrive();assert(!wardriveActive()&&wardriveBusy());reconViewLoop();assert(menuReturns==0);
 wardriveLoop();reconViewLoop();assert(menuReturns==1&&!wardriveBusy()&&!driveLog.pending());
 assert(!wardriveStart());receiver();assert(wardriveAvailable());
 externalGpsSuspended=true;assert(!wardriveAvailable()&&!wardriveStart());externalGpsSuspended=false;
 assert(wardriveStart());reconEndWardrive();driveLog.failNext=true;wardriveLoop();reconViewLoop();
 assert(menuReturns==1&&!wardriveBusy()&&wardriveSnapshot().failed);
 free(seen);puts("PASS optional UART wardrive default-off/readiness gates, no-fix start, model header, GPS loss, safe End, SD failure");
}
"""
cpp=work/'optional_session.cpp';exe=work/'optional_session.exe';cpp.write_text(optional)
subprocess.run(compiler+['-UNDEBUG','-std=c++17','-O1','-Ifirmware/include',str(cpp),'-o',str(exe)],cwd=root,check=True)
subprocess.run([str(exe)],cwd=root,check=True)
