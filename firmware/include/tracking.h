#pragma once
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <math.h>

namespace dragon {
// Median suppresses isolated spikes; elapsed-time EWMA avoids packet-rate bias.
// Reception is time-shared (BLE 2.2 s / Wi-Fi 2.2 s, 200 ms per channel), so
// most emitters arrive as short runs separated by silence. After a gap longer
// than GapMs the median spans only the samples heard since: otherwise the
// elapsed-time weight is spent on pre-gap values and the dot stays put. One
// post-gap sample takes at most FirstWeight, so a single fade cannot teleport
// the dot; the rest of the elapsed-time weight waits for a second fresh sample.
struct SignalTrack {
  static constexpr uint32_t GapMs=1000;
  static constexpr float FirstWeight=.55f;
  int8_t samples[5]{};
  uint8_t count=0, cursor=0, fresh=0; // fresh: window samples since the last gap
  float smooth=-100, baseline=-100, spread=0;
  uint32_t last=0, trendAt=0, filterAt=0;
  uint32_t gapSum=0; // diagnostics: ms between filter samples, their count
  uint16_t accepted=0, gapMax=0; // and the longest wait; the mean stops at 65535
  int8_t trend=0,raw=-100;
  uint8_t credit=0; // deferred post-gap weight, 0..255
  void add(int rssi,uint32_t now,uint16_t smoothingMs=1800) {
    if(rssi>=0 || rssi<-127)return;
    if(!count || uint32_t(now-last)>15000){
      *this={};smooth=baseline=rssi;last=trendAt=filterAt=now;
    }
    last=now;raw=rssi;
    // One median sample per 100 ms: packet floods must not dominate the
    // filter, and expensive math need not run for every repeated advert.
    if(count && uint32_t(now-filterAt)<100)return;
    uint32_t dt=now-filterAt;
    if(accepted){gapMax=std::max<uint32_t>(gapMax,std::min<uint32_t>(dt,65535));if(accepted<65535)gapSum+=dt;}
    if(accepted<65535)++accepted;
    if(dt>GapMs){fresh=0;credit=0;}
    samples[cursor]=rssi;cursor=(cursor+1)%5;if(count<5)++count;if(fresh<5)++fresh;
    int8_t sorted[5];for(unsigned i=0;i<count;++i)sorted[i]=samples[i];
    std::sort(sorted,sorted+count);spread=sorted[count-1]-sorted[0];
    for(unsigned i=0;i<fresh;++i)sorted[i]=samples[(cursor+4-i)%5];
    std::sort(sorted,sorted+fresh);
    float median=fresh&1?sorted[fresh/2]:(sorted[fresh/2-1]+sorted[fresh/2])*.5f;
    float alpha=1.0f-expf(-float(dt)/std::max<uint16_t>(smoothingMs,100));
    if(fresh==1&&alpha>FirstWeight){credit=uint8_t((alpha-FirstWeight)/(1-FirstWeight)*255);alpha=FirstWeight;}
    else if(credit){if(fresh==2)alpha=std::max(alpha,credit/255.f);credit=0;}
    smooth+=alpha*(median-smooth);filterAt=now;
    if(uint32_t(now-trendAt)>=4000){
      float delta=smooth-baseline;trend=delta>=3?1:delta<=-3?-1:0;
      baseline=smooth;trendAt=now;
    }
  }
  uint32_t gapMean() const {return accepted>1?gapSum/(accepted-1):0;}
  int value() const {return int(lroundf(smooth));}
  const char* quality() const {return count<5?"Warming up":spread>12?"Unstable signal":"Stable signal";}
  const char* direction() const {return trend>0?"Signal rising":trend<0?"Signal falling":"Signal steady";}
};
// Display units only: this is a dBm scale, never an estimated range or bearing.
inline float radarRadius(float dbm){return 18.f+std::clamp((-35.f-dbm)/65.f,0.f,1.f)*140.f;}
// Freshness is drawn as three explicit states, not a fade nobody can read.
// Live allows two radio cycles (about 10 s) without a frame, because a present
// emitter is routinely unheard for one. Fading is clearly dimmer and has no
// highlight or halo. Recent is the segmented marker. The halo follows
// reception: it flashes on a new sample and is gone RadarHaloMs later.
enum class RadarFreshness:uint8_t {Live,Fading,Recent};
constexpr uint32_t RadarLiveMs=10000,RadarFadingMs=30000,RadarHaloMs=600;
inline RadarFreshness radarFreshness(uint32_t age){return age<RadarLiveMs?RadarFreshness::Live:age<RadarFadingMs?RadarFreshness::Fading:RadarFreshness::Recent;}
inline uint8_t radarOpacity(uint32_t age){return age<RadarLiveMs?255:age<RadarFadingMs?130:205;}
inline uint8_t radarHalo(uint32_t sinceSample){return sinceSample>=RadarHaloMs?0:uint8_t(110-sinceSample*110/RadarHaloMs);}
inline uint32_t crc32(const uint8_t* data,size_t n,uint32_t crc=0xffffffffu){
  for(size_t i=0;i<n;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
  return crc;
}
}
