#pragma once
#include "monitor.h"
#include "tracking.h"

namespace dragon {
// Retry suppression also catches interleaved retransmissions. The full source,
// destination, subtype and sequence/fragment identify a frame for one second.
struct WifiRetryFilter {
  struct Entry {uint8_t source[6]{},destination[6]{},type=0;uint16_t sequence=0;uint32_t at=0;bool used=false;};
  Entry entries[32]{};uint8_t next=0;
  bool duplicate(const WifiFrame& f,uint32_t now){
    if(f.retry)for(const auto& e:entries)if(e.used&&uint32_t(now-e.at)<1000&&e.sequence==f.sequence&&e.type==f.frameType&&!memcmp(e.source,f.source,6)&&!memcmp(e.destination,f.destination,6))return true;
    auto& e=entries[next];next=(next+1)%32;e.used=true;e.at=now;e.sequence=f.sequence;e.type=f.frameType;memcpy(e.source,f.source,6);memcpy(e.destination,f.destination,6);return false;
  }
};
struct BleBurstResult {unsigned addresses=0,variants=0;bool alert=false,payloadChurn=false;};
class BleBurstMonitor {
  struct Entry {DeviceKey key;uint32_t fingerprint=0,at=0;uint8_t group=0;bool used=false;};
  Entry entries[96]{};
public:
  uint32_t lastAlert[4]{};bool alerted[4]{};
  BleBurstResult observe(const DeviceKey& key,uint8_t group,uint32_t fingerprint,uint32_t now){
    BleBurstResult r;if(group>=4)return r;
    Entry* slot=nullptr;Entry* oldest=&entries[0];bool existing=false;
    for(auto& e:entries){
      if(e.used&&e.group==group&&e.key==key&&e.fingerprint==fingerprint&&uint32_t(now-e.at)<=5000){slot=&e;existing=true;break;}
      if((!e.used||uint32_t(now-e.at)>5000)&&!slot)slot=&e;
      if(uint32_t(now-e.at)>uint32_t(now-oldest->at))oldest=&e;
    }
    if(!slot)slot=oldest;*slot={key,fingerprint,now,group,true};
    // At most 96 bounded entries; only compute distinct counts for new facts.
    // Repeated ordinary adverts cannot trigger or prolong a new alert.
    if(existing)return r;
    uint8_t seen[128]{},payloads[128]{}; // bounded indexes, no copied device keys
    for(unsigned i=0;i<96;++i){const auto& e=entries[i];if(!e.used||e.group!=group||uint32_t(now-e.at)>5000)continue;
      unsigned ph=(e.fingerprint^(e.fingerprint>>16))&127;
      while(payloads[ph]&&entries[payloads[ph]-1].fingerprint!=e.fingerprint)ph=(ph+1)&127;
      if(!payloads[ph]){payloads[ph]=i+1;++r.variants;}
      unsigned hash=e.key.addressType;for(auto b:e.key.mac)hash=(hash*33)^b;hash&=127;
      while(seen[hash]&&!(entries[seen[hash]-1].key==e.key))hash=(hash+1)&127;
      if(!seen[hash]){seen[hash]=i+1;++r.addresses;}
    }
    // Tag telemetry routinely changes. Payload-only churn is limited to
    // pairing protocols and needs a higher threshold than address bursts.
    r.payloadChurn=group<3 && r.variants>=24;
    if((r.addresses>=12||r.payloadChurn)&&(!alerted[group]||uint32_t(now-lastAlert[group])>=30000)){
      r.alert=true;alerted[group]=true;lastAlert[group]=now;
    }
    return r;
  }
};
struct TrafficStatus {
  uint32_t disconnectFrames=0,disconnectAlerts=0,bleAlerts=0,lastDisconnect=0,lastBle=0;
  uint8_t channel=0,bleGroup=0;uint16_t reason=0,bleAddresses=0,bleVariants=0;
  bool protectedFrame=false;
  bool disconnectActive(uint32_t now)const{return disconnectAlerts&&uint32_t(now-lastDisconnect)<30000;}
  bool bleActive(uint32_t now)const{return bleAlerts&&uint32_t(now-lastBle)<30000;}
};
inline const char* bleBurstName(unsigned group){static const char* names[]={"Apple pairing","Google Fast Pair","Microsoft Swift Pair","Tracker/beacon"};return group<4?names[group]:"Unknown";}
}
