#pragma once
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <math.h>

namespace dragon {
// Median suppresses isolated spikes; elapsed-time EWMA avoids packet-rate bias.
struct SignalTrack {
  int8_t samples[5]{};
  uint8_t count=0, cursor=0;
  float smooth=-100, baseline=-100, spread=0;
  uint32_t last=0, trendAt=0, filterAt=0;
  int8_t trend=0,raw=-100;
  void add(int rssi,uint32_t now,uint16_t smoothingMs=1800) {
    if(rssi>=0 || rssi<-127)return;
    if(!count || uint32_t(now-last)>15000){
      *this={};smooth=baseline=rssi;last=trendAt=filterAt=now;
    }
    last=now;raw=rssi;
    // One median sample per 100 ms: packet floods must not dominate the
    // filter, and expensive math need not run for every repeated advert.
    if(count && uint32_t(now-filterAt)<100)return;
    samples[cursor]=rssi;cursor=(cursor+1)%5;if(count<5)++count;
    int8_t sorted[5];for(unsigned i=0;i<count;++i)sorted[i]=samples[i];
    std::sort(sorted,sorted+count);
    float median=sorted[count/2];
    uint32_t dt=now-filterAt;
    float alpha=1.0f-expf(-float(dt)/std::max<uint16_t>(smoothingMs,100));
    smooth+=alpha*(median-smooth);spread=sorted[count-1]-sorted[0];filterAt=now;
    if(uint32_t(now-trendAt)>=4000){
      float delta=smooth-baseline;trend=delta>=3?1:delta<=-3?-1:0;
      baseline=smooth;trendAt=now;
    }
  }
  int value() const {return int(lroundf(smooth));}
  const char* quality() const {return count<5?"Warming up":spread>12?"Unstable signal":"Stable signal";}
  const char* direction() const {return trend>0?"Signal rising":trend<0?"Signal falling":"Signal steady";}
};
// Display units only: this is a dBm scale, never an estimated range or bearing.
inline float radarRadius(float dbm){return 18.f+std::clamp((-35.f-dbm)/65.f,0.f,1.f)*140.f;}
inline uint8_t radarOpacity(uint32_t age){return age>=30000?0:uint8_t(255-std::min<uint32_t>(age,30000)*200/30000);}
inline uint32_t crc32(const uint8_t* data,size_t n,uint32_t crc=0xffffffffu){
  for(size_t i=0;i<n;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
  return crc;
}
}
