#pragma once
#include "monitor.h"
#include "tracking.h"

struct Contact {
  bool used=false,ble=false; uint8_t addressType=0,mac[6]{}; char name[64]{};
  int16_t rssi=-100; uint8_t channel=0; uint32_t seen=0,firstSeen=0,packets=0,lastLog=0;
  dragon::Match match{dragon::Family::Wifi,dragon::Confidence::None,""};
  dragon::SignalTrack signal; uint32_t matchedAt=0,matchSince=0; uint16_t matchHits=0; bool signatureAlerted=false;
  uint32_t wifiAlerts=0,lastWifiAlert=0;
  dragon::Family trafficFamily=dragon::Family::Deauth;
  char trafficEvidence[128]{};
  float radarX=0,radarY=0,plotRadius=0;
  dragon::AlertGate gate;dragon::DroneInfo drone;dragon::SignalHistory trace;
  uint32_t historyAt=0;dragon::Family historyFamily=dragon::Family::Count;
};
constexpr size_t MAX_CONTACTS=384;
// Storage lives in PSRAM on all three boards, leaving internal RAM for radio,
// USB, LVGL and DMA. Constructed during setup after PSRAM is initialized.
struct ContactStore {
  Contact* data=nullptr;
  Contact* begin(){return data;}
  Contact* end(){return data?data+MAX_CONTACTS:data;}
  Contact& operator[](size_t index){return data[index];}
};
inline dragon::DeviceKey keyOf(const Contact& c){dragon::DeviceKey k;memcpy(k.mac,c.mac,6);k.radio=c.ble;k.addressType=c.ble?c.addressType:0;return k;}
inline uint32_t contactHash(const dragon::DeviceKey& key){uint32_t hash=2166136261u;for(uint8_t b:key.mac)hash=(hash^b)*16777619u;return ((hash^key.radio)*16777619u^key.addressType)*16777619u;}
// A collision is a cache miss, never a merged identity. Slot reuse is checked
// against the complete key before returning cached results.
struct ContactCache {
  uint16_t slots[1024]{};
  int lookup(const dragon::DeviceKey& key,const Contact* entries)const {
    uint16_t value=slots[contactHash(key)%1024];
    return value && entries[value-1].used && keyOf(entries[value-1])==key?value-1:-1;
  }
  void remember(const dragon::DeviceKey& key,size_t index){slots[contactHash(key)%1024]=index+1;}
};
inline bool trafficFresh(const Contact& c,uint32_t now){return c.wifiAlerts && uint32_t(now-c.lastWifiAlert)<30000;}
// Visibility is deliberately independent of audible-alert confidence and the
// repeat-evidence gate. A fleeting, valid observation still deserves a dot.
inline bool candidateVisible(const Contact& c,uint32_t now,const dragon::DetectorSettings& settings,bool all,bool ignore,bool watch){
  if(!c.used || !c.packets || ignore || uint32_t(now-c.seen)>=30000)return false;
  bool signature=c.match.confidence!=dragon::Confidence::None && c.matchHits && uint32_t(now-c.matchedAt)<30000;
  return watch || (settings.enabled(c.match.family)&&(all||signature)) || (trafficFresh(c,now)&&settings.enabled(c.trafficFamily));
}
inline dragon::Family visibleFamily(const Contact& c,uint32_t now,uint32_t families=0xffffffffu){return trafficFresh(c,now)&&(families&(1u<<unsigned(c.trafficFamily)))?c.trafficFamily:c.match.family;}
inline bool huntSweep(const Contact& c,uint32_t now){return !c.ble && (!c.packets || uint32_t(now-c.seen)>=12000);}
inline bool preferContact(const Contact& a,const Contact& b,uint32_t now){
  if(trafficFresh(a,now)!=trafficFresh(b,now))return trafficFresh(a,now);
  if(a.match.confidence!=b.match.confidence)return a.match.confidence>b.match.confidence;
  if(a.rssi!=b.rssi)return a.rssi>b.rssi;
  return contactHash(keyOf(a))<contactHash(keyOf(b));
}
