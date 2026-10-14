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
  uint32_t last=0, trendAt=0;
  int8_t trend=0;
  void add(int rssi,uint32_t now,uint16_t smoothingMs=1800) {
    if(rssi>=0 || rssi<-127)return;
    if(!count || uint32_t(now-last)>15000){
      *this={};smooth=baseline=rssi;last=trendAt=now;
    }
    samples[cursor]=rssi;cursor=(cursor+1)%5;if(count<5)++count;
    int8_t sorted[5];for(unsigned i=0;i<count;++i)sorted[i]=samples[i];
    std::sort(sorted,sorted+count);
    float median=sorted[count/2];
    uint32_t dt=now-last;
    float alpha=1.0f-expf(-float(dt)/std::max<uint16_t>(smoothingMs,100));
    smooth+=alpha*(median-smooth);spread=sorted[count-1]-sorted[0];last=now;
    if(uint32_t(now-trendAt)>=4000){
      float delta=smooth-baseline;trend=delta>=3?1:delta<=-3?-1:0;
      baseline=smooth;trendAt=now;
    }
  }
  int value() const {return int(lroundf(smooth));}
  const char* quality() const {return count<5?"Warming up":spread>12?"Unstable signal":"Stable signal";}
  const char* direction() const {return trend>0?"Signal rising":trend<0?"Signal falling":"Signal steady";}
};
inline uint32_t crc32(const uint8_t* data,size_t n,uint32_t crc=0xffffffffu){
  for(size_t i=0;i<n;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
  return crc;
}
}
