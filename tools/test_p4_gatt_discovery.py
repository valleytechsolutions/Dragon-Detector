"""Run the production GATT inspector against a deterministic peripheral on the host."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "app.h": """
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
namespace dragon { struct DeviceKey { unsigned radio,addressType; uint8_t mac[6]; }; }
struct Contact { uint32_t seen; bool connectable; };
extern Contact contacts[1];
extern bool radioEnabled;
int findContact(const dragon::DeviceKey&);
bool radioStandby(), p4CoprocBusy();
uint32_t millis();
inline size_t strlcpy(char* dst,const char* src,size_t size){return snprintf(dst,size,"%s",src);}
struct SerialPort {
    std::string text;
    void printf(const char* literal){text+=literal;}
    template<class... T> void printf(const char* format,T... fields){
        char row[512];snprintf(row,sizeof(row),format,fields...);text+=row;
    }
};
extern SerialPort Serial;
""",
    "p4_radio.h": """
bool p4RadarBluetoothAvailable();
uint8_t p4RadarScanMode();
void p4RadioWakeForGatt();
""",
    "missions.h": "bool missionActive();",
    "esp_heap_caps.h": """
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define heap_caps_calloc(n,s,c) calloc(n,s)
""",
    "freertos/event_groups.h": """
typedef unsigned EventBits_t,StaticEventGroup_t,portMUX_TYPE;
typedef unsigned* EventGroupHandle_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(lock) (void)(lock)
#define taskEXIT_CRITICAL(lock) (void)(lock)
#define pdFALSE false
#define pdMS_TO_TICKS(ms) (ms)
inline auto xEventGroupCreateStatic(StaticEventGroup_t* p){return p;}
inline auto xEventGroupSetBits(EventGroupHandle_t p,unsigned bits){return *p|=bits;}
inline auto xEventGroupClearBits(EventGroupHandle_t p,unsigned bits){return *p&=~bits;}
inline auto xEventGroupGetBits(EventGroupHandle_t p){return *p;}
unsigned xEventGroupWaitBits(EventGroupHandle_t,unsigned,bool,bool,unsigned);
""",
    "esp_gattc_api.h": """
#include <cstdint>
using esp_gatt_if_t=uint8_t;
using esp_ble_addr_type_t=uint8_t;
using esp_gattc_cb_event_t=int;
enum {ESP_OK=0,ESP_ERR_NO_MEM=0x101,ESP_GATT_OK=0,ESP_GATT_WRITE_NOT_PERMIT=3,ESP_GATT_IF_NONE=255,ESP_APP_ID_MAX=0x7fff};
enum {ESP_GATTC_REG_EVT,ESP_GATTC_OPEN_EVT,ESP_GATTC_DIS_SRVC_CMPL_EVT,ESP_GATTC_CANCEL_OPEN_EVT,
      ESP_GATTC_CLOSE_EVT,ESP_GATTC_DISCONNECT_EVT,ESP_GATTC_CONNECT_EVT};
enum {ESP_GATT_DB_PRIMARY_SERVICE,ESP_GATT_DB_SECONDARY_SERVICE,ESP_GATT_DB_CHARACTERISTIC,ESP_GATT_DB_DESCRIPTOR};
enum {ESP_UUID_LEN_16=2,ESP_UUID_LEN_32=4,ESP_UUID_LEN_128=16};
struct esp_bt_uuid_t {uint16_t len; union {uint16_t uuid16;uint32_t uuid32;uint8_t uuid128[16];}uuid;};
struct esp_gattc_db_elem_t {int type; uint16_t start_handle,attribute_handle;uint8_t properties;esp_bt_uuid_t uuid;};
struct esp_ble_gattc_cancel_open_params_t {esp_gatt_if_t gattc_if;uint8_t remote_bda[6];};
struct esp_ble_gattc_cb_param_t {
  struct {int status;uint16_t app_id;}reg;
  struct {int status,reason;uint16_t conn_id;uint8_t remote_bda[6];}open,close,cancel_open,disconnect,connect,dis_srvc_cmpl;
};
using Callback=void(*)(esp_gattc_cb_event_t,esp_gatt_if_t,esp_ble_gattc_cb_param_t*);
int esp_ble_gattc_register_callback(Callback);
int esp_ble_gattc_app_register(uint16_t);
int esp_ble_gattc_cache_refresh(uint8_t*);
int esp_ble_gattc_open(esp_gatt_if_t,uint8_t*,esp_ble_addr_type_t,bool);
int esp_ble_gattc_cancel_open(esp_ble_gattc_cancel_open_params_t*);
int esp_ble_gattc_close(esp_gatt_if_t,uint16_t);
int esp_ble_gattc_get_db(esp_gatt_if_t,uint16_t,uint16_t,uint16_t,esp_gattc_db_elem_t*,uint16_t*);
""",
    "checks.cpp": """
#include "app.h"
#include "p4_gatt.h"
#include "esp_gattc_api.h"
#include "freertos/event_groups.h"
#include <cassert>
#include <algorithm>
#ifdef NDEBUG
#error Assertions are required
#endif
Contact contacts[1]={{0,true}};
bool radioEnabled=true;
SerialPort Serial;
namespace {
enum class Scenario { Success,Capacity,Refused,Cancelled,DiscoveryTimeout,LateOpen,WrongPeer,LostLink };
Scenario scenario=Scenario::Success;
const uint8_t target[6]={0xC2,0,1,2,3,4};
uint8_t mode=2;
bool session=false;
uint32_t now=100;
int wakes,opens,closes,cancels,reads,refreshes;
Callback callback;
bool pendingCancel=false;
const uint16_t link=12;
const esp_gatt_if_t iface=4;
void opened(){
  esp_ble_gattc_cb_param_t e{};e.open.conn_id=link;memcpy(e.open.remote_bda,target,6);
  e.open.status=scenario==Scenario::Refused?1:0;callback(ESP_GATTC_OPEN_EVT,iface,&e);
}
bool wanted(){return true;}
P4GattView run(Scenario s){
  scenario=s;contacts[0].seen=now;assert(p4GattStart(target,1));
  assert(!p4GattStart(target,1));p4GattRun(wanted,true);assert(!p4GattBusy());
  P4GattView v{};p4GattSnapshot(v);assert(!v.busy&&v.addressType==1&&!memcmp(v.address,target,6));
  return v;
}
}
uint32_t millis(){return now;}
int findContact(const dragon::DeviceKey& k){return k.radio==1 && k.addressType==1 && !memcmp(k.mac,target,6)?0:-1;}
bool radioStandby(){return false;}
bool missionActive(){return session;}
bool p4CoprocBusy(){return false;}
bool p4RadarBluetoothAvailable(){return true;}
uint8_t p4RadarScanMode(){return mode;}
void p4RadioWakeForGatt(){++wakes;}
int esp_ble_gattc_register_callback(Callback cb){callback=cb;return 0;}
int esp_ble_gattc_app_register(uint16_t id){
  assert(id<=ESP_APP_ID_MAX);
  esp_ble_gattc_cb_param_t e{};e.reg.app_id=id;callback(ESP_GATTC_REG_EVT,iface,&e);return 0;
}
int esp_ble_gattc_cache_refresh(uint8_t* address){
  assert(!memcmp(address,target,6));++refreshes;return 0;
}
int esp_ble_gattc_open(esp_gatt_if_t client,uint8_t* address,esp_ble_addr_type_t type,bool direct){
  assert(client==iface && !memcmp(address,target,6)&&type==1&&direct);++opens;
  assert(refreshes==opens);
  if(scenario==Scenario::Cancelled){pendingCancel=true;return 0;}
  if(scenario==Scenario::LateOpen)return 0;
  esp_ble_gattc_cb_param_t e{};
  if(scenario!=Scenario::Refused){memcpy(e.connect.remote_bda,target,6);e.connect.conn_id=link;
      callback(ESP_GATTC_CONNECT_EVT,iface,&e);}
  opened();
  if(scenario==Scenario::LostLink){memcpy(e.disconnect.remote_bda,target,6);e.disconnect.conn_id=link;e.disconnect.reason=0x13;
    callback(ESP_GATTC_DISCONNECT_EVT,iface,&e);return 0;}
  if(scenario==Scenario::Refused || scenario==Scenario::DiscoveryTimeout)return 0;
  e.dis_srvc_cmpl.conn_id=scenario==Scenario::WrongPeer?link+1:link;
  callback(ESP_GATTC_DIS_SRVC_CMPL_EVT,iface,&e);return 0;
}
int esp_ble_gattc_cancel_open(esp_ble_gattc_cancel_open_params_t* p){
  assert(p->gattc_if==iface&&!memcmp(p->remote_bda,target,6));++cancels;
  if(scenario!=Scenario::LateOpen){esp_ble_gattc_cb_param_t e{};memcpy(e.cancel_open.remote_bda,target,6);
    callback(ESP_GATTC_CANCEL_OPEN_EVT,iface,&e);}
  return 0;
}
int esp_ble_gattc_close(esp_gatt_if_t client,uint16_t id){
  assert(client==iface&&id==link);++closes;
  esp_ble_gattc_cb_param_t e{};e.close.conn_id=id;memcpy(e.close.remote_bda,target,6);
  callback(ESP_GATTC_CLOSE_EVT,iface,&e);return 0;
}
int esp_ble_gattc_get_db(esp_gatt_if_t client,uint16_t id,uint16_t lo,uint16_t hi,esp_gattc_db_elem_t* a,uint16_t* n){
  assert(client==iface&&id==link&&lo==1&&hi==65535&&*n==65);++reads;
  *n=scenario==Scenario::Capacity?65:3;
  for(unsigned i=0;i<*n;++i){a[i].type=ESP_GATT_DB_DESCRIPTOR;a[i].attribute_handle=i+1;
    a[i].uuid.len=4;a[i].uuid.uuid.uuid32=0x12345678;}
  a[0].type=ESP_GATT_DB_PRIMARY_SERVICE;a[0].start_handle=1;a[0].uuid.len=2;a[0].uuid.uuid.uuid16=0x180f;
  a[1].type=ESP_GATT_DB_CHARACTERISTIC;a[1].uuid.len=16;a[1].properties=0x12;
  for(unsigned j=0;j<16;++j)a[1].uuid.uuid.uuid128[j]=j;
  return 0;
}
unsigned xEventGroupWaitBits(EventGroupHandle_t g,unsigned bits,bool,bool,unsigned delay){
  if(!(*g&bits)){now+=delay;if(pendingCancel){pendingCancel=false;p4GattCancel();}}
  return *g&bits;
}
extern "C" uint8_t __wrap_GATTC_Write(uint16_t,uint8_t,void*);
int main(){
  assert(__wrap_GATTC_Write(12,1,nullptr)==ESP_GATT_WRITE_NOT_PERMIT);
  uint8_t other[6]={};assert(!p4GattStart(other,1));assert(!p4GattStart(target,0));
  mode=1;assert(!p4GattStart(target,1));mode=2;
  session=true;assert(!p4GattStart(target,1));session=false;
  now=30001;assert(!p4GattStart(target,1));contacts[0].seen=now;
  contacts[0].connectable=false;assert(!p4GattStart(target,1));contacts[0].connectable=true;
  assert(opens==0&&wakes==0);auto v=run(Scenario::Success);
  assert(v.complete&&!v.truncated&&v.count==3&&reads==1&&closes==1);
  assert(v.entries[0].kind==P4GattKind::Service&&v.entries[0].handle==1&&!strcmp(v.entries[0].uuid,"180F"));
  assert(v.entries[1].kind==P4GattKind::Characteristic&&v.entries[1].properties==0x12&&
    !strcmp(v.entries[1].uuid,"0F0E0D0C-0B0A-0908-0706-050403020100"));
  assert(v.entries[2].kind==P4GattKind::Descriptor&&!strcmp(v.entries[2].uuid,"12345678"));
  p4GattPrintInventory(false);
  assert(Serial.text.find(v.entries[1].uuid)!=std::string::npos&&Serial.text.find("contact=1")!=std::string::npos);
  Serial.text.clear();p4GattPrintInventory(true);
  assert(Serial.text.find(v.entries[1].uuid)==std::string::npos&&Serial.text.find("180F")!=std::string::npos&&
         Serial.text.find("uuid=hidden")!=std::string::npos);
  v=run(Scenario::Capacity);assert(v.complete&&v.truncated&&v.count==64);
  for(auto s:{Scenario::Refused,Scenario::Cancelled,Scenario::DiscoveryTimeout,Scenario::WrongPeer,Scenario::LostLink}){
    v=run(s);assert(!v.complete&&v.count==0&&reads==2);
    Serial.text.clear();p4GattPrintStatus();
    if(s==Scenario::Refused)assert(Serial.text.find("stage=open_event")!=std::string::npos&&Serial.text.find("open=1")!=std::string::npos);
    if(s==Scenario::LostLink)assert(Serial.text.find("disconnect=19")!=std::string::npos);
    Serial.text.clear();p4GattPrintInventory(false);assert(Serial.text.find("no completed inventory")!=std::string::npos);
  }
  v=run(Scenario::LateOpen);assert(!v.complete&&cancels==2);
  assert(!p4GattStart(target,1));int before=closes;opened();assert(closes==before+1);
  v=run(Scenario::Success);assert(v.complete&&reads==3&&opens==9&&wakes==9);
  Serial.text.clear();p4GattPrintStatus();assert(Serial.text.find("stage=complete")!=std::string::npos&&Serial.text.find("raw_count=3")!=std::string::npos);
}
""",
}

with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    for name, source in FILES.items():
        file = path / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(source)
    exe = path / ("gatt.exe" if sys.platform == "win32" else "gatt")
    subprocess.run([sys.executable, "-m", "ziglang", "c++", "-std=c++17", "-Wall",
                    "-Werror", "-UNDEBUG", "-DBOARD_WAVESHARE_P4_KNOB", "-I", str(path),
                    "-I", str(ROOT / "firmware/include"), str(ROOT / "firmware/src/p4_gatt.cpp"),
                    str(path / "checks.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("GATT selection, inventory, truncation, cancellation, link loss and late-open checks passed.")
