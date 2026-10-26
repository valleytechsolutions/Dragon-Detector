"""Exercise production network ownership of the external GPS UART under failures."""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
work=root/'.work'/'gps-network-tests';work.mkdir(parents=True,exist_ok=True)
driver=(root/'firmware/src/network_tools.cpp').read_text()
def function(signature):
    start=driver.index(signature);brace=driver.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(driver[end]=='{')-(driver[end]=='}');end+=1
    return driver[start:end]
source=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#define BOARD_AMOLED_175 1
#define BOARD_GPS_UART 1
struct Profile {char name[33]{};char password[64]{};};
Profile joiningProfile;
bool active=false,connected=false,resume=false,radioEnabled=true,suspended=false,standby=false,other=false;
unsigned suspendCount=0,restoreCount=0,heap=28000;
void gpsExternalSuspend(bool value){if(value==suspended)return;suspended=value;if(value)++suspendCount;else ++restoreCount;}
bool radioStandby(){return standby;}bool reconOwnsRadio(){return other;}bool reconStorageBusy(){return false;}bool bleLabOwnsRadio(){return false;}
bool valid(const Profile& p){return *p.name;}void setRadio(bool b){radioEnabled=b;}void note(const char*){}void scanStop(){}
namespace dragon::toolkit {struct {bool recording=false;} status;}
struct {unsigned getFreeHeap(){return heap;}} ESP;
int ifaceValue=1,eventValue=1;int* iface=nullptr;int* wifiEvents=nullptr;
int oldLock=0,channelLock=0;unsigned started=0,lastCheck=0,disconnectReason=0,disconnectCount=0,handledDisconnect=0,attempts=0,lastAttempt=0;
struct {int ip=0;} address;
unsigned millis(){return 1234;}
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_INVALID_STATE=1,WIFI_IF_STA=0,WIFI_MODE_NULL=0,WIFI_MODE_STA=1,WIFI_EVENT=0,ESP_EVENT_ANY_ID=0,WIFI_AUTH_WPA2_PSK=2,WIFI_AUTH_OPEN=0;
struct wifi_config_t {struct {char ssid[32]{},password[64]{};struct {int authmode=0;} threshold;struct {bool capable=false,required=false;} pmf_cfg;} sta;};
int failure=0;
int esp_netif_init(){return failure==1?-1:ESP_OK;}
int* esp_netif_create_default_wifi_sta(){return failure==2?nullptr:&ifaceValue;}
void esp_netif_destroy_default_wifi(int*){}
void wifiEvent(){}
int esp_event_handler_instance_register(int,int,void(*)(),void*,int** out){if(failure==3)return -1;*out=&eventValue;return ESP_OK;}
void esp_event_handler_instance_unregister(int,int,int*){}
void esp_wifi_disconnect(){}
int esp_wifi_set_mode(int mode){return mode==WIFI_MODE_STA&&failure==4?-1:ESP_OK;}
int esp_wifi_set_config(int,const wifi_config_t* cfg){return *cfg->sta.ssid&&failure==5?-1:ESP_OK;}
int esp_wifi_connect(){return failure==6?-1:ESP_OK;}
''' + function('void networkStop(')+function('static bool connectProfile(')+r'''
int main(){Profile p;strcpy(p.name,"Synthetic offline test");strcpy(p.password,"test-passphrase");
 other=true;assert(!connectProfile(p,false)&&!suspendCount);other=false;
 standby=true;assert(!connectProfile(p,false)&&!suspendCount);standby=false;
 heap=17999;assert(!connectProfile(p,false)&&!suspended&&suspendCount==1&&restoreCount==1);heap=22000;
 for(failure=1;failure<=6;++failure){assert(!connectProfile(p,false));assert(!suspended&&!active&&!iface&&!wifiEvents&&radioEnabled&&suspendCount==restoreCount);}
 failure=0;assert(connectProfile(p,false)&&suspended&&active&&iface&&wifiEvents&&!radioEnabled);
 unsigned before=suspendCount;assert(!connectProfile(p,false)&&suspended&&suspendCount==before);
 networkStop();assert(!active&&!iface&&!wifiEvents&&!suspended&&radioEnabled&&suspendCount==restoreCount);
 networkStop();assert(suspendCount==before&&restoreCount==before);
 standby=true;assert(connectProfile(p,true)&&suspended);networkStop();assert(!suspended);
 puts("PASS external GPS network suspension: conflicts, low heap, interface/event/Wi-Fi failures, successful lease, clock-only connection and idempotent restore");
}
'''
cpp=work/'gps_network.cpp';exe=work/'gps_network.exe';cpp.write_text(source)
subprocess.run([sys.executable,'-m','ziglang','c++','-UNDEBUG','-std=c++17','-O1',str(cpp),'-o',str(exe)],cwd=root,check=True)
subprocess.run([str(exe)],check=True)
