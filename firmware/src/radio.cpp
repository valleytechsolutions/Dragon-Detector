#include "app.h"
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <atomic>

bool radioEnabled=true,blePhase=false,candidatesOnly=true;
int trackedContact=-1;
bool radarVisible(const Contact& c,uint32_t now){
 uint8_t flags=deviceRules(contactKey(c));
 return candidateVisible(c,now,detector,!candidatesOnly,flags&RuleIgnore,flags&RuleWatch);
}
bool activeBle=false;
uint32_t radioMaxBatchUs=0,radioMaxLagMs=0,contactEvictions=0,contactCacheHits=0,contactCacheMisses=0;
uint16_t radioQueueHigh=0;
static ContactCache contactCache;
uint8_t wifiChannel=1,channelLock=0;
struct Observation { uint32_t at; uint64_t us; uint16_t len,original,advLength; int8_t rssi; uint8_t channel,kind,addressType,mac[6]; uint8_t data[1024]; };
static std::atomic<uint32_t> drops{0};
static constexpr unsigned QUEUE_SIZE=96;
static QueueHandle_t queue,freeSlots;
static Observation* observations;
// Free/ready queues carry one-byte pool indexes. Only received bytes are copied
// in callbacks; the old implementation copied/cleared a 1 KB object each time.
static bool reserveObservation(uint8_t& index){if(xQueueReceive(freeSlots,&index,0)==pdTRUE)return true;++drops;return false;}


static NimBLEScan* scanner;
static uint32_t phaseStart=0,lastHop=0,lastAccounting=0;
struct Advertiser { uint8_t mac[6]{}; uint32_t at=0; uint8_t group=0; bool used=false; };
static Advertiser advertisers[96];
static uint32_t lastBleAlert[4]{}; static bool bleAlerted[4]{};
static dragon::SsidMonitor ssids;
static dragon::DisconnectMonitor disconnects;
static bool fox=false;
bool foxTransmit(){return fox;}
void setFoxTransmit(bool enabled){
  if(!scanner)return;
  auto* advertising=NimBLEDevice::getAdvertising();advertising->stop();fox=false;
  if(enabled){
    // Original Dragon fox protocol. Explicitly enabled, non-connectable,
    // ephemeral per-session token; never impersonates a surveillance device.
    uint32_t token=esp_random();uint8_t payload[]={1,uint8_t(token),uint8_t(token>>8),uint8_t(token>>16),uint8_t(token>>24)};
    NimBLEAdvertisementData data;data.setFlags(0x04);
    data.setServiceData(NimBLEUUID("dd040001-7114-452b-8e26-5f44c65a4b90"),payload,sizeof(payload));
    advertising->setConnectableMode(BLE_GAP_CONN_MODE_NON);advertising->setAdvertisementData(data);
    advertising->setMinInterval(160);advertising->setMaxInterval(240);fox=advertising->start();
  }
}
static void wifiRx(void* buffer,wifi_promiscuous_pkt_type_t type){
  if(type!=WIFI_PKT_MGMT && type!=WIFI_PKT_DATA)return;
  auto* p=static_cast<wifi_promiscuous_pkt_t*>(buffer);
  if(p->rx_ctrl.rx_state || p->rx_ctrl.sig_len<28)return;
  uint8_t index;if(!reserveObservation(index))return;auto& o=observations[index];o.at=millis();o.us=esp_timer_get_time();o.kind=0;o.addressType=0;
  o.original=p->rx_ctrl.sig_len-4;
  // Without PCAP, a data frame only needs its maximum MAC header + LLC for
  // EAPOL and source-address clues. Management bodies remain complete.
  o.len=std::min<size_t>(o.original,type==WIFI_PKT_DATA&&!captureEnabled?48:sizeof(o.data));
  o.rssi=p->rx_ctrl.rssi;o.channel=p->rx_ctrl.channel;
  memcpy(o.data,p->payload,o.len);
  if(xQueueSend(queue,&index,0)!=pdTRUE){++drops;xQueueSend(freeSlots,&index,0);}
}
class BleCallbacks:public NimBLEScanCallbacks {
  void onDiscovered(const NimBLEAdvertisedDevice* d) override {
    // In active mode, don't lose devices which never send a scan response.
    if(activeBle && d->isScannable())onResult(d);
  }
  void onResult(const NimBLEAdvertisedDevice* d) override {
    uint8_t index;if(!reserveObservation(index))return;auto& o=observations[index];o.at=millis();o.kind=1;o.channel=0;o.us=0;o.rssi=d->getRSSI();
    auto addr=d->getAddress();memcpy(o.mac,addr.getBase()->val,6); // NimBLE native address is little endian
    for(int i=0;i<3;++i)std::swap(o.mac[i],o.mac[5-i]);o.addressType=addr.getType();
    const auto& payload=d->getPayload();o.original=payload.size();o.advLength=d->getAdvLength();o.len=std::min<size_t>(payload.size(),sizeof(o.data));memcpy(o.data,payload.data(),o.len);
    if(xQueueSend(queue,&index,0)!=pdTRUE){++drops;xQueueSend(freeSlots,&index,0);}
  }
};
static BleCallbacks bleCallbacks;
static Contact& getContact(const uint8_t* mac,bool ble,uint8_t addressType,uint32_t now){
  dragon::DeviceKey key;memcpy(key.mac,mac,6);key.radio=ble;key.addressType=ble?addressType:0;
  int cached=contactCache.lookup(key,contacts.data);
  if(cached>=0){++contactCacheHits;return contacts[cached];}++contactCacheMisses;
  Contact* slot=nullptr;
  // Rule lookup/eviction scoring happens only when inserting into a full table.
  for(auto& c:contacts){
    if(c.used && keyOf(c)==key){contactCache.remember(key,&c-contacts.data);return c;}
    if(!c.used && !slot)slot=&c;
  }
  if(!slot){
    uint32_t best=0;
    for(auto& c:contacts){
      if(&c-contacts.data==trackedContact)continue;
      uint8_t flags=deviceRules(keyOf(c));
      uint32_t age=std::min<uint32_t>(uint32_t(now-c.seen),3600000);
      uint32_t score=age + (!(flags&RuleWatch)?4000000u:0) + (c.match.confidence==Confidence::None&&!trafficFresh(c,now)?4000000u:0);
      if(!slot||score>best){slot=&c;best=score;}
    }
    ++contactEvictions;
  }
  *slot={};slot->used=true;slot->ble=ble;slot->addressType=key.addressType;memcpy(slot->mac,mac,6);slot->firstSeen=now;
  slot->match={ble?Family::Ble:Family::Wifi,Confidence::None,"no specific signature"};
  contactCache.remember(key,slot-contacts.data);return *slot;
}
static void update(Contact& c,const Observation& o,const char* name,dragon::Match m){
  if(c.packets && uint32_t(o.at-c.seen)>15000)c.trace={};
  if(c.packets && uint32_t(o.at-c.seen)>30000){c.signal={};c.matchHits=0;c.match={c.ble?Family::Ble:Family::Wifi,Confidence::None,"expired evidence"};c.signatureAlerted=false;c.wifiAlerts=0;}
  c.signal.add(o.rssi,o.at,detector.smoothingMs);c.rssi=c.signal.value();c.trace.add(c.rssi,o.at);c.seen=o.at;c.channel=o.channel;++c.packets;
  if(*name)strlcpy(c.name,name,sizeof(c.name));
  if(c.matchHits && uint32_t(o.at-c.matchedAt)>30000){c.matchHits=0;c.signatureAlerted=false;c.match={c.ble?Family::Ble:Family::Wifi,Confidence::None,"expired evidence"};}
  bool changed=m.confidence>c.match.confidence;
  if(m.confidence!=Confidence::None && (changed || (m.family==c.match.family && m.confidence>=c.match.confidence))){
    if(changed){c.match=m;c.matchHits=0;c.signatureAlerted=false;}
    else c.match.evidence=m.evidence;
    if(!c.matchHits)c.matchSince=o.at;
    if(c.matchHits<65535)++c.matchHits;c.matchedAt=o.at;
  }
  if(detector.enabled(c.match.family) && (c.packets==1 || changed || uint32_t(o.at-c.lastLog)>30000)){logContact(c,changed?"signature":"seen");c.lastLog=o.at;}
  // Save the first valid match too: short encounters remain inspectable even
  // when they have not earned a repeat-confirmed audible alert.
  if(c.match.confidence!=Confidence::None && (c.matchHits==1 || changed))historyObserve(c,c.match.family,c.match.confidence,c.match.evidence);
  bool watched=deviceRules(contactKey(c))&RuleWatch;
  bool fresh=c.match.confidence!=Confidence::None && c.matchHits>=2 && uint32_t(o.at-c.matchSince)>=400;
  if((fresh || (watched&&c.packets>=2)) && uint32_t(o.at-c.seen)<30000){
    bool tracker=c.match.family==Family::Tracker||c.match.family==Family::SmartTag||c.match.family==Family::GoogleTag||c.match.family==Family::Tile||c.match.family==Family::IBeacon;
    // Group heavy tag traffic rather than announcing every spoofable address.
    if(tracker && bleAlerted[3] && uint32_t(o.at-lastBleAlert[3])<30000 && !watched)historyObserve(c,c.match.family,c.match.confidence,c.match.evidence);
    else emitDetection(c,c.match.family,c.match.confidence,c.match.evidence);
  }
}
static void checkBleBurst(Contact& participant,const Observation& o,uint8_t group){
  Advertiser* slot=nullptr;Advertiser* oldest=&advertisers[0];size_t distinct=0;
  for(auto& a:advertisers){
    if(a.used && a.group==group && !memcmp(a.mac,o.mac,6) && uint32_t(o.at-a.at)<=5000){a.at=o.at;return;} // duplicates alone are ordinary advertising
    if((!a.used || uint32_t(o.at-a.at)>5000) && !slot)slot=&a;
    if(uint32_t(o.at-a.at)>uint32_t(o.at-oldest->at))oldest=&a;
  }
  if(!slot)slot=oldest;memcpy(slot->mac,o.mac,6);slot->at=o.at;slot->group=group;slot->used=true;
  for(auto& a:advertisers)if(a.used && a.group==group && uint32_t(o.at-a.at)<=5000)++distinct;
  if(distinct>=12 && (!bleAlerted[group] || uint32_t(o.at-lastBleAlert[group])>30000)){
    ++participant.wifiAlerts;participant.lastWifiAlert=o.at;participant.trafficFamily=Family::BleSpam;
    emitDetection(participant,Family::BleSpam,Confidence::Medium,"12+ addresses / 5s; participant, not proven source; spoofing/crowding possible",true);
    lastBleAlert[group]=o.at;bleAlerted[group]=true;
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
    if(&c-contacts.data==trackedContact && o.channel>=1 && o.channel<=11)channelLock=o.channel;
    dragon::mergeDrone(c.drone,f.drone,o.at);
    uint8_t peer[6];
    if(ssids.observe(f,o.at,peer)){
      char mac[18],msg[128];macText(peer,mac);snprintf(msg,sizeof(msg),"Same SSID, open/protected mismatch with %s; possible evil twin or network configuration",mac);
      if(detector.enabled(Family::EvilTwin)){++c.wifiAlerts;c.lastWifiAlert=o.at;c.trafficFamily=Family::EvilTwin;emitDetection(c,Family::EvilTwin,Confidence::Medium,msg);}
    }
    if((f.deauth || f.disassoc) && disconnects.observe(o.at) && detector.enabled(Family::Deauth)){
      ++c.wifiAlerts;c.lastWifiAlert=o.at;c.trafficFamily=Family::Deauth;char msg[128];snprintf(msg,sizeof(msg),"6+ disconnect frames / 3s; last CH %u; address is a participant, spoofing/configuration possible",o.channel);
      emitDetection(c,Family::Deauth,Confidence::Medium,msg,true);
    }
  }else{
    ++bleFrames;if(o.len!=o.original){++malformedFrames;return;}auto f=dragon::parseBle(o.data,o.len,o.advLength);if(!f.valid){++malformedFrames;return;}
    f.match=dragon::addressHint(o.mac,o.addressType==0,f.match);
    Contact& c=getContact(o.mac,true,o.addressType,o.at);update(c,o,f.name,f.match);dragon::mergeDrone(c.drone,f.drone,o.at);
    if(f.match.family==Family::Tracker||f.match.family==Family::SmartTag||f.match.family==Family::GoogleTag||f.match.family==Family::Tile||f.match.family==Family::IBeacon)checkBleBurst(c,o,3);
    if(f.applePopup)checkBleBurst(c,o,0);if(f.fastPair)checkBleBurst(c,o,1);if(f.swiftPair)checkBleBurst(c,o,2);
  }
}
void radioBegin(){
  observations=(Observation*)ps_malloc(QUEUE_SIZE*sizeof(Observation));
  static StaticQueue_t readyControl,freeControl;static uint8_t readyMemory[QUEUE_SIZE],freeMemory[QUEUE_SIZE];
  if(!observations){radioEnabled=false;addAlert("Radio memory error","Could not allocate packet pool");return;}
  queue=xQueueCreateStatic(QUEUE_SIZE,1,readyMemory,&readyControl);
  freeSlots=xQueueCreateStatic(QUEUE_SIZE,1,freeMemory,&freeControl);
  for(uint8_t i=0;i<QUEUE_SIZE;++i)xQueueSend(freeSlots,&i,0);
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
  scanner->setActiveScan(activeBle);scanner->setInterval(100);scanner->setWindow(85);scanner->setMaxResults(activeBle?96:0);
  esp_wifi_set_promiscuous(true);phaseStart=lastHop=lastAccounting=millis();
  Serial.println("RADIO ready: passive Wi-Fi 3300ms / BLE 3300ms; channels 1-11");
}
void setRadio(bool enabled){
  if(!queue || !scanner)return;
  if(!enabled && fox)setFoxTransmit(false);
  radioEnabled=enabled;scanner->stop();esp_wifi_set_promiscuous(enabled);blePhase=false;phaseStart=lastAccounting=millis();
}
void radioLoop(){
  if(!queue)return;
  radioQueueHigh=std::max<uint16_t>(radioQueueHigh,uxQueueMessagesWaiting(queue));
  uint64_t batchAt=esp_timer_get_time();uint8_t index;
  for(int i=0;i<64 && xQueueReceive(queue,&index,0)==pdTRUE;++i){
    radioMaxLagMs=std::max(radioMaxLagMs,uint32_t(millis()-observations[index].at));
    process(observations[index]);xQueueSend(freeSlots,&index,0);
    if(esp_timer_get_time()-batchAt>=4000)break; // give touch/audio/render a turn
  }
  radioMaxBatchUs=std::max<uint32_t>(radioMaxBatchUs,esp_timer_get_time()-batchAt);
  queueDrops=drops.load();
  uint32_t now=millis(),elapsed=now-lastAccounting;lastAccounting=now;
  if(!radioEnabled || !scanner)return;
  if(trackedContact>=0 && contacts[trackedContact].ble){
    if(!blePhase){esp_wifi_set_promiscuous(false);blePhase=true;scanner->start(activeBle?1000:10000,false,true);}
    if(!scanner->isScanning())scanner->start(activeBle?1000:10000,false,true);
    bleListenMs+=elapsed;return;
  }
  if(blePhase && now-phaseStart<3300 && !scanner->isScanning())scanner->start(activeBle?1000:3300,false,true);
  if(blePhase)bleListenMs+=elapsed;else{wifiListenMs+=elapsed;channelDwellMs[wifiChannel]+=elapsed;}
  if(!blePhase && trackedContact<0 && now-phaseStart>=3300){
    esp_wifi_set_promiscuous(false);blePhase=true;phaseStart=now;
    if(!scanner->start(activeBle?1000:3300,false,true)){blePhase=false;esp_wifi_set_promiscuous(true);addAlert("BLE scan failed","Wi-Fi listening resumed");}
  }else if(blePhase && now-phaseStart>=3300){
    scanner->stop();blePhase=false;phaseStart=now;esp_wifi_set_promiscuous(true);
  }
  bool relocating=trackedContact>=0 && huntSweep(contacts[trackedContact],now);
  if(!blePhase && now-lastHop>=(relocating?180u:350u)){lastHop=now;uint8_t c=channelLock&&!relocating?channelLock:(wifiChannel%11)+1;
    if(esp_wifi_set_channel(c,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=c;
  }
}

void trackContact(int index){
 if(index<0 || index>=int(MAX_CONTACTS) || !contacts[index].used){trackedContact=-1;channelLock=0;setRadio(true);return;}
 trackedContact=index;channelLock=contacts[index].ble?0:contacts[index].channel;setRadio(true);
 if(channelLock && esp_wifi_set_channel(channelLock,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=channelLock;
}
int findContact(const dragon::DeviceKey& key){for(size_t i=0;i<MAX_CONTACTS;++i)if(contacts[i].used&&contactKey(contacts[i])==key)return i;return -1;}
bool huntDevice(const dragon::DeviceKey& key,uint8_t channel){
 int i=findContact(key);
 if(i<0){auto& c=getContact(key.mac,key.radio,key.addressType,millis());c.seen=millis()-60000;c.channel=channel;i=&c-contacts.data;}
 trackContact(i);return trackedContact==i;
}

void setActiveBle(bool enabled){
  activeBle=enabled;detectorSave();
  if(!scanner)return;
  bool listening=radioEnabled;setRadio(false);scanner->clearResults();
  scanner->setActiveScan(enabled);scanner->setMaxResults(enabled?96:0);setRadio(listening);
}
