#include "app.h"
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <atomic>
#include "axon.h"
#include "radio_schedule.h"

bool radioEnabled=true,blePhase=false,candidatesOnly=true;
bool radarRecent=true;
int trackedContact=-1;
bool radarVisible(const Contact& c,uint32_t now){
 uint8_t flags=deviceRules(contactKey(c));
 return candidateVisible(c,now,detector,!candidatesOnly,flags&RuleIgnore,flags&RuleWatch) || (radarRecent&&recentCandidateVisible(c,now,detector,flags&RuleIgnore,flags&RuleWatch));
}
bool activeBle=false;
uint32_t radioMaxBatchUs=0,radioMaxLagMs=0,contactEvictions=0,contactCacheHits=0,contactCacheMisses=0;
uint16_t radioQueueHigh=0;
static ContactCache contactCache;
uint8_t wifiChannel=1,channelLock=0;
struct Observation { uint32_t at; uint64_t us; uint16_t len,original,advLength; int8_t rssi; uint8_t channel,kind,addressType,mac[6]; uint8_t data[1024]; };
static std::atomic<uint32_t> drops{0},axonDrops{0},axonQueued{0};
static constexpr unsigned QUEUE_SIZE=96;
static constexpr unsigned AXON_RESERVE=24;
static QueueHandle_t queue,priorityQueue,freeSlots;
static Observation* observations;
// Free/ready queues carry one-byte pool indexes. Only received bytes are copied
// in callbacks; the old implementation copied/cleared a 1 KB object each time.
static bool reserveObservation(uint8_t& index,bool priority){
  if((priority||uxQueueMessagesWaiting(freeSlots)>AXON_RESERVE)&&xQueueReceive(freeSlots,&index,0)==pdTRUE)return true;
  ++drops;if(priority)++axonDrops;return false;
}
static void enqueueObservation(uint8_t index,bool priority){
  if(xQueueSend(priority?priorityQueue:queue,&index,0)==pdTRUE){if(priority)++axonQueued;return;}
  ++drops;if(priority)++axonDrops;xQueueSend(freeSlots,&index,0);
}
static uint32_t axonBleHits=0,axonWifiHits=0,axonCompanyHits=0,axonServiceHits=0,axonOuiHits=0,axonWeakHits=0,axonRecovered=0,axonMaxLagMs=0;
void axonStatus(){
  Serial.printf("AXON_STATUS priority=%u enabled=%u minimum=%u quiet=%u sound=%u tracked=%d alerts=%lu ble=%lu wifi=%lu company=%lu service=%lu oui=%lu weak=%lu recovered=%lu queued=%lu drops=%lu lag_ms=%lu ble_ms=%lu wifi_ms=%lu\n",axonPriority,detector.enabled(Family::Axon),unsigned(detector.minimum),detector.quiet,alertSound,trackedContact,axonAlertCount,axonBleHits,axonWifiHits,axonCompanyHits,axonServiceHits,axonOuiHits,axonWeakHits,axonRecovered,axonQueued.load(),axonDrops.load(),axonMaxLagMs,dragon::RadioSchedule::BleMs,dragon::RadioSchedule::WifiMs);
}
static void countAxon(dragon::AxonClue clue,bool ble,bool payloadValid,uint32_t receivedAt){
  if(!clue.signals)return;
  if(ble)++axonBleHits;else ++axonWifiHits;
  if(clue.signals&dragon::AxonCompany)++axonCompanyHits;
  if(clue.signals&dragon::AxonService)++axonServiceHits;
  if(clue.signals&dragon::AxonOui)++axonOuiHits;
  if(clue.signals&dragon::AxonCompatibility)++axonWeakHits;
  if(!payloadValid)++axonRecovered;
  axonMaxLagMs=std::max(axonMaxLagMs,uint32_t(millis()-receivedAt));
}


static NimBLEScan* scanner;
static uint32_t lastHop=0,lastAccounting=0;
static dragon::RadioSchedule schedule;
static dragon::BleBurstMonitor bleBursts;
static dragon::WifiRetryFilter retries;
dragon::TrafficStatus trafficStatus;
static dragon::SsidMonitor ssids;
static dragon::DisconnectMonitor disconnects;
unsigned recentDisconnects(){return disconnects.recent(millis());}
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
  bool priority=dragon::inspectAxonWifi(p->payload,p->rx_ctrl.sig_len-4).registered();
  uint8_t index;if(!reserveObservation(index,priority))return;auto& o=observations[index];o.at=millis();o.us=esp_timer_get_time();o.kind=0;o.addressType=0;
  o.original=p->rx_ctrl.sig_len-4;
  // Without PCAP, a data frame only needs its maximum MAC header + LLC for
  // EAPOL and source-address clues. Management bodies remain complete.
  o.len=std::min<size_t>(o.original,type==WIFI_PKT_DATA&&!captureEnabled?48:sizeof(o.data));
  o.rssi=p->rx_ctrl.rssi;o.channel=p->rx_ctrl.channel;
  memcpy(o.data,p->payload,o.len);
  enqueueObservation(index,priority);
}
class BleCallbacks:public NimBLEScanCallbacks {
  void onDiscovered(const NimBLEAdvertisedDevice* d) override {
    // In active mode, don't lose devices which never send a scan response.
    if(activeBle && d->isScannable())onResult(d);
  }
  void onResult(const NimBLEAdvertisedDevice* d) override {
    auto addr=d->getAddress();uint8_t mac[6];for(int i=0;i<6;++i)mac[i]=addr.getBase()->val[5-i];
    const auto& payload=d->getPayload();
    bool priority=dragon::inspectAxonBle(payload.data(),payload.size(),d->getAdvLength(),mac,addr.getType()).registered();
    uint8_t index;if(!reserveObservation(index,priority))return;auto& o=observations[index];o.at=millis();o.kind=1;o.channel=0;o.us=0;o.rssi=d->getRSSI();
    memcpy(o.mac,mac,6);o.addressType=addr.getType();
    o.original=payload.size();o.advLength=d->getAdvLength();o.len=std::min<size_t>(payload.size(),sizeof(o.data));memcpy(o.data,payload.data(),o.len);
    enqueueObservation(index,priority);
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
  float angle=(contactHash(key)%360)*.0174532925f;slot->radarX=cosf(angle);slot->radarY=sinf(angle);
  slot->match={ble?Family::Ble:Family::Wifi,Confidence::None,"no specific signature"};
  contactCache.remember(key,slot-contacts.data);return *slot;
}
static void update(Contact& c,const Observation& o,const char* name,dragon::Match m){
  if(c.packets && uint32_t(o.at-c.seen)>30000){c.signal={};c.matchHits=0;c.match={c.ble?Family::Ble:Family::Wifi,Confidence::None,"expired evidence"};c.signatureAlerted=false;c.wifiAlerts=0;}
  if(o.rssi<0 && o.rssi>=-127){c.signal.add(o.rssi,o.at,detector.smoothingMs);c.rssi=c.signal.value();c.trace.add(c.rssi,o.at);}c.seen=o.at;c.channel=o.channel;++c.packets;
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
  bool fresh=c.match.confidence!=Confidence::None && dragon::signatureAlertReady(c.match,c.matchHits,c.matchSince,o.at,axonPriority);
  if((fresh || (watched&&c.packets>=2)) && uint32_t(o.at-c.seen)<30000){
    bool tracker=c.match.family==Family::Tracker||c.match.family==Family::SmartTag||c.match.family==Family::GoogleTag||c.match.family==Family::Tile||c.match.family==Family::IBeacon;
    // Group heavy tag traffic rather than announcing every spoofable address.
    if(tracker && bleBursts.alerted[3] && uint32_t(o.at-bleBursts.lastAlert[3])<30000 && !watched)historyObserve(c,c.match.family,c.match.confidence,c.match.evidence);
    else emitDetection(c,c.match.family,c.match.confidence,c.match.evidence);
  }
}
static void checkBleBurst(Contact& participant,const Observation& o,uint8_t group){
  // Ignore scan-response bytes in the fingerprint, so active scanning doesn't
  // turn one advertisement plus its response into two payload variants.
  size_t length=o.advLength?std::min<size_t>(o.advLength,o.len):o.len;
  auto result=bleBursts.observe(contactKey(participant),group,dragon::crc32(o.data,length),o.at);
  if(result.alert){
    ++participant.wifiAlerts;participant.lastWifiAlert=o.at;participant.trafficFamily=Family::BleSpam;
    snprintf(participant.trafficEvidence,sizeof(participant.trafficEvidence),"%s: %u addresses, %u payload variants / 5s; possible spam, not proven source",dragon::bleBurstName(group),result.addresses,result.variants);
    ++trafficStatus.bleAlerts;trafficStatus.lastBle=o.at;trafficStatus.bleGroup=group;trafficStatus.bleAddresses=result.addresses;trafficStatus.bleVariants=result.variants;
    emitDetection(participant,Family::BleSpam,Confidence::Medium,participant.trafficEvidence,true);
  }
}
static void process(const Observation& o){
  if(o.kind==0){
    ++wifiFrames;if(o.channel<14)++channelFrames[o.channel];
    writePcap(o.data,o.len,o.original,o.us);
    // Header evidence survives an incomplete body, but partial SSID/security
    // elements never become network or attack claims.
    bool payloadValid=false;
    auto f=dragon::parseWifiObservation(o.data,o.len,o.len<o.original&&((o.data[0]>>2)&3)==0,payloadValid);
    if(!payloadValid)++malformedFrames;if(!f.valid)return;
    countAxon(dragon::inspectAxonWifi(o.data,o.len),false,payloadValid,o.at);
    if(f.eapol)++eapolFrames;
    // Data traffic must not evict the disconnect retry cache during a burst.
    if((f.deauth||f.disassoc) && retries.duplicate(f,o.at))return;
    Contact& c=getContact(f.source,false,0,o.at);
    // Priority observations may overtake older general traffic for this key.
    if(c.packets&&int32_t(o.at-c.seen)<0)return;
    update(c,o,f.name,f.match);
    if(&c-contacts.data==trackedContact && o.channel>=1 && o.channel<=11)channelLock=o.channel;
    dragon::mergeDrone(c.drone,f.drone,o.at);
    uint8_t peer[6];
    if(ssids.observe(f,o.at,peer)){
      char mac[18],msg[128];macText(peer,mac);snprintf(msg,sizeof(msg),"Same SSID, open/protected mismatch with %s; possible evil twin or network configuration",mac);
      if(detector.enabled(Family::EvilTwin)){++c.wifiAlerts;c.lastWifiAlert=o.at;c.trafficFamily=Family::EvilTwin;strlcpy(c.trafficEvidence,msg,sizeof(c.trafficEvidence));emitDetection(c,Family::EvilTwin,Confidence::Medium,msg);}
    }
    if(f.deauth || f.disassoc){
      ++trafficStatus.disconnectFrames;
      if(disconnects.observe(o.at)){
        ++trafficStatus.disconnectAlerts;trafficStatus.lastDisconnect=o.at;trafficStatus.channel=o.channel;trafficStatus.reason=f.reason;trafficStatus.protectedFrame=f.protectedFrame;
        ++c.wifiAlerts;c.lastWifiAlert=o.at;c.trafficFamily=Family::Deauth;
        snprintf(c.trafficEvidence,sizeof(c.trafficEvidence),"%u+ disconnects / 3s; CH %u; %s reason %u; possible attack, source may be spoofed",disconnects.recent(o.at),o.channel,f.protectedFrame?"protected/unknown":"clear",f.reason);
        emitDetection(c,Family::Deauth,Confidence::Medium,c.trafficEvidence,true);
      }
    }
  }else{
    ++bleFrames;if(o.len!=o.original){++malformedFrames;return;}
    bool payloadValid=false;auto f=dragon::parseBleObservation(o.data,o.len,o.advLength,o.mac,o.addressType,payloadValid);
    if(!payloadValid)++malformedFrames;if(!f.valid)return;
    countAxon(dragon::inspectAxonBle(o.data,o.len,o.advLength,o.mac,o.addressType),true,payloadValid,o.at);
    Contact& c=getContact(o.mac,true,o.addressType,o.at);if(c.packets&&int32_t(o.at-c.seen)<0)return;
    update(c,o,f.name,f.match);dragon::mergeDrone(c.drone,f.drone,o.at);
    if(f.match.family==Family::Tracker||f.match.family==Family::SmartTag||f.match.family==Family::GoogleTag||f.match.family==Family::Tile||f.match.family==Family::IBeacon)checkBleBurst(c,o,3);
    if(f.applePopup)checkBleBurst(c,o,0);if(f.fastPair)checkBleBurst(c,o,1);if(f.swiftPair)checkBleBurst(c,o,2);
  }
}
void radioBegin(){
  observations=(Observation*)ps_malloc(QUEUE_SIZE*sizeof(Observation));
  static StaticQueue_t readyControl,priorityControl,freeControl;static uint8_t readyMemory[QUEUE_SIZE],priorityMemory[QUEUE_SIZE],freeMemory[QUEUE_SIZE];
  if(!observations){radioEnabled=false;addAlert("Radio memory error","Could not allocate packet pool");return;}
  queue=xQueueCreateStatic(QUEUE_SIZE,1,readyMemory,&readyControl);
  priorityQueue=xQueueCreateStatic(QUEUE_SIZE,1,priorityMemory,&priorityControl);
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
  setRadio(true);
  Serial.println("RADIO ready: BLE-first 2200ms / Wi-Fi 2200ms; channels 1-11; Axon reserved queue");
}
void setRadio(bool enabled){
  if(!queue || !scanner)return;
  if(!enabled && fox)setFoxTransmit(false);
  radioEnabled=enabled;scanner->stop();esp_wifi_set_promiscuous(false);
  uint32_t now=millis();lastHop=lastAccounting=now;
  bool wifiHunt=trackedContact>=0&&!contacts[trackedContact].ble;
  blePhase=enabled&&!wifiHunt;schedule.begin(now,blePhase,wifiChannel);
  if(blePhase&&!scanner->start(activeBle?1000:dragon::RadioSchedule::BleMs,false,true)){
    blePhase=false;schedule.begin(now,false,wifiChannel);addAlert("BLE scan failed","Wi-Fi listening resumed");
  }
  if(enabled&&!blePhase)esp_wifi_set_promiscuous(true);
}
void radioLoop(){
  if(!queue)return;
  radioQueueHigh=std::max<uint16_t>(radioQueueHigh,uxQueueMessagesWaiting(queue)+uxQueueMessagesWaiting(priorityQueue));
  uint64_t batchAt=esp_timer_get_time();uint8_t index;
  for(int i=0;i<64;++i){
    if(xQueueReceive(priorityQueue,&index,0)!=pdTRUE&&xQueueReceive(queue,&index,0)!=pdTRUE)break;
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
  if(blePhase && !schedule.due(now) && !scanner->isScanning())scanner->start(activeBle?1000:dragon::RadioSchedule::BleMs,false,true);
  if(blePhase)bleListenMs+=elapsed;else{wifiListenMs+=elapsed;channelDwellMs[wifiChannel]+=elapsed;}
  if(!blePhase && trackedContact<0 && schedule.due(now)){
    esp_wifi_set_promiscuous(false);blePhase=true;schedule.begin(now,true,schedule.firstChannel);
    if(!scanner->start(activeBle?1000:dragon::RadioSchedule::BleMs,false,true)){blePhase=false;schedule.begin(now,false,wifiChannel);esp_wifi_set_promiscuous(true);addAlert("BLE scan failed","Wi-Fi listening resumed");}
  }else if(blePhase && schedule.due(now)){
    scanner->stop();blePhase=false;schedule.begin(now,false,(schedule.firstChannel%11)+1);
    wifiChannel=channelLock?channelLock:schedule.firstChannel;
    esp_wifi_set_channel(wifiChannel,WIFI_SECOND_CHAN_NONE);esp_wifi_set_promiscuous(true);lastHop=now;
  }
  bool relocating=trackedContact>=0 && huntSweep(contacts[trackedContact],now);
  if(!blePhase && now-lastHop>=(relocating?180u:dragon::RadioSchedule::HopMs)){lastHop=now;uint8_t c=channelLock&&!relocating?channelLock:trackedContact>=0?(wifiChannel%11)+1:schedule.channel(now);
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
