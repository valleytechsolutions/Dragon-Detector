#pragma once
#include "detection.h"
#include <algorithm>

namespace dragon {
struct DeviceKey {
  uint8_t mac[6]{};uint8_t radio=0,addressType=0;
  bool operator==(const DeviceKey& b)const {return radio==b.radio && addressType==b.addressType && !memcmp(mac,b.mac,6);}
};
struct DetectorSettings {
  uint32_t families=(1u<<unsigned(Family::Count))-1u;
  Confidence minimum=Confidence::Medium;
  uint8_t autoQuiet=5;
  bool privacy=false,history=true,wakeAlert=true,quiet=false;
  uint16_t dimSeconds=0,offSeconds=0;
  uint8_t idleBrightness=8;
  uint16_t smoothingMs=1800;
  bool enabled(Family f)const{return unsigned(f)<unsigned(Family::Count) && (families&(1u<<unsigned(f)));}
};
struct AlertGate {
  uint32_t window=0,last=0;int peak=-127;uint8_t count=0;bool active=false;
  bool allow(uint32_t now,int rssi,uint8_t budget,bool watched=false) {
    if(!active || uint32_t(now-window)>=1800000){*this={};active=true;window=now;}
    bool louder=rssi>=peak+7;
    if(count && uint32_t(now-last)<(watched?10000u:30000u))return false;
    if(!watched && budget && count>=budget && !louder)return false;
    last=now;peak=std::max(peak,rssi);if(count<255)++count;return true;
  }
};
// Six received disconnect frames in a rolling three-second window, including
// changing/spoofed source addresses. A participant is not an identified sender.
struct DisconnectMonitor {
  uint32_t times[64]{},lastAlert=0;uint8_t count=0,next=0;bool alerted=false;
  unsigned recent(uint32_t now)const{unsigned n=0;for(unsigned i=0;i<count;++i)if(uint32_t(now-times[i])<=3000)++n;return n;}
  bool observe(uint32_t now){
    times[next]=now;next=(next+1)%64;if(count<64)++count;
    if(recent(now)>=6 && (!alerted||uint32_t(now-lastAlert)>=15000)){
      lastAlert=now;alerted=true;return true;
    }return false;
  }
};
// A repeated, contemporaneous security mismatch is useful evidence. Multiple
// BSSIDs alone are normal mesh/enterprise Wi-Fi and must never raise an alert.
class SsidMonitor {
  struct AP {bool used=false;uint8_t mac[6]{},security=0,hits=0;uint32_t first=0,last=0,alertAt=0;bool alerted=false;uint8_t ssid[32]{},length=0;};
  AP entries[64]{};
public:
  bool observe(const WifiFrame& f,uint32_t now,uint8_t* peer) {
    if(!f.valid || !f.beacon || !*f.name || (f.bssid[0]&1) || memcmp(f.source,f.bssid,6))return false;
    AP* slot=nullptr;AP* oldest=&entries[0];
    for(auto& a:entries){if(a.used&&!memcmp(a.mac,f.bssid,6)){slot=&a;break;}if(!a.used&&!slot)slot=&a;if(uint32_t(now-a.last)>uint32_t(now-oldest->last))oldest=&a;}
    if(!slot)slot=oldest;
    if(!slot->used || memcmp(slot->mac,f.bssid,6) || slot->length!=f.ssidLength || memcmp(slot->ssid,f.ssid,f.ssidLength) || slot->security!=f.security || uint32_t(now-slot->last)>30000){
      *slot={};slot->used=true;memcpy(slot->mac,f.bssid,6);memcpy(slot->ssid,f.ssid,f.ssidLength);slot->length=f.ssidLength;slot->security=f.security;slot->first=now;
    }
    slot->last=now;if(slot->hits<255)++slot->hits;
    if(slot->hits<3 || uint32_t(now-slot->first)<400 || (slot->alerted&&uint32_t(now-slot->alertAt)<60000))return false;
    for(auto& a:entries)if(&a!=slot && a.used && a.hits>=3 && uint32_t(a.last-a.first)>=400 && uint32_t(now-a.last)<15000 && a.length==slot->length && !memcmp(a.ssid,slot->ssid,a.length)) {
      // Compare open versus protected only: WPA2/WPA3 transition-mode suites
      // legitimately differ between APs. Do not overclaim an evil twin.
      if(bool(a.security)!=bool(slot->security) && (!a.alerted || uint32_t(now-a.alertAt)>=60000)) {
        slot->alerted=a.alerted=true;slot->alertAt=a.alertAt=now;memcpy(peer,a.mac,6);return true;
      }
    }
    return false;
  }
};
struct SignalHistory {
  int8_t samples[48]{};uint8_t count=0,next=0;uint32_t last=0;int16_t peak=-127,mark=-127;
  static constexpr int Gap=-128;
  void push(int rssi){samples[next]=rssi;next=(next+1)%48;if(count<48)++count;}
  void add(int rssi,uint32_t now){
    if(rssi>=0||rssi<-127)return;peak=std::max<int16_t>(peak,rssi);
    if(!count){last=now;push(rssi);return;}
    uint32_t steps=uint32_t(now-last)/500;
    if(!steps){samples[(next+47)%48]=rssi;return;}
    for(unsigned i=1;i<std::min<uint32_t>(steps,48);++i)push(Gap);
    push(rssi);last+=steps*500;
  }
  int at(unsigned i)const{return samples[(next+48-count+i)%48];}
  int recent(unsigned column,uint32_t now)const{
    uint32_t age=uint32_t(now-last)/500;if(!count||age>=48||column>=48)return Gap;
    int index=int(column)+int(age)+int(count)-48;return index>=0&&index<count?at(index):Gap;
  }
};
}
