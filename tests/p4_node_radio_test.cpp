// Exercise the real radio adapter with a simulated SDIO radio and ownership lease.
#include <cassert>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include "node_link.h"
#define BOARD_WAVESHARE_P4_KNOB
#define ESP_IDF_VERSION 50500
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
enum {ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=-2,pdTRUE=1,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_INTERNAL=4,ESP_NOW_MAX_DATA_LEN=250,ESP_NOW_KEY_LEN=16,WIFI_IF_STA=0,ESP_MAC_WIFI_STA=0,WIFI_EVENT=1,WIFI_EVENT_SCAN_DONE=2,WIFI_SECOND_CHAN_NONE=0,WIFI_SCAN_TYPE_ACTIVE=0};
using esp_err_t=int;using esp_event_base_t=int;using esp_event_handler_instance_t=void*;
enum wifi_mode_t {WIFI_MODE_NULL,WIFI_MODE_STA};
enum wifi_ps_type_t {WIFI_PS_NONE,WIFI_PS_MIN_MODEM};
enum wifi_auth_mode_t {WIFI_AUTH_OPEN,WIFI_AUTH_WEP,WIFI_AUTH_WPA_PSK,WIFI_AUTH_WPA2_PSK,WIFI_AUTH_WPA_WPA2_PSK,WIFI_AUTH_WPA2_ENTERPRISE,WIFI_AUTH_WPA3_PSK,WIFI_AUTH_WPA2_WPA3_PSK,WIFI_AUTH_WAPI_PSK};
enum esp_now_send_status_t {ESP_NOW_SEND_SUCCESS,ESP_NOW_SEND_FAIL};
struct esp_now_send_info_t{};
struct rx_control {int rssi;};
struct esp_now_recv_info_t {const uint8_t* src_addr;rx_control* rx_ctrl;};
struct esp_now_peer_info_t {uint8_t peer_addr[6]{},lmk[16]{};uint8_t channel{};int ifidx{};bool encrypt{};};
struct wifi_event_sta_scan_done_t {int status;};
struct wifi_scan_config_t {uint8_t channel;bool show_hidden;int scan_type;struct {struct {unsigned min,max;} active;} scan_time;};
struct wifi_ap_record_t {uint8_t bssid[6],ssid[33],primary;wifi_auth_mode_t authmode;int rssi;};
struct StaticQueue_t{};using QueueHandle_t=void*;
struct SerialStub {template<class... T>void printf(const char*,T...){}void write(const uint8_t*,size_t){}} Serial;
namespace sim {
enum Failure {None,Lease,Start,Init,Receive,SendCallback,Peer,Mac,Event,BlePrepare};
Failure failure=None;bool lease=false,station=false,espnow=false,handler=false,ready=true;
bool bleBusy=false,blePending=false;unsigned bleStarts=0,bleStops=0;
uint32_t now=1000;unsigned sends=0;std::set<void*> allocations;
int result(Failure at){return failure==at?ESP_FAIL:ESP_OK;}
}
uint32_t millis(){return sim::now;}
uint32_t esp_random(){return 1234;}
void* heap_caps_malloc(size_t n,unsigned){auto* p=malloc(n);if(p)sim::allocations.insert(p);return p;}
void* heap_caps_calloc(size_t n,size_t s,unsigned caps){auto* p=heap_caps_malloc(n*s,caps);if(p)memset(p,0,n*s);return p;}
void heap_caps_free(void* p){if(p){assert(sim::allocations.erase(p)==1);free(p);}}
QueueHandle_t xQueueCreateStatic(unsigned,unsigned,uint8_t*,StaticQueue_t* q){return q;}
int xQueueSend(QueueHandle_t,const void*,unsigned){return pdTRUE;}
int xQueueReceive(QueueHandle_t,void*,unsigned){return 0;}
void xQueueReset(QueueHandle_t){}
bool p4CoprocReady(){return sim::ready;}
bool p4RadarAcquireClockNetwork(){if(sim::failure==sim::Lease)return false;assert(!sim::lease);sim::lease=true;return true;}
void p4RadarReleaseClockNetwork(){assert(sim::lease);sim::lease=false;}
bool p4NodeBlePrepare(){assert(sim::lease);return sim::failure!=sim::BlePrepare;}
bool p4NodeBleStart(){assert(sim::lease&&!sim::bleBusy);sim::bleBusy=true;++sim::bleStarts;return true;}
bool p4NodeBleBusy(){return sim::bleBusy;}
void p4NodeBleDrain(void(*seen)(const uint8_t*,int)){
  assert(!sim::bleBusy);
  if(sim::blePending){sim::blePending=false;uint8_t mac[6]={0x11,0x22,0x33,0x44,0x55,0x66};seen(mac,-48);}
}
void p4NodeBleStop(){++sim::bleStops;sim::bleBusy=sim::blePending=false;}
bool radioEnabled=true,blePhase=false,wardriveTransition=false;
std::atomic<bool> wardriveScanning{false};
int trackedContact=7;uint8_t channelLock=11,wifiChannel=1;
uint32_t lastAccounting=0;unsigned radioTransferDepth=0;
void setRadio(bool on){radioEnabled=on;}
StaticQueue_t observationsQueue;QueueHandle_t queue=&observationsQueue;
esp_err_t esp_event_loop_create_default(){return ESP_OK;}
esp_err_t esp_event_handler_instance_register(int,int,void(*)(void*,int,int32_t,void*),void*,void** out){
  if(sim::failure==sim::Event)return ESP_FAIL;*out=&observationsQueue;sim::handler=true;return ESP_OK;
}
void esp_event_handler_instance_unregister(int,int,void*){sim::handler=false;}
esp_err_t esp_wifi_get_mode(wifi_mode_t* mode){*mode=WIFI_MODE_STA;return ESP_OK;}
esp_err_t esp_wifi_get_ps(wifi_ps_type_t* mode){*mode=WIFI_PS_NONE;return ESP_OK;}
esp_err_t esp_wifi_set_mode(wifi_mode_t){assert(sim::lease);return ESP_OK;}
esp_err_t esp_wifi_set_ps(wifi_ps_type_t){assert(sim::lease);return ESP_OK;}
esp_err_t esp_wifi_start(){assert(sim::lease);sim::station=sim::failure!=sim::Start;return sim::result(sim::Start);}
esp_err_t esp_wifi_stop(){sim::station=false;return ESP_OK;}
esp_err_t esp_wifi_set_channel(uint8_t,int){assert(sim::lease&&sim::station);return ESP_OK;}
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*,bool){assert(sim::lease&&sim::station);return ESP_OK;}
void esp_wifi_scan_stop(){}
void esp_wifi_clear_ap_list(){}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t* count){*count=0;return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t*){return ESP_FAIL;}
esp_err_t esp_wifi_get_mac(int,uint8_t* mac){memset(mac,0x22,6);return sim::result(sim::Mac);}
void esp_read_mac(uint8_t* mac,int){memset(mac,0x44,6);}
esp_err_t esp_now_init(){sim::espnow=sim::failure!=sim::Init;return sim::result(sim::Init);}
void esp_now_deinit(){sim::espnow=false;}
esp_err_t esp_now_set_pmk(const uint8_t*){return ESP_OK;}
esp_err_t esp_now_register_recv_cb(void(*)(const esp_now_recv_info_t*,const uint8_t*,int)){return sim::result(sim::Receive);}
esp_err_t esp_now_register_send_cb(void(*)(const esp_now_send_info_t*,esp_now_send_status_t)){return sim::result(sim::SendCallback);}
void esp_now_unregister_recv_cb(){}
void esp_now_unregister_send_cb(){}
bool esp_now_is_peer_exist(const uint8_t*){return false;}
esp_err_t esp_now_add_peer(const esp_now_peer_info_t*){return sim::result(sim::Peer);}
esp_err_t esp_now_mod_peer(const esp_now_peer_info_t* info){return esp_now_add_peer(info);}
void esp_now_del_peer(const uint8_t*){}
esp_err_t esp_now_send(const uint8_t*,const uint8_t* data,size_t size){
  assert(sim::lease&&sim::espnow&&size==dragon::node::FrameSize);
  assert(!memcmp(data,"ENOW",4));++sim::sends;return ESP_OK;
}
#include "../firmware/src/node_radio.inc"

void restored(size_t retained){
  assert(!radioNodeOwned()&&!sim::lease&&!sim::station&&!sim::espnow&&!sim::handler);
  assert(!sim::bleBusy&&!sim::blePending);
  assert(!wardriveTransition&&!wardriveScanning&&radioEnabled&&trackedContact==7&&channelLock==11);
  assert(sim::allocations.size()==retained);
}
int main(){
  using namespace dragon::node;
  Config config;config.family=Family::Biscuit;config.ble=false;char why[96]{};
  assert(radioNodeBegin(config,nullptr,nullptr,why,sizeof why));
  assert(radioNodeOwned()&&sim::lease&&sim::station&&!radioEnabled);
  for(unsigned i=0;i<120;++i){sim::now+=100;noderadio::loop(sim::now);}
  assert(sim::sends>=4&&radioNodeStatus().requests>=4); // Search persists beyond one request.
  radioNodeEnd();const size_t retained=sim::allocations.size();assert(retained==1);restored(retained);
  for(auto failure:{sim::Lease,sim::Event,sim::Start,sim::Init,sim::Receive,sim::SendCallback,sim::Peer,sim::Mac}){
    sim::failure=failure;
    assert(!radioNodeBegin(config,nullptr,nullptr,why,sizeof why));restored(retained);
  }
  sim::failure=sim::None;
  config.family=Family::Jcmk;config.ble=true;
  sim::failure=sim::BlePrepare;
  assert(!radioNodeBegin(config,nullptr,nullptr,why,sizeof why));restored(retained);
  sim::failure=sim::None;
  assert(radioNodeBegin(config,nullptr,nullptr,why,sizeof why));
  for(unsigned i=0;i<120;++i){sim::now+=100;noderadio::loop(sim::now);}
  assert(radioNodeStatus().requests>=4); // Both modes keep searching on-device.
  uint8_t core[6]={1,2,3,4,5,6},reply[FrameSize]={'E','N','O','W',CoreReply};
  noderadio::link->received(core,reply,sizeof reply,-42,sim::now);
  assert(radioNodeStatus().linked&&radioNodeStatus().core==Core::Jcmk);
  for(unsigned i=0;i<200&&!sim::bleBusy;++i){
    sim::now+=100;
    if(noderadio::scanning)noderadio::scanResult=0;
    noderadio::loop(sim::now);
  }
  assert(sim::bleBusy&&sim::bleStarts==1&&radioNodeStatus().phase==Phase::Ble);
  sim::now+=1000;noderadio::loop(sim::now);
  assert(radioNodeStatus().phase==Phase::Ble); // Do not cut off the companion's finite scan.
  sim::bleBusy=false;sim::blePending=true;sim::now+=1000;noderadio::loop(sim::now);
  assert(radioNodeStatus().ble==1&&radioNodeStatus().phase!=Phase::Ble);
  for(unsigned i=0;i<50;++i){
    if(noderadio::sendsOut)noderadio::reported(nullptr,ESP_NOW_SEND_FAIL);
    sim::now+=10;noderadio::loop(sim::now);
  }
  assert(radioNodeStatus().submitted==1&&radioNodeStatus().sent==0&&radioNodeStatus().failed==0);
  // Transport-only feedback must not keep a silent core alive.
  sim::now+=CoreLostMs;noderadio::loop(sim::now);
  assert(!radioNodeStatus().linked&&radioNodeStatus().losses==1);
  radioNodeEnd();restored(retained);
  assert(radioNodeBegin(config,nullptr,nullptr,why,sizeof why));
  noderadio::port.ble(true,500); // Exiting during a scan must drain it before releasing the lease.
  radioNodeEnd();restored(retained);
  config.family=Family::Biscuit;sim::ready=false;
  assert(!radioNodeBegin(config,nullptr,nullptr,why,sizeof why));restored(retained);
  sim::ready=true;uint8_t address[6]{};radioNodeMac(address);assert(address[0]==0x22);
  assert(radioNodeBegin(config,nullptr,nullptr,why,sizeof why));radioNodeEnd();restored(retained);
  puts("PASS P4 node adapter: both modes, continuous discovery, companion MAC, BLE delivery and exit, repeat sessions and nine failure cleanups");
}
