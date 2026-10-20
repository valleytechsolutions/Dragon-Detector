#pragma once
#include "monitor.h"
namespace dragon {
// Identity-based, not cache-slot-based: crowded scans can reuse a contact slot.
class RadarTapGesture {
  DeviceKey key_{};uint32_t at_=0;bool pending_=false;
public:
  void clear(){pending_=false;}
  bool tap(const DeviceKey& key,uint32_t now){
    const uint32_t elapsed=now-at_;
    const bool expand=pending_&&key==key_&&elapsed>=40&&elapsed<=450;
    key_=key;at_=now;pending_=!expand;return expand;
  }
};
}
