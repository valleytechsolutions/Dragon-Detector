#pragma once
#include "monitor.h"
#include "tracking.h"
// One notification per device per visit. A visit ends after GoneMs with no
// packet from that device while the radar was listening on its radio; repeats
// inside a visit are counted, never announced. Time with a radio not listening
// (paused, clock, Room motion, a tool, BLE-only or Wi-Fi-only mode) is not
// time away. Both ledgers live outside Contact, so contact eviction cannot
// reset them. All time math is rollover-safe.
namespace dragon {
enum class Verdict:uint8_t {Notify,Repeat};
constexpr uint32_t VisitGoneMs=900000;
// The saved autoQuiet byte keeps its slot in the size-checked settings blob:
// 5 = 15 min away (default), 10 = 60 min away, 0 = once until reboot.
inline uint32_t visitGoneMs(uint8_t setting){return setting==0?0u:setting==10?3600000u:VisitGoneMs;}
inline uint32_t visitHash(const DeviceKey& key){uint32_t h=2166136261u;for(uint8_t b:key.mac)h=(h^b)*16777619u;return ((h^key.radio)*16777619u^key.addressType)*16777619u;}
// notified: the device itself was announced this visit. kinds: incident kinds
// (bit = IncidentKind) this device already took part in, announced, this visit.
struct Visit {DeviceKey key;uint32_t firstAt=0,lastSeen=0;uint16_t repeats=0;uint8_t family=0,kinds=0;bool used=false,notified=false;};
// Compact copy that survives deep sleep and software resets (RTC memory).
struct VisitRecord {DeviceKey key;uint32_t age=0;uint8_t family=0,kinds=0;bool notified=false;uint8_t spare=0;};
// Per-radio listening clock: while a radio is paused, ages stop at pausedAt;
// on resume every visit not already over moves forward by the paused time.
struct ListenClock {
  bool paused[2]{};uint32_t pausedAt[2]{};
  uint32_t at(uint8_t radio,uint32_t now)const{return paused[radio&1]?pausedAt[radio&1]:now;}
  // Something heard after the pause began has age 0 until listening resumes.
  uint32_t age(uint8_t radio,uint32_t last,uint32_t now)const{const uint32_t d=at(radio,now)-last;return paused[radio&1]&&int32_t(d)<0?0:d;}
  // True on resume, with the paused time (away) and when it began (since).
  bool set(uint8_t radio,bool on,uint32_t now,uint32_t& away,uint32_t& since){radio&=1;away=0;if(on!=paused[radio])return false;
    if(!on){paused[radio]=true;pausedAt[radio]=now;return false;}
    paused[radio]=false;since=pausedAt[radio];away=now-since;return true;}
};
// Bounded open addressing: a key lives within Probe slots of its hash. Lookups
// scan the whole window, so freeing an entry never hides another.
struct VisitLedger {
  static constexpr unsigned Probe=16;
  Visit* slots=nullptr;unsigned size=0;uint32_t goneMs=VisitGoneMs,evictions=0,notices=0,repeats=0,restored=0;ListenClock clock;
  void begin(Visit* storage,unsigned n){slots=storage;size=storage?n:0;for(unsigned i=0;i<size;++i)slots[i]=Visit{};evictions=notices=repeats=restored=0;clock=ListenClock{};}
  uint32_t age(const Visit& v,uint32_t now)const{return clock.age(v.key.radio,v.lastSeen,now);}
  bool gone(const Visit& v,uint32_t now)const{return goneMs&&age(v,now)>=goneMs;}
  Visit* find(const DeviceKey& key){
    if(!size)return nullptr;const unsigned h=visitHash(key)%size;
    for(unsigned i=0;i<Probe&&i<size;++i){Visit& v=slots[(h+i)%size];if(v.used&&v.key==key)return &v;}
    return nullptr;
  }
  static void seen(Visit& v,uint32_t now){if(int32_t(now-v.lastSeen)>0)v.lastSeen=now;}
  // Every packet: extends a running visit only. A gone visit stays gone, so the
  // next claim starts a new one.
  void touch(const DeviceKey& key,uint32_t now){Visit* v=find(key);if(v&&!gone(*v,now))seen(*v,now);}
  // A running visit, or a new one (null only for an empty ledger).
  Visit* visit(const DeviceKey& key,uint8_t family,uint32_t now,bool& fresh){
    Visit* v=find(key);fresh=!v||gone(*v,now);
    if(!fresh){seen(*v,now);return v;}
    if(!v)v=place(key,now);
    if(v){*v=Visit{};v->key=key;v->firstAt=v->lastSeen=now;v->family=family;v->used=true;}
    return v;
  }
  Verdict claim(const DeviceKey& key,uint8_t family,uint32_t now){
    bool fresh;Visit* v=visit(key,family,now,fresh);
    if(v&&v->notified){if(v->repeats<65535)++v->repeats;++repeats;return Verdict::Repeat;}
    if(v){v->notified=true;v->family=family;}
    ++notices;return Verdict::Notify;
  }
  // An incident with this device as participant: once per kind per visit. It
  // does not count as the device's own notification.
  Verdict claimIncident(const DeviceKey& key,uint8_t kind,uint32_t now){
    bool fresh;Visit* v=visit(key,0,now,fresh);const uint8_t bit=uint8_t(1u<<(kind&7));
    if(v&&(v->kinds&bit)){if(v->repeats<65535)++v->repeats;return Verdict::Repeat;}
    if(v)v->kinds|=bit;
    return Verdict::Notify;
  }
  // Free slot first, then a visit that has ended, then the least recently seen.
  Visit* place(const DeviceKey& key,uint32_t now){
    if(!size)return nullptr;const unsigned h=visitHash(key)%size;Visit* ended=nullptr;Visit* oldest=nullptr;
    for(unsigned i=0;i<Probe&&i<size;++i){Visit& v=slots[(h+i)%size];
      if(!v.used)return &v;
      if(!ended&&gone(v,now))ended=&v;
      if(!oldest||age(v,now)>age(*oldest,now))oldest=&v;}
    if(ended)return ended;
    ++evictions;return oldest;
  }
  // The radar stopped or resumed listening on one radio (0 Wi-Fi, 1 BLE).
  void listen(uint8_t radio,bool on,uint32_t now){
    uint32_t away,since;if(!clock.set(radio,on,now,away,since)||!away)return;
    for(unsigned i=0;i<size;++i){Visit& v=slots[i];
      if(!v.used||v.key.radio!=(radio&1))continue;
      const uint32_t d=since-v.lastSeen;
      if(int32_t(d)>=0&&(!goneMs||d<goneMs))v.lastSeen+=away;}
  }
  bool listening(uint8_t radio)const{return !clock.paused[radio&1];}
  // Periodic: frees ended visits so a very long absence can never wrap back
  // into "still here" after a millis() rollover.
  void expire(uint32_t now){for(unsigned i=0;i<size;++i)if(slots[i].used&&gone(slots[i],now))slots[i].used=false;}
  unsigned used()const{unsigned n=0;for(unsigned i=0;i<size;++i)n+=slots[i].used;return n;}
  unsigned live(uint32_t now)const{unsigned n=0;for(unsigned i=0;i<size;++i)n+=slots[i].used&&!gone(slots[i],now);return n;}
  // Up to max running visits, the most recently heard first choice.
  unsigned save(VisitRecord* out,unsigned max,uint32_t now)const{
    unsigned n=0,worst=0;
    for(unsigned i=0;i<size&&max;++i){const Visit& v=slots[i];if(!v.used||gone(v,now))continue;
      VisitRecord r;r.key=v.key;r.age=age(v,now);r.family=v.family;r.kinds=v.kinds;r.notified=v.notified;
      if(n<max){out[n]=r;if(out[n].age>out[worst].age)worst=n;++n;continue;}
      if(r.age>=out[worst].age)continue;
      out[worst]=r;for(unsigned j=0;j<n;++j)if(out[j].age>out[worst].age)worst=j;}
    return n;
  }
  // After a sleep or restart: a visit continues when its age plus the time
  // away is still short of the gone time (15 min when that is "until restart").
  void restore(const VisitRecord* in,unsigned n,uint32_t elapsed,uint32_t now){
    const uint32_t limit=goneMs?goneMs:VisitGoneMs;
    for(unsigned i=0;i<n;++i){const VisitRecord& r=in[i];
      if(elapsed>=limit||r.age>=limit-elapsed||r.key.radio>1)continue;
      Visit* v=find(r.key);if(!v)v=place(r.key,now);if(!v)continue;
      *v=Visit{};v->key=r.key;v->lastSeen=v->firstAt=now-(r.age+elapsed);v->family=r.family;v->kinds=r.kinds;v->notified=r.notified;v->used=true;++restored;}
  }
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
inline uint8_t incidentRadio(IncidentKind kind){return kind==IncidentKind::BleBurst?1:0;}
// Never quieter than a device visit; "until restart" (0) never notifies again.
inline uint32_t incidentQuietMs(uint32_t base,uint8_t setting){const uint32_t visit=visitGoneMs(setting);return visit?std::max(base,visit):0xFFFFFFFFu;}
// One disconnect incident per network (BSSID) and channel.
inline uint32_t deauthId(const uint8_t* bssid,uint8_t channel){return crc32(bssid,6)^(uint32_t(channel)*2654435761u);}
// One SSID conflict per SSID and open AP, however many protected (mesh) nodes
// share the name.
inline uint32_t ssidConflictId(const uint8_t* ssid,uint8_t length,const uint8_t* openBssid){return crc32(ssid,length)^(crc32(openBssid,6)*2654435761u);}
// Alerts row key for one incident: never 0, so it never matches a device row.
inline uint32_t incidentTag(const Incident& in){return (uint32_t(in.kind)<<28)^(in.id&0x0FFFFFFFu);}
struct IncidentLedger {
  struct Entry {uint8_t kind=0;uint32_t id=0,firstAt=0,lastTrigger=0,quietMs=0;uint16_t repeats=0;bool used=false;};
  static constexpr unsigned Size=32;
  Entry entries[Size];uint32_t notices=0,repeats=0,evictions=0;ListenClock clock;
  uint32_t age(const Entry& e,uint32_t now)const{return clock.age(incidentRadio(IncidentKind(e.kind)),e.lastTrigger,now);}
  Verdict claim(const Incident& in,uint32_t now){
    Entry* slot=nullptr;
    for(auto& e:entries)if(e.used&&e.kind==uint8_t(in.kind)&&e.id==in.id){slot=&e;break;}
    if(slot&&age(*slot,now)<in.quietMs){slot->lastTrigger=now;if(slot->repeats<65535)++slot->repeats;++repeats;return Verdict::Repeat;}
    if(!slot){Entry* ended=nullptr;Entry* oldest=nullptr;
      for(auto& e:entries){if(!e.used){slot=&e;break;}if(!ended&&age(e,now)>=e.quietMs)ended=&e;if(!oldest||age(e,now)>age(*oldest,now))oldest=&e;}
      if(!slot&&ended)slot=ended;
      if(!slot){slot=oldest;++evictions;}
    }
    *slot=Entry{};slot->kind=uint8_t(in.kind);slot->id=in.id;slot->firstAt=slot->lastTrigger=now;slot->quietMs=in.quietMs;slot->used=true;
    ++notices;return Verdict::Notify;
  }
  void listen(uint8_t radio,bool on,uint32_t now){
    uint32_t away,since;if(!clock.set(radio,on,now,away,since)||!away)return;
    for(auto& e:entries){if(!e.used||incidentRadio(IncidentKind(e.kind))!=(radio&1))continue;
      const uint32_t d=since-e.lastTrigger;if(int32_t(d)>=0&&d<e.quietMs)e.lastTrigger+=away;}
  }
  void expire(uint32_t now){for(auto& e:entries)if(e.used&&age(e,now)>=e.quietMs)e.used=false;}
  unsigned used()const{unsigned n=0;for(auto& e:entries)n+=e.used;return n;}
};
}
