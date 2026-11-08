#pragma once
#include <stdint.h>

namespace dragon {
enum class Amoled175cButtonEvent { None, Tap, Sleep };

class Amoled175cButton {
  bool raw_=true,stable_=true,armed_=false,pressed_=false;
  uint32_t changed_=0,started_=0;
public:
  Amoled175cButtonEvent update(bool down,uint32_t now){
    if(down!=raw_){raw_=down;changed_=now;}
    if(stable_==raw_||uint32_t(now-changed_)<35)return Amoled175cButtonEvent::None;
    stable_=raw_;
    if(stable_){
      if(armed_){pressed_=true;started_=changed_;}
      return Amoled175cButtonEvent::None;
    }
    armed_=true;
    if(!pressed_)return Amoled175cButtonEvent::None;
    pressed_=false;
    // Enter sleep after release; GPIO0 must not remain held as a boot strap.
    return uint32_t(changed_-started_)>=2000?Amoled175cButtonEvent::Sleep:Amoled175cButtonEvent::Tap;
  }
};
}
