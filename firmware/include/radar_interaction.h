#pragma once
#include "monitor.h"
namespace dragon {
inline bool radarSelectionExpired(uint32_t frameAt,uint32_t selectedAt){
  // A touch can invalidate the radar before its next animation tick. In that
  // redraw, frameAt precedes selectedAt; unsigned subtraction falsely expires it.
  return int32_t(frameAt-selectedAt)>=10000;
}
// Identity-based, not cache-slot-based: crowded scans can reuse a contact slot.
class RadarTapGesture {
  DeviceKey key_{};uint32_t at_=0;bool pending_=false;
public:
  void clear(){pending_=false;}
  bool tap(const DeviceKey& key,uint32_t now){
    const uint32_t elapsed=now-at_;
    // LVGL has already completed separate press/release cycles. A queued pair
    // can be delivered during one frame, so equal timestamps are valid.
    const bool expand=pending_&&key==key_&&elapsed<=650;
    key_=key;at_=now;pending_=!expand;return expand;
  }
};
// Capture the identity on finger-down. RSSI animation and release coordinates
// must not change the target halfway through a tap.
class RadarPress {
  DeviceKey key_{};bool valid_=false;
public:
  void clear(){valid_=false;}
  void begin(const DeviceKey& key){key_=key;valid_=true;}
  bool take(DeviceKey& key){if(!valid_)return false;key=key_;clear();return true;}
};
inline bool radarHit(int x,int y,int px,int py,int radius,int& nearest){
  const int dx=x-px,dy=y-py,distance=dx*dx+dy*dy;
  if(distance>radius*radius||distance>=nearest)return false;
  nearest=distance;return true;
}
}
