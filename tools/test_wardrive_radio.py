"""Exercise the production asynchronous WiFi/BLE wardrive adapter with fake IDF."""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
work=root/'.work'/'wardrive-radio-tests';work.mkdir(parents=True,exist_ok=True)
source=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#define BOARD_GPS_LC76G
using esp_event_handler_instance_t=void*;using esp_event_base_t=int;using wifi_mode_t=int;using wifi_ps_type_t=int;
constexpr int ESP_OK=0,WIFI_MODE_NULL=0,WIFI_MODE_STA=1,WIFI_PS_MIN_MODEM=1,WIFI_PS_NONE=0,WIFI_EVENT=0,WIFI_EVENT_SCAN_DONE=1,WIFI_SCAN_TYPE_ACTIVE=0;
enum wifi_auth_mode_t {WIFI_AUTH_OPEN,WIFI_AUTH_WEP,WIFI_AUTH_WPA_PSK,WIFI_AUTH_WPA2_PSK,WIFI_AUTH_WPA_WPA2_PSK,WIFI_AUTH_WPA2_ENTERPRISE,WIFI_AUTH_WPA3_PSK,WIFI_AUTH_WPA2_WPA3_PSK};
struct wifi_event_sta_scan_done_t {int status;};
struct wifi_ap_record_t {uint8_t bssid[6]{},ssid[33]{};wifi_auth_mode_t authmode=WIFI_AUTH_OPEN;int rssi=-42;uint8_t primary=6;};
struct wifi_scan_config_t {bool show_hidden=false;int scan_type=-1;struct {struct {unsigned min,max;}active;}scan_time;};
enum class ScanAction {Start,Stop};
uint32_t now=0,lastAccounting=0,bleListenMs=0,wifiListenMs=0;
bool radioEnabled=false,blePhase=false,active=true,inScan=false,failStart=false,failRead=false,bleOn=false;
unsigned registered=0,starts=0,clears=0,bleStarts=0,results=0;int mode=WIFI_MODE_NULL,power=WIFI_PS_MIN_MODEM;
std::vector<wifi_ap_record_t> aps;
uint32_t millis(){return now;}bool wardriveActive(){return active;}
int esp_event_handler_instance_register(int,int,void(*)(void*,int,int,void*),void*,void** instance){*instance=(void*)1;++registered;return 0;}
int esp_event_handler_instance_unregister(int,int,void*){--registered;return 0;}
int esp_wifi_get_mode(int* m){*m=mode;return 0;}int esp_wifi_set_mode(int m){mode=m;return 0;}
int esp_wifi_get_ps(int* p){*p=power;return 0;}int esp_wifi_set_ps(int p){power=p;return 0;}
int esp_wifi_scan_stop(){inScan=false;return 0;}int esp_wifi_clear_ap_list(){aps.clear();++clears;return 0;}
bool scanCommand(ScanAction action,uint32_t duration=0){bleOn=action==ScanAction::Start;if(bleOn){assert(duration==1500);++bleStarts;}return true;}
int esp_wifi_scan_start(wifi_scan_config_t* c,bool block){assert(!block&&c->show_hidden&&c->scan_type==WIFI_SCAN_TYPE_ACTIVE&&c->scan_time.active.max==100);assert(!bleOn);++starts;if(failStart)return 1;inScan=true;return 0;}
int esp_wifi_scan_get_ap_num(uint16_t* n){*n=aps.size();return 0;}
int esp_wifi_scan_get_ap_record(wifi_ap_record_t* ap){if(failRead||aps.empty())return 1;*ap=aps.back();aps.pop_back();return 0;}
void wardriveWifiResult(const uint8_t*,const uint8_t*,size_t,const char* auth,int,uint8_t channel,uint32_t received){assert(channel>=1&&channel<=11&&received<=now);assert(!strcmp(auth,"[ESS]"));++results;}
''' + (root/'firmware/src/wardrive_radio.inc').read_text()+r'''
void step(uint32_t ms){now+=ms;driveRadioLoop(now,ms);}
void complete(unsigned count){aps.assign(count,wifi_ap_record_t{});wifi_event_sta_scan_done_t event{0};driveScanDone(nullptr,0,0,&event);inScan=false;}
int main(){
 assert(driveRadioBegin()&&registered==1&&mode==WIFI_MODE_STA&&power==WIFI_PS_NONE);
 assert(starts==1&&!bleOn);step(1000);assert(results==0&&driveScans==0);
 complete(10);step(100);assert(results==4&&aps.size()==6&&bleOn&&driveScans==1);
 step(2);assert(results==8&&aps.size()==2);step(2);assert(results==10&&aps.empty());
 step(1495);assert(starts==1);step(1);assert(starts==2&&!bleOn);
 // Empty scans are successful, not a reason to reset an uncrowded radio.
 complete(0);step(1100);assert(driveScans==2&&driveScanErrors==0&&bleOn);
 step(1500);step(8000);assert(driveScanErrors==1&&bleOn&&!inScan);
 failStart=true;step(1500);step(1500);assert(driveScanResets==1&&driveFailures==0);failStart=false;
 step(1500);complete(2);failRead=true;step(1200);assert(driveScanErrors==4&&aps.empty());failRead=false;
 step(1500);active=false;step(1);assert(driveScanPhase==DriveScanPhase::Off&&!bleOn&&!inScan);
 complete(1);assert(driveScanResult.load()==-1);aps.clear();driveRadioEnd();assert(registered==0&&mode==WIFI_MODE_NULL&&power==WIFI_PS_MIN_MODEM);
 now=0xfffffff0;active=true;assert(driveRadioBegin());step(1200);complete(1);step(1);assert(driveScans==1);step(1500);assert(driveScanPhase==DriveScanPhase::Wifi);driveRadioEnd();
 puts("PASS async wardrive sweeps, 4-record budget, BLE gap, empty scans, timeout/recovery, cancellation, radio restore and rollover");
}
'''
path=work/'radio.cpp';path.write_text(source);exe=work/'radio.exe'
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O1',str(path),'-o',str(exe)],cwd=root,check=True)
subprocess.run([str(exe)],check=True)
