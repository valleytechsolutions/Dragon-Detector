#pragma once
#include <stdint.h>
namespace dragon {
// One complete 1..11 Wi-Fi sweep per slice; begin each cycle listening to BLE.
// Dedicated Hunt still owns its selected radio. No concurrent radio contention.
struct RadioSchedule {
  static constexpr uint32_t BleMs=2200,WifiMs=2200,HopMs=200;
  bool ble=true;uint32_t started=0;uint8_t firstChannel=1;
  void begin(uint32_t now,bool bluetooth=true,uint8_t first=1){ble=bluetooth;started=now;firstChannel=first;}
  bool due(uint32_t now)const{return uint32_t(now-started)>=(ble?BleMs:WifiMs);}
  uint8_t channel(uint32_t now)const{return 1+(firstChannel-1+uint32_t(now-started)/HopMs)%11;}
};
}
