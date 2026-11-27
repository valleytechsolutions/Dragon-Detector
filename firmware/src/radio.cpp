#include "app.h"
#include "notebook.h"
#include "ble_lab.h"
#include "recon.h"
#include "recipes.h"
#include "missions.h"
#include "wardrive.h"
#include "csi_monitor.h"
#include "node_mode.h"
#include "background_radio.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <new>
#include <atomic>
#ifdef BOARD_WAVESHARE_P4_KNOB
#include "p4_radio.h"
#include "p4_wifi_metadata.h"
#else
#include <NimBLEDevice.h>
#include <nimble/porting/nimble/include/nimble/nimble_port.h>
#endif
#include "axon.h"
#include "radio_schedule.h"

bool radioEnabled=true,blePhase=false,candidatesOnly=true;
static bool standby=false;
#ifdef BOARD_WAVESHARE_P4_KNOB
static bool p4RadarRequested=true;
static p4_radar_wifi_snapshot_t p4LastSurvey{};
static bool p4HaveSurvey=false;
const p4_radar_wifi_snapshot_t* p4LatestWifiSurvey(){return p4HaveSurvey?&p4LastSurvey:nullptr;}
bool p4RadarChannelSummary(uint16_t counts[P4_RADAR_CHANNELS],int8_t strongest[P4_RADAR_CHANNELS],uint32_t* completedMs){
  if(!p4HaveSurvey||!counts||!strongest||!completedMs)return false;
  memcpy(counts,p4LastSurvey.channel_counts,sizeof(p4LastSurvey.channel_counts));
  memcpy(strongest,p4LastSurvey.channel_peaks,sizeof(p4LastSurvey.channel_peaks));
  *completedMs=p4LastSurvey.finished_ms;
  return true;
}
#endif
static bool wardriveTransition=false;
bool radioStandby(){return standby;}
void setRadioStandby(bool enabled){
  if(standby==enabled)return;
#ifdef BOARD_WAVESHARE_P4_KNOB
  const bool requested=p4RadarRequested;
#endif
  standby=enabled;
  if(enabled){
    recipeStop();
#ifndef BOARD_RADAR_LITE
    missionStop();
#endif
    reconStop();bleLabStop();setCapture(false);
  }
  // Utility pages suspend receive; a manually paused radar stays paused.
#ifdef BOARD_WAVESHARE_P4_KNOB
  setRadio(requested);
#else
  setRadio(!enabled);
#endif
}
bool radarRecent=true;
int trackedContact=-1;
static bool radarVisibleWith(const Contact& c,uint32_t now,uint8_t flags){
#ifdef BOARD_WAVESHARE_P4_KNOB
 const uint8_t scannerMode=p4RadarScanMode();
 if ((scannerMode==1 && c.ble) || (scannerMode==2 && !c.ble)) return false;
#endif
 if(!reconAccepts(c.match.family,c.ble))return false;
 return candidateVisible(c,now,detector,!candidatesOnly||reconAll(),flags&RuleIgnore,flags&RuleWatch) || (radarRecent&&recentCandidateVisible(c,now,detector,flags&RuleIgnore,flags&RuleWatch));
}
bool radarVisible(const Contact& c,uint32_t now){return radarVisibleWith(c,now,deviceRules(contactKey(c)));}
bool activeBle=false;
uint32_t radioHandled=0,radioLateHandled=0,radioLongBatches=0,advertisedFrames=0,advertisedMismatch=0;
uint32_t radioMaxBatchUs=0,radioMaxLagMs=0,contactEvictions=0,contactCacheHits=0,contactCacheMisses=0;
uint16_t radioQueueHigh=0;
static ContactCache contactCache;
uint8_t wifiChannel=1,channelLock=0;
struct Observation { uint32_t at; uint64_t us; uint16_t len,original,advLength; int8_t rssi; uint8_t channel,kind,addressType,mac[6]; uint8_t data[1024]; };
static std::atomic<bool> wardriveScanning{false};
static std::atomic<uint32_t> drops{0},axonDrops{0},axonQueued{0};
#ifdef BOARD_M5DIAL
static constexpr unsigned QUEUE_SIZE=16;
static constexpr unsigned AXON_RESERVE=4;
#else
// 240 slots (about 250 KB of PSRAM) ride out a slow radar frame; indexes stay
// one byte. The Axon reserve keeps its quarter share.
static constexpr unsigned QUEUE_SIZE=240;
static constexpr unsigned AXON_RESERVE=60;
#endif
static_assert(QUEUE_SIZE<=255,"pool indexes are uint8_t");
unsigned radioPoolSize(){return QUEUE_SIZE;}
static QueueHandle_t queue,priorityQueue,freeSlots;
static Observation* observations;
// Free/ready queues carry one-byte pool indexes. Only received bytes are copied
// in callbacks; the old implementation copied/cleared a 1 KB object each time.
static bool reserveObservation(uint8_t& index,bool priority){
  if((wardriveScanning||priority||uxQueueMessagesWaiting(freeSlots)>AXON_RESERVE)&&xQueueReceive(freeSlots,&index,0)==pdTRUE)return true;
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
static void countAxon(dragon::AxonEvidence clue,bool ble,bool payloadValid,uint32_t receivedAt){
  if(!clue.signals)return;
  if(ble)++axonBleHits;else ++axonWifiHits;
  if(clue.signals&dragon::AxonCompany)++axonCompanyHits;
  if(clue.signals&dragon::AxonService)++axonServiceHits;
  if(clue.signals&dragon::AxonOui)++axonOuiHits;
  if(clue.signals&(dragon::AxonCompatibility|dragon::AxonName|dragon::AxonResearch))++axonWeakHits;
  if(!payloadValid)++axonRecovered;
  axonMaxLagMs=std::max(axonMaxLagMs,uint32_t(millis()-receivedAt));
}


#ifndef BOARD_WAVESHARE_P4_KNOB
static NimBLEScan* scanner;
static dragon::WifiSweep wifiSweep;
static bool configuredActive=false;
// NimBLE 2.3.6 stop/start/clearResults can free the advertised device that its
// host task is still delivering. Serialize scanner mutations on that same host
// queue, including ordinary radio phase changes and USB transfer pauses.
enum class ScanAction { Start, Stop, Configure };
static struct {
  ble_npl_event event;
  StaticSemaphore_t semaphore;
  SemaphoreHandle_t done;
  ScanAction action;
  uint32_t duration;
  bool active,ok;
} scanRequest;
static void scanOnHost(ble_npl_event*){
  if(scanRequest.action==ScanAction::Start)scanRequest.ok=scanner->start(scanRequest.duration,false,true);
  else if(scanRequest.action==ScanAction::Stop)scanRequest.ok=scanner->stop();
  else {scanner->clearResults();scanner->setActiveScan(scanRequest.active);configuredActive=scanRequest.active;scanner->setMaxResults(0);scanRequest.ok=true;}
  xSemaphoreGive(scanRequest.done);
}
// Only the application task submits requests; no BLE callback waits on itself.
static bool scanCommand(ScanAction action,uint32_t duration=0,bool active=false){
  scanRequest.action=action;scanRequest.duration=duration;scanRequest.active=active;
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&scanRequest.event);
  xSemaphoreTake(scanRequest.done,portMAX_DELAY);
  return scanRequest.ok;
}
#endif
static uint32_t lastHop=0,lastAccounting=0;
static bool surveyListen=false; // see radioSurveyListen()
static dragon::RadioSchedule schedule;
static dragon::BleBurstMonitor bleBursts;
static dragon::WifiRetryFilter retries;
dragon::TrafficStatus trafficStatus;
dragon::NearbyRadios nearby;
static dragon::SsidMonitor ssids;
static dragon::DisconnectMonitor disconnects;
unsigned recentDisconnects(){return disconnects.recent(millis());}
static bool fox=false;
bool foxTransmit(){return fox;}
#ifdef BOARD_WAVESHARE_P4_KNOB
void setFoxTransmit(bool) {}
#else
void setFoxTransmit(bool enabled){
  if(enabled&&standby)return;
  if(bleLabOwnsRadio()||reconOwnsRadio())return;
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
#endif
static void wifiRx(void* buffer,wifi_promiscuous_pkt_type_t type){
  if(type!=WIFI_PKT_MGMT && type!=WIFI_PKT_DATA)return;
  auto* p=static_cast<wifi_promiscuous_pkt_t*>(buffer);
  if(p->rx_ctrl.rx_state || p->rx_ctrl.sig_len<28)return;
#ifdef BOARD_WAVESHARE_P4_KNOB
  static_assert(sizeof(wifi_pkt_rx_ctrl_t)==28,"Check the extconn RX layout after SDK changes");
  const uint8_t packetChannel=dragon::p4::wifiPrimaryChannel(&p->rx_ctrl);
  if(packetChannel<1||packetChannel>14||p->rx_ctrl.sig_len>2500)return;
#else
  const uint8_t packetChannel=p->rx_ctrl.channel;
#endif
#ifndef BOARD_RADAR_LITE
  // Room motion counts the selected AP's PHY classes; detections still queue.
  if(csiActive())csiFrame(p);
#endif
  if(wardriveScanning&&(type!=WIFI_PKT_MGMT||(p->payload[0]!=0x80&&p->payload[0]!=0x50)))return;
  bool priority=!wardriveScanning&&dragon::inspectAxonWifi(p->payload,p->rx_ctrl.sig_len-4).detected();
  uint8_t index;if(!reserveObservation(index,priority))return;auto& o=observations[index];o.at=millis();o.us=esp_timer_get_time();o.kind=0;o.addressType=0;
  o.original=p->rx_ctrl.sig_len-4;
  // Without PCAP, a data frame only needs its maximum MAC header + LLC for
  // EAPOL and source-address clues. Management bodies remain complete.
  bool full=captureEnabled||reconRecording()||type!=WIFI_PKT_DATA;
#ifndef BOARD_RADAR_LITE
  // Preserve EAPOL bodies for message/PMKID analysis even outside a capture.
  full=full||dragon::parseWifi(p->payload,std::min<size_t>(o.original,48)).eapol;
#endif
  o.len=std::min<size_t>(o.original,full?sizeof(o.data):48);
  o.rssi=p->rx_ctrl.rssi;o.channel=packetChannel;
  memcpy(o.data,p->payload,o.len);
  enqueueObservation(index,priority);
}
#ifdef BOARD_WAVESHARE_P4_KNOB
void p4ReceiveWifiFrame(void* buffer,wifi_promiscuous_pkt_type_t type){wifiRx(buffer,type);}
#endif
#ifndef BOARD_WAVESHARE_P4_KNOB
class BleCallbacks:public NimBLEScanCallbacks {
  void onDiscovered(const NimBLEAdvertisedDevice* d) override {
    // In active mode, don't lose devices which never send a scan response.
    if(configuredActive && d->isScannable())onResult(d);
  }
  void onResult(const NimBLEAdvertisedDevice* d) override {
    auto addr=d->getAddress();uint8_t mac[6];for(int i=0;i<6;++i)mac[i]=addr.getBase()->val[5-i];
    const auto& payload=d->getPayload();
    bool priority=!wardriveScanning&&dragon::inspectAxonBle(payload.data(),payload.size(),d->getAdvLength(),mac,addr.getType()).detected();
    uint8_t index;if(!reserveObservation(index,priority))return;auto& o=observations[index];o.at=millis();o.kind=1;o.channel=0;o.us=0;o.rssi=d->getRSSI();
    memcpy(o.mac,mac,6);o.addressType=addr.getType();
    o.original=payload.size();o.advLength=d->getAdvLength();o.len=std::min<size_t>(payload.size(),sizeof(o.data));memcpy(o.data,payload.data(),o.len);
    enqueueObservation(index,priority);
  }
};
static BleCallbacks bleCallbacks;
#endif
static Contact* getContact(const uint8_t* mac,bool ble,uint8_t addressType,uint32_t now,dragon::Match incoming){
  dragon::DeviceKey key;memcpy(key.mac,mac,6);key.radio=ble;key.addressType=ble?addressType:0;
  int cached=contactCache.lookup(key,contacts.data);
  if(cached>=0){++contactCacheHits;return &contacts[cached];}++contactCacheMisses;
  Contact* slot=nullptr;
  // Rule lookup/eviction scoring happens only when inserting into a full table.
  for(auto& c:contacts){
    if(c.used && keyOf(c)==key){contactCache.remember(key,&c-contacts.data);return &c;}
    if(!c.used && !slot)slot=&c;
  }
  if(!slot){
    uint32_t best=0;
    for(auto& c:contacts){
      if(&c-contacts.data==trackedContact)continue;
      uint8_t flags=deviceRules(keyOf(c));
      uint32_t score=evictionScore(c,now,flags&RuleWatch,radarVisibleWith(c,now,flags),incoming.family);
      if(score>best){slot=&c;best=score;}
    }
    if(!slot)return nullptr;
    ++contactEvictions;
  }
  *slot={};slot->used=true;slot->ble=ble;slot->addressType=key.addressType;memcpy(slot->mac,mac,6);slot->firstSeen=now;
  float angle=(contactHash(key)%360)*.0174532925f;slot->radarX=cosf(angle);slot->radarY=sinf(angle);
  slot->match={ble?Family::Ble:Family::Wifi,Confidence::None,"no specific signature"};
  contactCache.remember(key,slot-contacts.data);return slot;
}
static bool noteSignature(Contact& c,dragon::Match m,uint32_t at){
  if(c.matchHits && uint32_t(at-c.matchedAt)>30000){c.matchHits=0;c.signatureAlerted=false;c.match={c.ble?Family::Ble:Family::Wifi,Confidence::None,"expired evidence"};}
  bool changed=dragon::strongerMatch(m,c.match);
  if(m.confidence!=Confidence::None && (changed || (m.family==c.match.family && m.confidence>=c.match.confidence))){
    if(changed){c.match=m;c.matchHits=0;c.signatureAlerted=false;}
    else c.match.evidence=m.evidence;
    if(!c.matchHits)c.matchSince=at;
    if(c.matchHits<65535)++c.matchHits;c.matchedAt=at;
  }
  return changed;
}
struct ContactSample {
  uint32_t at;
  int8_t rssi;
  uint8_t channel;
  uint32_t reports;
};
static void update(Contact& c,const ContactSample& o,const char* name,dragon::Match m,uint8_t advertised=0,bool beacon=false){
  const bool first=c.packets==0;
  if(c.packets && uint32_t(o.at-c.seen)>30000){c.signal={};c.advChannel=0;c.matchHits=0;c.match={c.ble?Family::Ble:Family::Wifi,Confidence::None,"expired evidence"};c.signatureAlerted=false;c.wifiAlerts=0;}
  if(o.rssi<0 && o.rssi>=-127){c.signal.add(o.rssi,o.at,detector.smoothingMs);c.rssi=c.signal.value();c.trace.add(c.rssi,o.at);}c.seen=o.at;noteChannel(c,o.channel,advertised,beacon);c.packets+=std::min(UINT32_MAX-c.packets,o.reports);
  visitTouch(keyOf(c),o.at);
  if(*name)strlcpy(c.name,name,sizeof(c.name));
  const bool changed=noteSignature(c,m,o.at);
  if(c.match.family!=Family::Adult && detector.enabled(c.match.family) && (first || changed || uint32_t(o.at-c.lastLog)>30000)){logContact(c,changed?"signature":"seen");c.lastLog=o.at;}
  // Save the first valid match too: short encounters remain inspectable even
  // when they have not earned a repeat-confirmed audible alert.
  if(c.match.confidence!=Confidence::None && (c.matchHits==1 || changed))historyObserve(c,c.match.family,c.match.confidence,c.match.evidence);
  bool watched=deviceRules(contactKey(c))&RuleWatch;
  bool fresh=c.match.confidence!=Confidence::None && dragon::signatureAlertReady(c.match,c.matchHits,c.matchSince,o.at,axonPriority);
  if(fresh || (watched&&c.packets>=2)){
    // One alert decision per evidence run (reset by expiry, a new family or a
    // settings/rule/mode change). The visit ledger decides across runs and
    // eviction, so a repeat decision stays silent.
    if(!dragon::alertDecisionDue(c.signatureAlerted,c.alertRevision,alertRevision())){historyRefresh(c,c.match.family,c.match.confidence,c.match.evidence);return;}
    // Group heavy tag traffic rather than announcing every spoofable address.
    if(dragon::trackerFamily(c.match.family) && bleBursts.alerted[3] && uint32_t(o.at-bleBursts.lastAlert[3])<30000 && !watched)historyRefresh(c,c.match.family,c.match.confidence,c.match.evidence);
    else emitDetection(c,c.match.family,c.match.confidence,c.match.evidence);
  }
}
static void observeNearby(const Contact& c){
  if(c.rssi>=nearby.exitDbm()&&!(deviceRules(contactKey(c))&RuleIgnore))nearby.observe(contactHash(keyOf(c)),c.rssi,dragon::rotatingAddress(c.mac,c.addressType));
}
static void checkBleBurst(Contact& participant,uint8_t group,const uint8_t* advertisement,size_t length,uint32_t at){
  auto result=bleBursts.observe(contactKey(participant),group,dragon::crc32(advertisement,length),at);
  if(result.alert){
    ++participant.wifiAlerts;participant.lastWifiAlert=at;participant.trafficFamily=Family::BleSpam;
    snprintf(participant.trafficEvidence,sizeof(participant.trafficEvidence),"%s: %u addresses, %u payload variants / 5s; possible spam, not proven source",dragon::bleBurstName(group),result.addresses,result.variants);
    ++trafficStatus.bleAlerts;trafficStatus.lastBle=at;trafficStatus.bleGroup=group;trafficStatus.bleAddresses=result.addresses;trafficStatus.bleVariants=result.variants;
    const dragon::Incident burst{dragon::IncidentKind::BleBurst,group,dragon::BleBurstQuietMs};
    emitDetection(participant,Family::BleSpam,Confidence::Medium,participant.trafficEvidence,&burst);
  }
}
static void checkBleActivity(Contact& c,const dragon::BleFrame& f,const uint8_t* payload,size_t length,uint32_t at){
  const auto family=f.match.family;
  if(family==Family::Tracker || family==Family::SmartTag || family==Family::GoogleTag || family==Family::Tile || family==Family::IBeacon)
    checkBleBurst(c,3,payload,length,at);
  if(f.applePopup)checkBleBurst(c,0,payload,length,at);
  if(f.fastPair)checkBleBurst(c,1,payload,length,at);
  if(f.swiftPair)checkBleBurst(c,2,payload,length,at);
}
static void reportSsidConflict(Contact& c,const uint8_t* ssid,uint8_t length,bool secured,const uint8_t* peer,uint32_t at){
  if(!detector.enabled(Family::EvilTwin))return;
  char mac[18];macText(peer,mac);
  ++trafficStatus.ssidConflicts;
  ++c.wifiAlerts;c.lastWifiAlert=at;c.trafficFamily=Family::EvilTwin;
  snprintf(c.trafficEvidence,sizeof(c.trafficEvidence),"Same SSID, open/protected mismatch with %s; possible evil twin or network configuration",mac);
  const dragon::Incident pair{dragon::IncidentKind::EvilTwin,dragon::ssidConflictId(ssid,length,secured?peer:c.mac),dragon::EvilTwinQuietMs};
  emitDetection(c,Family::EvilTwin,Confidence::Medium,c.trafficEvidence,&pair);
}
#ifndef BOARD_WAVESHARE_P4_KNOB
namespace noderadio {void bleSeen(const uint8_t*,int);extern bool owned;}
#endif
static void process(const Observation& o){
  // A wardriving node only counts Bluetooth addresses; nothing reaches the detector.
#ifndef BOARD_WAVESHARE_P4_KNOB
  if(noderadio::owned){if(o.kind==1)noderadio::bleSeen(o.mac,o.rssi);return;}
#endif
  if(wardriveBusy()){
    if(wardriveActive()){
      if(o.kind==0)wardriveWifi(o.data,o.len,o.rssi,o.channel,o.at);
      else if(o.len==o.original)wardriveBle(o.mac,o.addressType,o.rssi,o.data,o.len,o.at);
    }
    return;
  }
  if(o.kind==0){
    reconWifi(o.data,o.len,o.original,o.us,o.rssi,o.channel);
    if(reconOwnsRadio()||surveyListen)return;
    ++wifiFrames;if(o.channel<14)++channelFrames[o.channel];
    writePcap(o.data,o.len,o.original,o.us);
    // Header evidence survives an incomplete body, but partial SSID/security
    // elements never become network or attack claims.
    bool payloadValid=false;
    auto f=dragon::parseWifiObservation(o.data,o.len,o.len<o.original&&((o.data[0]>>2)&3)==0,payloadValid);
    if(!payloadValid)++malformedFrames;if(!f.valid)return;
    if(payloadValid)notebookWifi(o.data,o.len,f,o.rssi,o.channel,o.at);
    countAxon(dragon::inspectAxonWifi(o.data,o.len),false,payloadValid,o.at);
    if(f.eapol)++eapolFrames;
    // Data traffic must not evict the disconnect retry cache during a burst.
    if((f.deauth||f.disassoc) && retries.duplicate(f,o.at))return;
    auto* contact=getContact(f.source,false,0,o.at,f.match);if(!contact)return;Contact& c=*contact;
    // Priority observations may overtake older general traffic for this key.
    if(c.packets&&int32_t(o.at-c.seen)<0)return;
    if(radarChannel(f.channel)){++advertisedFrames;if(f.channel!=o.channel)++advertisedMismatch;}
    update(c,{o.at,o.rssi,o.channel,1},f.name,f.match,f.channel,f.beacon);
    if(&c-contacts.data==trackedContact && radarChannel(c.channel))channelLock=c.channel;
    dragon::mergeDrone(c.drone,f.drone,o.at);
    uint8_t peer[6];
    if(ssids.observe(f,o.at,peer))reportSsidConflict(c,f.ssid,f.ssidLength,f.security!=0,peer,o.at);
    if(f.deauth || f.disassoc){
      ++trafficStatus.disconnectFrames;
      if(disconnects.observe(o.at)){
        ++trafficStatus.disconnectAlerts;trafficStatus.lastDisconnect=o.at;trafficStatus.channel=o.channel;trafficStatus.reason=f.reason;trafficStatus.protectedFrame=f.protectedFrame;
        ++c.wifiAlerts;c.lastWifiAlert=o.at;c.trafficFamily=Family::Deauth;
        snprintf(c.trafficEvidence,sizeof(c.trafficEvidence),"%u+ disconnects / 3s; CH %u; %s reason %u; possible attack, source may be spoofed",disconnects.recent(o.at),o.channel,f.protectedFrame?"protected/unknown":"clear",f.reason);
        const dragon::Incident burst{dragon::IncidentKind::Deauth,dragon::deauthId(f.bssid,o.channel),dragon::DeauthQuietMs};
        emitDetection(c,Family::Deauth,Confidence::Medium,c.trafficEvidence,&burst);
      }
    }
  }else{
    reconBle(o.mac,o.addressType,o.rssi,o.data,o.len,o.advLength,o.at);
    if(reconOwnsRadio()||surveyListen)return;
    ++bleFrames;bleLabObserve(o.mac,o.addressType,o.rssi,o.data,o.len,o.advLength,o.at);if(o.len!=o.original){++malformedFrames;return;}
    notebookBle(o.mac,o.addressType,o.rssi,o.data,o.len,o.advLength,o.at);
    bool payloadValid=false;auto f=dragon::parseBleObservation(o.data,o.len,o.advLength,o.mac,o.addressType,payloadValid,detector.enabled(Family::Adult));
    if(!payloadValid)++malformedFrames;if(!f.valid)return;
    countAxon(dragon::inspectAxonBle(o.data,o.len,o.advLength,o.mac,o.addressType),true,payloadValid,o.at);
    auto* contact=getContact(o.mac,true,o.addressType,o.at,f.match);if(!contact)return;Contact& c=*contact;if(c.packets&&int32_t(o.at-c.seen)<0)return;
    update(c,{o.at,o.rssi,o.channel,1},f.name,f.match);
    observeNearby(c);
    dragon::mergeDrone(c.drone,f.drone,o.at);
    // A following scan response is not another advertisement payload variant.
    const size_t length=o.advLength?std::min<size_t>(o.advLength,o.len):o.len;
    checkBleActivity(c,f,o.data,length,o.at);
  }
}
void radioBegin(){
  observations=(Observation*)
#ifdef BOARD_M5DIAL
    malloc(QUEUE_SIZE*sizeof(Observation));
#else
    ps_malloc(QUEUE_SIZE*sizeof(Observation));
#endif
  static StaticQueue_t readyControl,priorityControl,freeControl;static uint8_t readyMemory[QUEUE_SIZE],priorityMemory[QUEUE_SIZE],freeMemory[QUEUE_SIZE];
  if(!observations){radioEnabled=false;addAlert("Radio memory error","Could not allocate packet pool");return;}
  queue=xQueueCreateStatic(QUEUE_SIZE,1,readyMemory,&readyControl);
  priorityQueue=xQueueCreateStatic(QUEUE_SIZE,1,priorityMemory,&priorityControl);
  freeSlots=xQueueCreateStatic(QUEUE_SIZE,1,freeMemory,&freeControl);
  for(uint8_t i=0;i<QUEUE_SIZE;++i)xQueueSend(freeSlots,&i,0);

#ifdef BOARD_WAVESHARE_P4_KNOB
  // The companion worker feeds the same packet pool and detector as other boards.
  blePhase=false;
  p4RadarSetEnabled(radioEnabled && !standby);
  p4CoprocStart();
#else
  // STA transmit sessions publish driver events even without joining a network.
  // The passive NULL interface did not require the default event loop.
#ifndef BOARD_RADAR_LITE
  esp_err_t eventResult=esp_event_loop_create_default();
  if(eventResult!=ESP_OK&&eventResult!=ESP_ERR_INVALID_STATE){radioEnabled=false;addAlert("Wi-Fi event loop failed",esp_err_to_name(eventResult));return;}
#endif
  wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
#ifdef BOARD_M5DIAL
  cfg.static_rx_buf_num=4;cfg.dynamic_rx_buf_num=8;cfg.tx_buf_type=1;
  cfg.dynamic_tx_buf_num=8;cfg.cache_tx_buf_num=8;cfg.rx_ba_win=4;
#endif
  esp_err_t result=esp_wifi_init(&cfg);
  if(result==ESP_OK)result=esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if(result==ESP_OK)result=esp_wifi_set_mode(WIFI_MODE_NULL);
  if(result==ESP_OK)result=esp_wifi_start();
  if(result!=ESP_OK){radioEnabled=false;addAlert("Wi-Fi init failed",esp_err_to_name(result));return;}
  wifi_country_t country={"US",1,11,20,WIFI_COUNTRY_POLICY_MANUAL};esp_wifi_set_country(&country);
  wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filter);esp_wifi_set_promiscuous_rx_cb(wifiRx);esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE);
  // On the P4 Knob BLE is on the radio chip; if it never syncs, NimBLEDevice::init
  // returns false (bounded wait) instead of hanging. Keep Wi-Fi either way.
  if(NimBLEDevice::init("")){
    scanner=NimBLEDevice::getScan();scanner->setScanCallbacks(&bleCallbacks,true);
    configuredActive=activeBle&&reconMode()==dragon::ReconMode::Radar;scanner->setActiveScan(configuredActive);scanner->setInterval(100);scanner->setWindow(100);scanner->setMaxResults(0);
    scanRequest.done=xSemaphoreCreateBinaryStatic(&scanRequest.semaphore);
    ble_npl_event_init(&scanRequest.event,scanOnHost,nullptr);
  }else addAlert("Bluetooth unavailable","Radio chip BLE did not start; Wi-Fi only");
  setRadio(true);
  Serial.printf("RADIO ready: %s; channels 1-11; Axon reserved queue\n",scanner?"BLE-first 2200ms / Wi-Fi 2200ms":"Wi-Fi only (no BLE)");
#endif
}
static void discardQueued(){
  uint8_t pendingIndex;while(xQueueReceive(priorityQueue,&pendingIndex,0)==pdTRUE)xQueueSend(freeSlots,&pendingIndex,0);
  while(xQueueReceive(queue,&pendingIndex,0)==pdTRUE)xQueueSend(freeSlots,&pendingIndex,0);
}
// Room motion's network list while the radar is paused: Wi-Fi beacons fill
// the AP table on every channel, ignoring a channel lock. No BLE, no
// detections, no alerts, and radioEnabled stays false, so the paused radar is
// what other tools see and restore. Any setRadio() call ends it.
#ifdef BOARD_WAVESHARE_P4_KNOB
bool radioSurveyListen(bool on){return !on;}
#else
bool radioSurveyListen(bool on){
  if(on==surveyListen)return true;
  if(on){
    if(radioEnabled||standby||!queue||!scanner||wardriveBusy()||bleLabOwnsRadio()||reconOwnsRadio())return false;
    discardQueued();surveyListen=true;lastHop=millis();
    if(esp_wifi_set_channel(wifiChannel>=1&&wifiChannel<=11?wifiChannel:1,WIFI_SECOND_CHAN_NONE)==ESP_OK&&esp_wifi_set_promiscuous(true)==ESP_OK)return true;
    surveyListen=false;esp_wifi_set_promiscuous(false);return false;
  }
  surveyListen=false;esp_wifi_set_promiscuous(false);discardQueued();return true;
}
#endif
bool radioSurveyListening(){return surveyListen;}
void setRadio(bool enabled){
#ifdef BOARD_WAVESHARE_P4_KNOB
  p4RadarRequested=enabled;
  radioEnabled=p4RadarRequested&&!standby;
  blePhase=false;
  p4RadarSetEnabled(radioEnabled);
#else
  enabled=enabled&&!standby;
  if(surveyListen){surveyListen=false;esp_wifi_set_promiscuous(false);discardQueued();}
  if(!wardriveTransition&&(bleLabOwnsRadio()||reconOwnsRadio()))return;
  if(!queue || !scanner)return;
  if(!enabled && fox)setFoxTransmit(false);
  radioEnabled=enabled;scanCommand(ScanAction::Stop);esp_wifi_set_promiscuous(false);
  // A stopped tool must not turn its queued test traffic into radar alerts.
  discardQueued();
  uint32_t now=millis();lastHop=lastAccounting=now;
  schedule.focus=trackedContact>=0&&contacts[trackedContact].ble?dragon::RadioSchedule::Bluetooth:channelLock?dragon::RadioSchedule::Wifi:dragon::RadioSchedule::Balanced;
  blePhase=enabled&&(wardriveBusy()||reconRadio()!=1);schedule.begin(now,blePhase,wifiChannel);
  wifiSweep.begin(now,wifiChannel);
  if(blePhase&&!scanCommand(ScanAction::Start,configuredActive?1000:dragon::RadioSchedule::BleMs)){
    blePhase=false;schedule.begin(now,false,wifiChannel);addAlert("BLE scan failed","Wi-Fi listening resumed");
  }
  if(enabled&&!blePhase){
    if(channelLock>=1&&channelLock<=11&&esp_wifi_set_channel(channelLock,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=channelLock;
    esp_wifi_set_promiscuous(true);
  }
#endif
}
#if BOARD_HAS_GPS
#ifdef BOARD_WAVESHARE_P4_KNOB
bool radioWardrive(bool on){return !on;}
void radioWardriveStatus(){Serial.println("WARDRIVE_RADIO unavailable");}
#else
#include "wardrive_radio.inc"
bool radioWardrive(bool enabled){
  static int previousTracked=-1;static uint8_t previousChannel=0;static bool previousRadio=true;
  wardriveTransition=true;
  if(enabled){previousTracked=trackedContact;previousChannel=channelLock;previousRadio=radioEnabled;}
  setRadio(false);wardriveScanning=enabled;
  trackedContact=enabled?-1:previousTracked;channelLock=enabled?0:previousChannel;
  scanCommand(ScanAction::Configure,0,enabled?false:activeBle&&reconMode()==dragon::ReconMode::Radar);
#if defined(BOARD_AMOLED_175) || defined(BOARD_WAVESHARE_185B)
  if(enabled){bool ok=driveRadioBegin();wardriveTransition=false;return ok;}
  driveRadioEnd();
#endif
  setRadio(enabled||previousRadio);wardriveTransition=false;
  return true;
}
void radioWardriveStatus(){
#if defined(BOARD_AMOLED_175) || defined(BOARD_WAVESHARE_185B)
  Serial.printf("WARDRIVE_RADIO phase=%u scans=%lu errors=%lu recoveries=%lu scan_max_ms=%lu pending=%u wifi_dwell_ms=100 ble_gap_ms=1500 active_wifi=1\n",unsigned(driveScanPhase),driveScans,driveScanErrors,driveScanResets,driveScanMaxMs,driveResultsLeft);
#else
  Serial.println("WARDRIVE_RADIO passive=1");
#endif
}
#endif
#endif
#include "node_radio.inc"
// Runs on the application task only: from radioLoop() and once more after the
// UI pass, never from a radio or LVGL callback. process() applies the
// wardrive/recon ownership rules either way.
static void drain(bool afterUi){
  unsigned waiting=uxQueueMessagesWaiting(queue)+uxQueueMessagesWaiting(priorityQueue);
  radioQueueHigh=std::max<uint16_t>(radioQueueHigh,waiting);
  const uint32_t limit=dragon::DrainBudget::limit(waiting,QUEUE_SIZE);if(limit>dragon::DrainBudget::ShortUs)++radioLongBatches;
  uint64_t batchAt=esp_timer_get_time();uint8_t index;
  for(unsigned i=0;i<QUEUE_SIZE;++i){
    if(xQueueReceive(priorityQueue,&index,0)!=pdTRUE&&xQueueReceive(queue,&index,0)!=pdTRUE)break;
    radioMaxLagMs=std::max(radioMaxLagMs,uint32_t(millis()-observations[index].at));
    process(observations[index]);xQueueSend(freeSlots,&index,0);++radioHandled;if(afterUi)++radioLateHandled;
    if(!dragon::DrainBudget::more(i+1,esp_timer_get_time()-batchAt,limit))break; // give touch/audio/render a turn
  }
  radioMaxBatchUs=std::max<uint32_t>(radioMaxBatchUs,esp_timer_get_time()-batchAt);
  queueDrops=drops.load();
}
void radioDrain(){
  if(!queue)return;
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(!radioEnabled||standby||p4CoprocBusy()){discardQueued();return;}
#endif
  drain(true);
}
#ifdef BOARD_WAVESHARE_P4_KNOB
bool radioDriversUp(){return queue&&p4CoprocReady();}
#else
bool radioDriversUp(){return queue&&scanner;}
#endif
static void maintainVisits(bool wifi,bool ble,uint32_t now){
  static uint32_t expireAt=0,persistAt=0;
  visitListen(wifi,ble,now);
  if(uint32_t(now-expireAt)>=60000){expireAt=now;visitExpire(now);}
  if(uint32_t(now-persistAt)>=10000){persistAt=now;visitsPersist();}
}

void radioLoop(){
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(!queue)return;
  if(noderadio::owned){discardQueued();noderadio::loop(millis());return;}
  if(radioEnabled&&!standby&&!p4CoprocBusy())drain(false);else discardQueued();
  auto& sample=p4LastSurvey;
  static uint32_t lastBleWindow=0;
  const uint32_t now=millis();
  const bool on=radioEnabled && !standby && p4CoprocReady() && !p4CoprocBusy();
  const auto mode=p4RadarScanMode();
  // Stop charging absence to a radio which is paused, excluded or no longer
  // completing surveys. A normal combined cycle finishes within this interval.
  maintainVisits(on && mode!=2 && p4HaveSurvey && now-sample.finished_ms<15000,
                 on && mode!=1 && lastBleWindow && now-lastBleWindow<15000,now);
  if (radioEnabled && !standby && p4RadarTakeSnapshot(sample)) {
    p4HaveSurvey=true;
    for (unsigned i=0;contacts.data && i<sample.count;++i) {
      const p4_radar_ap_t& ap=sample.aps[i];
      static constexpr uint8_t zero[6]={};
      if (ap.rssi>=0 || ap.rssi<-127 || ap.channel<1 || ap.channel>14 ||
          (ap.bssid[0]&1) || !memcmp(ap.bssid,zero,6)) continue;
      p4RecordSessionContact(false,ap.bssid,0,ap.rssi,ap.channel,nullptr,0,sample.finished_ms);
      char name[64]{};
      dragon::safeText(name,sizeof(name),ap.ssid,std::min<size_t>(ap.ssid_length,sizeof(ap.ssid)));
      const auto identity=dragon::inspectAxonWifiIdentity(ap.bssid,ap.ssid,ap.ssid_length);
      const auto match=dragon::withAxon(dragon::addressHint(ap.bssid,true,dragon::named(name,Family::Wifi)),identity);
      Contact* dot=getContact(ap.bssid,false,0,sample.finished_ms,match);
      if (!dot || (dot->packets && int32_t(sample.finished_ms-dot->seen)<=0)) continue;
      // One observation per BSSID per completed survey, regardless of raw frame count.
      update(*dot,{sample.finished_ms,ap.rssi,ap.channel,1},name,match,ap.channel,true);
      dot->apSecurity=ap.security;
      // Enhanced Open transition deployments may intentionally pair open and OWE APs.
      if(ap.security!=P4_AP_UNKNOWN && ap.security!=P4_AP_OWE){
        uint8_t peer[6];
        const bool secured=ap.security!=P4_AP_OPEN;
        if(ap.ssid_length)++trafficStatus.apSecuritySamples;
        if(ssids.observeAp(ap.bssid,ap.ssid,ap.ssid_length,secured,sample.finished_ms,peer))
          reportSsidConflict(*dot,ap.ssid,ap.ssid_length,secured,peer,sample.finished_ms);
      }
    }
  }
  static p4_ble_snapshot_t* received=static_cast<p4_ble_snapshot_t*>(ps_malloc(sizeof(p4_ble_snapshot_t)));
  blePhase=p4RadarBleListening();
  if (received && p4RadarTakeBleSnapshot(*received) && radioEnabled && !standby && contacts.data) {
    auto& ble=*received;
    lastBleWindow=ble.captured_ms;
    if(ble.listen_ms){
      bleListenMs+=ble.listen_ms;
      nearby.listen(ble.listen_ms,ble.captured_ms);
    }
    // Slots are allocated on first sighting but contain the last sighting's time.
    // Feed the rolling burst windows in reception order, including across millis() wrap.
    std::sort(ble.sightings,ble.sightings+ble.count,[end=ble.captured_ms](const p4_ble_sighting_t& a,const p4_ble_sighting_t& b){
      return uint32_t(end-a.seen_ms)>uint32_t(end-b.seen_ms);
    });
    for (uint8_t i=0;i<ble.count;++i) {
      const p4_ble_sighting_t& sighting=ble.sightings[i];
      bool payloadValid=false;
      const size_t length=std::min<size_t>(sighting.payload_length,sizeof(sighting.payload));
      // The companion retains one complete legacy advertisement per address per window.
      notebookBle(sighting.address,sighting.address_type,sighting.rssi,
                  sighting.payload,length,length,sighting.seen_ms);
      const auto frame=dragon::parseBleObservation(sighting.payload,length,length,sighting.address,
                                                   sighting.address_type,payloadValid,detector.enabled(Family::Adult));
      p4RecordSessionContact(true,sighting.address,sighting.address_type,sighting.rssi,0,
                             sighting.payload,payloadValid?length:0,sighting.seen_ms);
      char name[64]{};
      const size_t nameSize=std::min<size_t>(sighting.name_length,sizeof(sighting.name));
      dragon::safeText(name,sizeof(name),sighting.name,nameSize);
      auto match=frame.valid?frame.match:dragon::Match{Family::Ble,Confidence::None,"passive advertisement"};
      // A name can come from an earlier report by this address in the same window.
      // Keep the stronger payload evidence when combining those observations.
      if (!frame.name[0] && nameSize) match=dragon::withObservedBleName(match,name);
      Contact* dot=getContact(sighting.address,true,sighting.address_type,sighting.seen_ms,match);
      if (!dot || (dot->packets && int32_t(sighting.seen_ms-dot->seen)<=0)) continue;
      dot->connectable=sighting.connectable;
      // Each batch retains one payload per address. Count all received reports,
      // but evaluate signature and notification policy once for the retained payload.
      update(*dot,{sighting.seen_ms,sighting.rssi,0,sighting.reports},name,match);
      if(ble.listen_ms)observeNearby(*dot);
      if(payloadValid){
        ++trafficStatus.blePayloadSamples;
        dragon::mergeDrone(dot->drone,frame.drone,sighting.seen_ms);
        checkBleActivity(*dot,frame,sighting.payload,length,sighting.seen_ms);
      }
    }
    if(ble.listen_ms)nearby.tick();
  }
#else
  if(!queue)return;
  drain(false);
  uint32_t now=millis(),elapsed=now-lastAccounting;lastAccounting=now;
  static uint32_t nearTick=0;if(uint32_t(now-nearTick)>=1000){nearTick=now;nearby.tick();}
  // Only time spent listening on a radio counts as a device being away: the
  // same gates as process(), per radio for BLE-only and Wi-Fi-only modes.
  {const bool on=radioEnabled&&!standby&&!surveyListen&&!wardriveBusy()&&!reconOwnsRadio()&&!bleLabOwnsRadio();maintainVisits(on&&reconRadio()!=2,on&&reconRadio()!=1,now);}
  if(noderadio::owned){noderadio::loop(now);return;}
#if defined(BOARD_AMOLED_175) || defined(BOARD_WAVESHARE_185B)
  if(wardriveBusy()){driveRadioLoop(now,elapsed);return;}
#endif
  if(surveyListen){if(now-lastHop>=250){lastHop=now;uint8_t next=wifiChannel%11+1;if(esp_wifi_set_channel(next,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=next;}return;}
  if(!radioEnabled || !scanner)return;
  schedule.focus=trackedContact>=0&&contacts[trackedContact].ble?dragon::RadioSchedule::Bluetooth:channelLock?dragon::RadioSchedule::Wifi:dragon::RadioSchedule::Balanced;
  if(!wardriveBusy()&&reconRadio()){
    if(reconRadio()==2){if(!scanner->isScanning())scanCommand(ScanAction::Start,10000);bleListenMs+=elapsed;nearby.listen(elapsed,now);return;}
    wifiListenMs+=elapsed;channelDwellMs[wifiChannel]+=elapsed;
    if(now-lastHop>=reconHopDwell()){lastHop=now;uint8_t c=channelLock?channelLock:reconNextChannel(wifiChannel);if(esp_wifi_set_channel(c,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=c;}
    return;
  }
  if(blePhase && !schedule.due(now) && !scanner->isScanning())scanCommand(ScanAction::Start,configuredActive?1000:dragon::RadioSchedule::BleMs);
  if(blePhase){bleListenMs+=elapsed;nearby.listen(elapsed,now);}else{wifiListenMs+=elapsed;channelDwellMs[wifiChannel]+=elapsed;}
  if(!blePhase && schedule.due(now) && wifiSweep.full()){
    esp_wifi_set_promiscuous(false);blePhase=true;schedule.begin(now,true,schedule.firstChannel);
    if(!scanCommand(ScanAction::Start,configuredActive?1000:dragon::RadioSchedule::BleMs)){blePhase=false;schedule.begin(now,false,wifiChannel);esp_wifi_set_promiscuous(true);addAlert("BLE scan failed","Wi-Fi listening resumed");}
  }else if(blePhase && schedule.due(now)){
    scanCommand(ScanAction::Stop);blePhase=false;schedule.begin(now,false,schedule.nextFirst());
    wifiChannel=schedule.firstChannel;
    wifiSweep.begin(now,wifiChannel);
    esp_wifi_set_channel(wifiChannel,WIFI_SECOND_CHAN_NONE);esp_wifi_set_promiscuous(true);lastHop=now;
  }
  bool relocating=trackedContact>=0 && huntSweep(contacts[trackedContact],now);
  if(!blePhase && wifiSweep.hop(now,relocating?0:channelLock,schedule.focus==dragon::RadioSchedule::Wifi?schedule.left(now):0)){
    if(esp_wifi_set_channel(wifiSweep.current,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=wifiSweep.current;
  }
#endif
}

void trackContact(int index){
 if(bleLabOwnsRadio()||reconOwnsRadio())return;
 if(index<0 || index>=int(MAX_CONTACTS) || !contacts[index].used){trackedContact=-1;channelLock=0;setRadio(true);return;}
 trackedContact=index;channelLock=contacts[index].ble?0:contacts[index].channel;setRadio(true);
#ifndef BOARD_WAVESHARE_P4_KNOB
 if(channelLock && esp_wifi_set_channel(channelLock,WIFI_SECOND_CHAN_NONE)==ESP_OK)wifiChannel=channelLock;
#endif
}
// Read-only filter diagnostics for `radar signal`. Addresses are shortened to
// their last two bytes: enough to follow a contact between calls.
void radarSignalStatus(){
 uint32_t now=millis();int rows[8];unsigned count=0;
 if(trackedContact>=0&&contacts[trackedContact].used)rows[count++]=trackedContact;
 else for(size_t i=0;i<MAX_CONTACTS;++i){
  auto& c=contacts[i];if(!c.used||!radarVisible(c,now))continue;
  unsigned at=count;while(at&&contacts[rows[at-1]].rssi<c.rssi)--at;
  if(at>=8)continue;if(count<8)++count;
  for(unsigned j=count-1;j>at;--j)rows[j]=rows[j-1];rows[at]=i;
 }
 Serial.printf("RADAR_SIGNAL_BEGIN count=%u tracked=%d smoothing_ms=%u uptime=%lu\n",count,trackedContact,unsigned(detector.smoothingMs),now);
 for(unsigned k=0;k<count;++k){auto& c=contacts[rows[k]];
  Serial.printf("RADAR_SIGNAL index=%d id=%02x%02x radio=%s family=%u raw=%d filtered=%d spread=%d samples=%u gap_mean_ms=%lu gap_max_ms=%u age_ms=%lu rx_channel=%u adv_channel=%u label=%s\n",rows[k],c.mac[4],c.mac[5],c.ble?"BLE":"WIFI",unsigned(c.match.family),c.signal.raw,c.signal.value(),int(c.signal.spread),unsigned(c.signal.accepted),c.signal.gapMean(),unsigned(c.signal.gapMax),uint32_t(now-c.seen),c.rxChannel,c.advChannel,dragon::familyName(c.match.family));
 }
 Serial.println("RADAR_SIGNAL_END");
}
int findContact(const dragon::DeviceKey& key){for(size_t i=0;i<MAX_CONTACTS;++i)if(contacts[i].used&&contactKey(contacts[i])==key)return i;return -1;}
bool huntDevice(const dragon::DeviceKey& key,uint8_t channel){
 int i=findContact(key);
 if(i<0){auto* c=getContact(key.mac,key.radio,key.addressType,millis(),{key.radio?Family::Ble:Family::Wifi,Confidence::None,"history hunt"});if(!c)return false;c->seen=millis()-60000;c->channel=channel;i=c-contacts.data;}
 trackContact(i);return trackedContact==i;
}

#ifdef BOARD_WAVESHARE_P4_KNOB
void setActiveBle(bool) { activeBle=false; }
void radioApplyMode() {}
void radioRebindBle() {}
#else
void setActiveBle(bool enabled){
  if(bleLabOwnsRadio()||reconOwnsRadio())return;
  activeBle=enabled;detectorSave();
  if(!scanner)return;
  bool listening=radioEnabled;setRadio(false);
  scanCommand(ScanAction::Configure,0,enabled&&reconMode()==dragon::ReconMode::Radar);setRadio(listening);
}

#ifndef BOARD_RADAR_LITE
void radioApplyMode(){if(bleLabOwnsRadio()||reconOwnsRadio()||!scanner)return;setRadio(false);scanCommand(ScanAction::Configure,0,reconMode()==dragon::ReconMode::Radar?activeBle:false);setRadio(true);}
void radioRebindBle(){
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(!p4CoprocReady()||!scanner)return;   // scanner null: BLE did not come up
#endif
  NimBLEDevice::init("");scanner=NimBLEDevice::getScan();scanner->setScanCallbacks(&bleCallbacks,true);
  configuredActive=activeBle&&reconMode()==dragon::ReconMode::Radar;scanner->setActiveScan(configuredActive);scanner->setInterval(100);scanner->setWindow(100);scanner->setMaxResults(0);
  ble_npl_event_init(&scanRequest.event,scanOnHost,nullptr);
}
#endif
#endif
