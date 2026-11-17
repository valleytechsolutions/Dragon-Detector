#pragma once
#include "monitor.h"
#include "tracking.h"
// One notification per device per visit. A visit ends after GoneMs with no
// packet from that device; repeats inside a visit are counted, never announced.
// Both ledgers live outside Contact, so contact eviction cannot reset them.
// RAM only: a reboot starts every visit again. All time math is rollover-safe.
namespace dragon {
enum class Verdict:uint8_t {Notify,Repeat};
constexpr uint32_t VisitGoneMs=900000;
// The saved autoQuiet byte keeps its slot in the size-checked settings blob:
// 5 = 15 min away (default), 10 = 60 min away, 0 = once until reboot.
inline uint32_t visitGoneMs(uint8_t setting){return setting==0?0u:setting==10?3600000u:VisitGoneMs;}
inline uint32_t visitHash(const DeviceKey& key){uint32_t h=2166136261u;for(uint8_t b:key.mac)h=(h^b)*16777619u;return ((h^key.radio)*16777619u^key.addressType)*16777619u;}
struct Visit {DeviceKey key;uint32_t firstAt=0,lastSeen=0;uint16_t repeats=0;uint8_t family=0;bool used=false;};
// Bounded open addressing: a key lives within Probe slots of its hash. Lookups
// scan the whole window, so freeing an entry never hides another.
struct VisitLedger {
  static constexpr unsigned Probe=8;
  Visit* slots=nullptr;unsigned size=0;uint32_t goneMs=VisitGoneMs,evictions=0,notices=0,repeats=0;
  void begin(Visit* storage,unsigned n){slots=storage;size=storage?n:0;for(unsigned i=0;i<size;++i)slots[i]=Visit{};evictions=notices=repeats=0;}
  bool gone(const Visit& v,uint32_t now)const{return goneMs&&uint32_t(now-v.lastSeen)>=goneMs;}
  Visit* find(const DeviceKey& key){
    if(!size)return nullptr;const unsigned h=visitHash(key)%size;
    for(unsigned i=0;i<Probe&&i<size;++i){Visit& v=slots[(h+i)%size];if(v.used&&v.key==key)return &v;}
    return nullptr;
  }
  // Every packet: extends a running visit only. A gone visit stays gone, so the
  // next claim starts a new one.
  void touch(const DeviceKey& key,uint32_t now){Visit* v=find(key);if(v&&!gone(*v,now)&&int32_t(now-v->lastSeen)>0)v->lastSeen=now;}
  Verdict claim(const DeviceKey& key,uint8_t family,uint32_t now){
    Visit* v=find(key);
    if(v&&!gone(*v,now)){if(int32_t(now-v->lastSeen)>0)v->lastSeen=now;if(v->repeats<65535)++v->repeats;++repeats;return Verdict::Repeat;}
    if(!v)v=place(key,now);
    if(v){*v=Visit{};v->key=key;v->firstAt=v->lastSeen=now;v->family=family;v->used=true;}
    ++notices;return Verdict::Notify;
  }
  // Free slot first, then a visit that has ended, then the least recently seen.
  Visit* place(const DeviceKey& key,uint32_t now){
    if(!size)return nullptr;const unsigned h=visitHash(key)%size;Visit* ended=nullptr;Visit* oldest=nullptr;
    for(unsigned i=0;i<Probe&&i<size;++i){Visit& v=slots[(h+i)%size];
      if(!v.used)return &v;
      if(!ended&&gone(v,now))ended=&v;
      if(!oldest||uint32_t(now-v.lastSeen)>uint32_t(now-oldest->lastSeen))oldest=&v;}
    if(ended)return ended;
    ++evictions;return oldest;
  }
  // Periodic: frees ended visits so a very long absence can never wrap back
  // into "still here" after a millis() rollover.
  void expire(uint32_t now){for(unsigned i=0;i<size;++i)if(slots[i].used&&gone(slots[i],now))slots[i].used=false;}
  unsigned used()const{unsigned n=0;for(unsigned i=0;i<size;++i)n+=slots[i].used;return n;}
  unsigned live(uint32_t now)const{unsigned n=0;for(unsigned i=0;i<size;++i)n+=slots[i].used&&!gone(slots[i],now);return n;}
};
// The per-contact step in front of the ledger: one decision per evidence run.
// The run restarts when the caller clears `decided` (expiry, a new family) or
// the settings/rules/mode revision changes.
inline bool alertDecisionDue(bool& decided,uint16_t& revision,uint16_t current){
  if(decided&&revision==current)return false;
  decided=true;revision=current;return true;
}
// Incidents are not one device: a deauth burst on a channel, an open/protected
// SSID pair, a BLE advert burst group. Every trigger refreshes the incident,
// so a continuing incident never notifies again; quiet for quietMs, it may.
enum class IncidentKind:uint8_t {Deauth=1,EvilTwin=2,BleBurst=3};
constexpr uint32_t DeauthQuietMs=600000,BleBurstQuietMs=600000,EvilTwinQuietMs=1800000;
struct Incident {IncidentKind kind;uint32_t id,quietMs;};
inline const char* incidentName(uint8_t kind){return kind==1?"deauth":kind==2?"ssid_conflict":kind==3?"ble_burst":"none";}
// The same pair in either order, for the same SSID, is one incident.
inline uint32_t ssidPairId(const uint8_t* a,const uint8_t* b,const uint8_t* ssid,uint8_t length){
  const uint8_t* lo=memcmp(a,b,6)<=0?a:b;const uint8_t* hi=lo==a?b:a;uint8_t pair[12];memcpy(pair,lo,6);memcpy(pair+6,hi,6);
  return crc32(ssid,length)^(crc32(pair,12)*2654435761u);
}
struct IncidentLedger {
  struct Entry {uint8_t kind=0;uint32_t id=0,firstAt=0,lastTrigger=0,quietMs=0;uint16_t repeats=0;bool used=false;};
  static constexpr unsigned Size=32;
  Entry entries[Size];uint32_t notices=0,repeats=0,evictions=0;
  Verdict claim(const Incident& in,uint32_t now){
    Entry* slot=nullptr;
    for(auto& e:entries)if(e.used&&e.kind==uint8_t(in.kind)&&e.id==in.id){slot=&e;break;}
    if(slot&&uint32_t(now-slot->lastTrigger)<in.quietMs){slot->lastTrigger=now;if(slot->repeats<65535)++slot->repeats;++repeats;return Verdict::Repeat;}
    if(!slot){Entry* ended=nullptr;Entry* oldest=nullptr;
      for(auto& e:entries){if(!e.used){slot=&e;break;}if(!ended&&uint32_t(now-e.lastTrigger)>=e.quietMs)ended=&e;if(!oldest||uint32_t(now-e.lastTrigger)>uint32_t(now-oldest->lastTrigger))oldest=&e;}
      if(!slot&&ended)slot=ended;
      if(!slot){slot=oldest;++evictions;}
    }
    *slot=Entry{};slot->kind=uint8_t(in.kind);slot->id=in.id;slot->firstAt=slot->lastTrigger=now;slot->quietMs=in.quietMs;slot->used=true;
    ++notices;return Verdict::Notify;
  }
  void expire(uint32_t now){for(auto& e:entries)if(e.used&&uint32_t(now-e.lastTrigger)>=e.quietMs)e.used=false;}
  unsigned used()const{unsigned n=0;for(auto& e:entries)n+=e.used;return n;}
};
}
