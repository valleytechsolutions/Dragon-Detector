#pragma once
#include <stdint.h>
namespace dragon {
struct WifiSweep {
  uint8_t current=1,completed=0;uint32_t last=0;
  void begin(uint32_t now,uint8_t first){current=first;completed=0;last=now;}
  bool full()const{return completed>=11;}
  bool hop(uint32_t now,uint8_t priority=0){
    if(uint32_t(now-last)<200)return false;
    last=now;if(completed<11)++completed;
    current=full()&&priority>=1&&priority<=11?priority:current%11+1;return true;
  }
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
  uint8_t channel(uint32_t now,uint8_t target=0)const{
    if(focus==Wifi&&uint32_t(now-started)>=WifiMs&&target>=1&&target<=11)return target;
    return 1+(firstChannel-1+uint32_t(now-started)/HopMs)%11;
  }
};
}
