#include "app.h"
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <atomic>

bool radioEnabled=true,blePhase=false;
uint8_t wifiChannel=1,channelLock=0;
struct Observation { uint32_t at; uint64_t us; uint16_t len,original; int8_t rssi; uint8_t channel,kind,addressType,mac[6]; uint8_t data[1024]; };
static QueueHandle_t queue;
static std::atomic<uint32_t> drops{0};
static NimBLEScan* scanner;
static uint32_t phaseStart=0,lastHop=0,lastAccounting=0;
static dragon::Burst appleBurst,fastBurst,swiftBurst;
struct Advertiser { uint8_t mac[6]{}; uint32_t at=0; uint8_t group=0; bool used=false; };
static Advertiser advertisers[96];
static uint32_t lastBleAlert[3]{}; static bool bleAlerted[3]{};
static void wifiRx(void* buffer,wifi_promiscuous_pkt_type_t type){
  if(type!=WIFI_PKT_MGMT && type!=WIFI_PKT_DATA)return;
  auto* p=static_cast<wifi_promiscuous_pkt_t*>(buffer);
  if(p->rx_ctrl.rx_state || p->rx_ctrl.sig_len<28)return;
  Observation o{};o.at=millis();o.us=esp_timer_get_time();o.kind=0;
  o.original=p->rx_ctrl.sig_len-4;o.len=std::min<size_t>(o.original,sizeof(o.data));
  o.rssi=p->rx_ctrl.rssi;o.channel=p->rx_ctrl.channel;
  memcpy(o.data,p->payload,o.len);
  if(xQueueSend(queue,&o,0)!=pdTRUE)++drops;
}
class BleCallbacks:public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    Observation o{};o.at=millis();o.kind=1;o.rssi=d->getRSSI();
    auto addr=d->getAddress();memcpy(o.mac,addr.getBase()->val,6); // NimBLE native address is little endian
    for(int i=0;i<3;++i)std::swap(o.mac[i],o.mac[5-i]);o.addressType=addr.getType();
    const auto& payload=d->getPayload();o.original=payload.size();o.len=std::min<size_t>(payload.size(),sizeof(o.data));memcpy(o.data,payload.data(),o.len);
    if(xQueueSend(queue,&o,0)!=pdTRUE)++drops;
  }
};
static BleCallbacks bleCallbacks;
static Contact& getContact(const uint8_t* mac,bool ble,uint8_t addressType,uint32_t now){
  Contact* slot=nullptr;Contact* oldest=&contacts[0];
  for(auto& c:contacts){
    if(c.used && c.ble==ble && (!ble || c.addressType==addressType) && !memcmp(c.mac,mac,6))return c;
    if(!c.used && !slot)slot=&c;
    if(uint32_t(now-c.seen)>uint32_t(now-oldest->seen))oldest=&c;
  }
  if(!slot)slot=oldest;*slot={};slot->used=true;slot->ble=ble;slot->addressType=addressType;memcpy(slot->mac,mac,6);slot->firstSeen=now;
  slot->match={ble?Family::Ble:Family::Wifi,Confidence::None,"no specific signature"};return *slot;
}
static void update(Contact& c,const Observation& o,const char* name,dragon::Match m){
  c.rssi=c.packets ? (c.rssi*3+o.rssi)/4 : o.rssi;c.seen=o.at;c.channel=o.channel;++c.packets;
  if(*name)strlcpy(c.name,name,sizeof(c.name));
  bool changed=m.confidence>c.match.confidence;
  if(changed)c.match=m;
  if(c.packets==1 || changed || uint32_t(o.at-c.lastLog)>30000){logContact(c,changed?"signature":"seen");c.lastLog=o.at;}
  if(changed && m.confidence>=Confidence::Medium){char detail[128];snprintf(detail,sizeof(detail),"%s; %s",dragon::confidenceName(m.confidence),m.evidence);addAlert(dragon::familyName(m.family),detail,c.mac);}
}
static void checkBleBurst(const Observation& o,uint8_t group){
  Advertiser* slot=nullptr;Advertiser* oldest=&advertisers[0];size_t distinct=0;
  for(auto& a:advertisers){
    if(a.used && a.group==group && !memcmp(a.mac,o.mac,6)){a.at=o.at;return;} // duplicates alone are ordinary advertising
    if((!a.used || uint32_t(o.at-a.at)>5000) && !slot)slot=&a;
    if(uint32_t(o.at-a.at)>uint32_t(o.at-oldest->at))oldest=&a;
  }
  if(!slot)slot=oldest;memcpy(slot->mac,o.mac,6);slot->at=o.at;slot->group=group;slot->used=true;
  for(auto& a:advertisers)if(a.used && a.group==group && uint32_t(o.at-a.at)<=5000)++distinct;
  if(distinct>=12 && (!bleAlerted[group] || uint32_t(o.at-lastBleAlert[group])>30000)){
    const char* names[]={"Apple popup burst","Fast Pair burst","Swift Pair burst"};char msg[128];
    snprintf(msg,sizeof(msg),"%u distinct addresses / 5s with pairing payloads; possible BLE spam, crowded areas can trigger",unsigned(distinct));
    addAlert(names[group],msg);lastBleAlert[group]=o.at;bleAlerted[group]=true;
  }
}
static void process(const Observation& o){
  if(o.kind==0){
    ++wifiFrames;if(o.channel<14)++channelFrames[o.channel];
    writePcap(o.data,o.len,o.original,o.us);
    // Truncation may cut information elements; never classify a partial beacon.
    if(o.len<o.original && ((o.data[0]>>2)&3)==0)return;
    auto f=dragon::parseWifi(o.data,o.len);if(!f.valid){++malformedFrames;return;}
    if(f.eapol)++eapolFrames;
    Contact& c=getContact(f.source,false,0,o.at);
    if(f.retry && c.sequenceKnown && c.lastSequence==f.sequence)return;
    c.lastSequence=f.sequence;c.sequenceKnown=true;
    update(c,o,f.name,f.match);
    if((f.deauth || f.disassoc) && c.deauthBurst.add(o.at,20)){
      ++c.wifiAlerts;char msg[128];snprintf(msg,sizeof(msg),"20+ deauth/disassoc frames / 5s, channel %u; source can be spoofed; intent unproven",o.channel);
      addAlert("Wi-Fi disconnect burst",msg,c.mac);
    }
  }else{
    ++bleFrames;auto f=dragon::parseBle(o.data,o.len);if(!f.valid){++malformedFrames;return;}
    Contact& c=getContact(o.mac,true,o.addressType,o.at);update(c,o,f.name,f.match);
    if(f.applePopup)checkBleBurst(o,0);if(f.fastPair)checkBleBurst(o,1);if(f.swiftPair)checkBleBurst(o,2);
  }
}
void radioBegin(){
  auto* memory=(uint8_t*)ps_malloc(32*sizeof(Observation));static StaticQueue_t control;
  if(!memory){radioEnabled=false;addAlert("Radio memory error","Could not allocate packet queue");return;}
  queue=xQueueCreateStatic(32,sizeof(Observation),memory,&control);
  wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t result=esp_wifi_init(&cfg);
  if(result==ESP_OK)result=esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if(result==ESP_OK)result=esp_wifi_set_mode(WIFI_MODE_NULL);
  if(result==ESP_OK)result=esp_wifi_start();
  if(result!=ESP_OK){radioEnabled=false;addAlert("Wi-Fi init failed",esp_err_to_name(result));return;}
  wifi_country_t country={"US",1,11,20,WIFI_COUNTRY_POLICY_MANUAL};esp_wifi_set_country(&country);
  wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filter);esp_wifi_set_promiscuous_rx_cb(wifiRx);esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE);
  NimBLEDevice::init("");scanner=NimBLEDevice::getScan();scanner->setScanCallbacks(&bleCallbacks,true);
  scanner->setActiveScan(false);scanner->setInterval(100);scanner->setWindow(85);scanner->setMaxResults(0);
  esp_wifi_set_promiscuous(true);phaseStart=lastHop=lastAccounting=millis();
  Serial.println("RADIO ready: passive Wi-Fi 7700ms / BLE 2300ms; channels 1-11");
}
void setRadio(bool enabled){
  if(!queue || !scanner)return;
  radioEnabled=enabled;scanner->stop();esp_wifi_set_promiscuous(enabled);blePhase=false;phaseStart=lastAccounting=millis();
}
void radioLoop(){
  if(!queue)return;Observation o;for(int i=0;i<24 && xQueueReceive(queue,&o,0)==pdTRUE;++i)process(o);
  queueDrops=drops.load();
  uint32_t now=millis(),elapsed=now-lastAccounting;lastAccounting=now;
  if(!radioEnabled || !scanner)return;
  if(blePhase)bleListenMs+=elapsed;else{wifiListenMs+=elapsed;channelDwellMs[wifiChannel]+=elapsed;}
  if(!blePhase && !channelLock && now-phaseStart>=7700){
    esp_wifi_set_promiscuous(false);blePhase=true;phaseStart=now;
    if(!scanner->start(2300,false,true)){blePhase=false;esp_wifi_set_promiscuous(true);addAlert("BLE scan failed","Wi-Fi listening resumed");}
  }else if(blePhase && now-phaseStart>=2300){
    scanner->stop();blePhase=false;phaseStart=now;esp_wifi_set_promiscuous(true);
  }
  if(!blePhase && now-lastHop>=350){lastHop=now;uint8_t c=channelLock?channelLock:(wifiChannel%11)+1;
    if(esp_wifi_set_channel(c,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=c;
  }
}
