#pragma once
#include <stdint.h>

namespace dragon {
class RotaryDecoder {
  uint8_t previous=0;
  int8_t partial=0;
public:
  void reset(uint8_t state){previous=state&3;partial=0;}
  __attribute__((always_inline)) int update(uint8_t state){
    state&=3;uint8_t changed=previous^state;
    if(!changed)return 0;
    if(changed==3){previous=state;partial=0;return 0;}
    partial+=((previous&1)^((state>>1)&1))?1:-1;previous=state;
    if(partial>=4){partial=0;return 1;}
    if(partial<=-4){partial=0;return -1;}
    return 0;
  }
};

// Emit only complete Gray-code cycles back to the resting position.
class DetentDecoder {
  uint8_t previous=0,anchor=0;
  int8_t travel=0;
  bool synchronized=true;
public:
  void reset(uint8_t state){previous=anchor=state&3;travel=0;synchronized=true;}
  int update(uint8_t state){
    state&=3;uint8_t changed=previous^state;
    if(!changed)return 0;
    if(changed==3){previous=state;travel=0;synchronized=state==anchor;return 0;}
    if(!synchronized){previous=state;if(state==anchor)synchronized=true;return 0;}
    travel+=((previous&1)^((state>>1)&1))?1:-1;previous=state;
    if(state!=anchor)return 0;
    int result=travel==4?1:travel==-4?-1:0;travel=0;return result;
  }
};

inline bool touchContactValid(uint8_t count,uint16_t x,uint16_t y,uint16_t strength,uint16_t size){
  return count>0&&strength>0&&x<size&&y<size;
}
// A single valid report starts contact; missing reports must persist before lift.
class TouchContactFilter {
  bool stable=false;
  uint32_t lastContact=0;
public:
  bool update(bool down,uint32_t now){
    if(down){stable=true;lastContact=now;}
    else if(stable&&uint32_t(now-lastContact)>=60)stable=false;
    return stable;
  }
};

enum class RotaryPress {None,Click,Back};
class RotaryNavigation {
  int pending=0;
  bool stepped=false;
  uint32_t receivedAt=0,steppedAt=0;
  uint32_t expiryMs,intervalMs;
public:
  explicit RotaryNavigation(uint32_t expiry=200,uint32_t interval=180):expiryMs(expiry),intervalMs(interval){}
  void clear(){pending=0;}
  void push(int steps,uint32_t now){
    if(!steps)return;
    pending=steps>0?1:-1;receivedAt=now;
  }
  int take(uint32_t now){
    if(!pending)return 0;
    if(uint32_t(now-receivedAt)>expiryMs){pending=0;return 0;}
    if(stepped&&uint32_t(now-steppedAt)<intervalMs)return 0;
    int step=pending;pending=0;stepped=true;steppedAt=now;return step;
  }
};

class RotaryButton {
  bool raw=false,stable=false,held=false,armed=false;
  uint32_t changedAt=0,pressedAt=0;
public:
  RotaryPress update(bool down,uint32_t now){
    if(down!=raw){raw=down;changedAt=now;}
    if(raw!=stable&&uint32_t(now-changedAt)>=40){
      stable=raw;
      if(stable){pressedAt=now;held=false;}
      else {bool click=armed&&!held;armed=true;return click?RotaryPress::Click:RotaryPress::None;}
    }
    if(!raw&&!stable)armed=true;
    if(stable&&armed&&!held&&uint32_t(now-pressedAt)>=800){held=true;return RotaryPress::Back;}
    return RotaryPress::None;
  }
};
}
