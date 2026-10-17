#pragma once
#include <stdint.h>
namespace dragon {
// GPIO0 doubles as the ROM download strap. A button held during startup must
// first be released. Stable presses generate one event, without auto-repeat.
class ThemeButton {
  bool raw_=true, stable_=true, armed_=false;
  uint32_t changed_=0;
 public:
  bool update(bool pressed,uint32_t now){
    if(pressed!=raw_){raw_=pressed;changed_=now;}
    if(uint32_t(now-changed_)<35||stable_==raw_)return false;
    stable_=raw_;
    if(!stable_){armed_=true;return false;}
    if(!armed_)return false;
    armed_=false;return true;
  }
};
}
