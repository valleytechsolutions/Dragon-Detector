#pragma once
#include <stdint.h>
#include <algorithm>
namespace dragon {
// leftMs is the time left in a Hunt slice (0: plain sweep). While it lasts, a
// 200 ms visit to the priority channel follows each sweep hop, so a Wi-Fi Hunt
// target is revisited every 400 ms instead of once at the end of the slice.
// A visit is skipped when the channels still to sweep would no longer fit at
// VisitMs each, or at the pace recent hops kept when the loop is slower than
// that, so visits do not lengthen the slice of a steadily slow loop.
struct WifiSweep {
  static constexpr uint32_t VisitMs=300;
  uint8_t current=1,completed=0,resume=0;uint32_t last=0,pace=200; // resume: sweep channel after a visit; pace: smoothed ms per hop
  void begin(uint32_t now,uint8_t first){current=first;completed=0;resume=0;last=now;}
  bool full()const{return completed>=11;}
  bool hop(uint32_t now,uint8_t priority=0,uint32_t leftMs=0){
    if(uint32_t(now-last)<200)return false;
    pace=(pace*3+std::min<uint32_t>(now-last,2000)+3)/4;last=now;if(resume){current=resume;resume=0;return true;}
    if(completed<11)++completed;
    const bool lock=priority>=1&&priority<=11;const uint8_t next=current%11+1;
    if(full()){current=lock?priority:next;return true;}
    if(lock&&current!=priority&&next!=priority&&leftMs>=(12u-completed)*std::max(VisitMs,pace)){resume=next;current=priority;}
    else current=next;
    return true;
  }
};
// Main-loop time for queued observations. One radar frame can hold the loop
// for 100-170 ms; a fixed 4 ms pass then handles less than arrives and the
// pool overflows although the average rate is modest (measured on a 1.75C:
// about 62 observations/s arriving, about 30% dropped). A backlog above a
// quarter of the pool earns a longer pass; every pass handles a few items.
struct DrainBudget {
  static constexpr uint32_t ShortUs=4000,LongUs=20000;
  static constexpr unsigned MinItems=4;
  static uint32_t limit(unsigned waiting,unsigned capacity){return waiting>capacity/4?LongUs:ShortUs;}
  static bool more(unsigned handled,uint32_t elapsedUs,uint32_t limitUs){return handled<MinItems||elapsedUs<limitUs;}
};
// One complete 1..11 Wi-Fi sweep per slice; begin each cycle listening to BLE.
// Hunt adds a target-focused slice without starving the other radio/channels.
// WifiSweep completes every channel even when rendering delays a hop.
struct RadioSchedule {
  static constexpr uint32_t BleMs=2200,WifiMs=2200,HopMs=200;
  enum Focus:uint8_t {Balanced,Bluetooth,Wifi};
  Focus focus=Balanced;
  bool ble=true;uint32_t started=0;uint8_t firstChannel=1;
  void begin(uint32_t now,bool bluetooth=true,uint8_t first=1){ble=bluetooth;started=now;firstChannel=first;}
  uint32_t duration()const{return ble?(focus==Bluetooth?2*BleMs:BleMs):(focus==Wifi?2*WifiMs:WifiMs);}
  bool due(uint32_t now)const{return uint32_t(now-started)>=duration();}
  uint32_t left(uint32_t now)const{return due(now)?0:duration()-uint32_t(now-started);}
  // The sweep normally starts one channel later each cycle. A Wi-Fi Hunt
  // spreads its sweep over the whole slice, so there the order stays fixed:
  // rotating would make one channel wait from the start of one slice to the
  // end of the next.
  uint8_t nextFirst()const{return focus==Wifi?firstChannel:firstChannel%11+1;}
  uint8_t channel(uint32_t now,uint8_t target=0)const{
    if(focus==Wifi&&uint32_t(now-started)>=WifiMs&&target>=1&&target<=11)return target;
    return 1+(firstChannel-1+uint32_t(now-started)/HopMs)%11;
  }
};
}
