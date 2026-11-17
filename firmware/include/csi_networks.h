#pragma once
// Room motion network choice: rank surveyed APs, pick Auto / next, remember
// per-BSSID motion-data evidence. Pure logic, host-tested.
#include <cstdint>
#include <cstring>
namespace dragon::csi {
enum class Evidence:uint8_t{Unknown,Ofdm,NoData};
inline const char* evidenceName(Evidence e){return e==Evidence::Ofdm?"ofdm":e==Evidence::NoData?"no_data":"unknown";}
struct Candidate {uint8_t mac[6]{},channel=0;int8_t rssi=-127;uint32_t age=0;Evidence evidence=Evidence::Unknown;bool last=false;uint16_t slot=0;};
// The list shows APs heard in the last 30 s. Room motion hears only its own
// channel while it runs, so Next accepts older entries; the in-run diagnosis
// still reports a network that has gone.
constexpr uint32_t FreshMs=30000,NextFreshMs=300000;
inline bool listable(const Candidate& c,uint32_t fresh=FreshMs){return c.age<fresh&&c.channel>=1&&c.channel<=13;}
// OFDM seen first, no motion data last; then signal, then BSSID for a stable order.
inline bool before(const Candidate& a,const Candidate& b){
 auto tier=[](Evidence e){return e==Evidence::Ofdm?0:e==Evidence::Unknown?1:2;};
 if(tier(a.evidence)!=tier(b.evidence))return tier(a.evidence)<tier(b.evidence);
 if(a.rssi!=b.rssi)return a.rssi>b.rssi;return memcmp(a.mac,b.mac,6)<0;
}
// Writes indexes of listable candidates into order, best first; returns count.
inline unsigned rank(const Candidate* c,unsigned n,uint16_t* order,unsigned max,uint32_t fresh=FreshMs){unsigned k=0;
 for(unsigned i=0;i<n;++i){if(!listable(c[i],fresh))continue;unsigned j;
  if(k<max)j=k++;else if(max&&before(c[i],c[order[max-1]]))j=max-1;else continue; // full: replace the worst
  while(j&&before(c[i],c[order[j-1]])){order[j]=order[j-1];--j;}order[j]=uint16_t(i);}
 return k;
}
// Best ranked candidate that is not skip and has not shown no motion data; -1 if none.
inline int pick(const Candidate* c,const uint16_t* order,unsigned count,const uint8_t* skip=nullptr){
 for(unsigned i=0;i<count;++i){auto& x=c[order[i]];if(x.evidence==Evidence::NoData||(skip&&!memcmp(x.mac,skip,6)))continue;return order[i];}return -1;
}
inline int lastHeard(const Candidate* c,const uint16_t* order,unsigned count){for(unsigned i=0;i<count;++i)if(c[order[i]].last)return order[i];return -1;}
// Small LRU of what each tried network gave while Room motion ran.
template<unsigned N> struct EvidenceTable {
 struct Row {bool used=false;uint8_t mac[6]{};Evidence evidence=Evidence::Unknown;uint32_t at=0;};Row rows[N];
 Evidence get(const uint8_t* mac) const {for(auto& r:rows)if(r.used&&!memcmp(r.mac,mac,6))return r.evidence;return Evidence::Unknown;}
 void note(const uint8_t* mac,Evidence e,uint32_t now){Row* slot=nullptr;
  for(auto& r:rows)if(r.used&&!memcmp(r.mac,mac,6)){slot=&r;break;}
  if(!slot)for(auto& r:rows)if(!r.used){slot=&r;break;}
  if(!slot){slot=&rows[0];for(auto& r:rows)if(int32_t(r.at-slot->at)<0)slot=&r;}
  slot->used=true;memcpy(slot->mac,mac,6);slot->evidence=e;slot->at=now;}
};
}
