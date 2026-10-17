#pragma once
#include <stdint.h>
namespace dragon {
enum class LitePress {None,Tap,Double,Hold};
class LiteButton {
  bool raw=true,stable=true,armed=false,held=false,pending=false,second=false;
  uint32_t changed=0,pressedAt=0,releasedAt=0;
 public:
  LitePress update(bool down,uint32_t now){
    if(down!=raw){raw=down;changed=now;}
    if(now-changed>=35&&stable!=raw){
      stable=raw;
      if(stable){if(armed){pressedAt=now;held=false;second=pending&&now-releasedAt<300;}}
      else if(!armed){armed=true;}
      else if(!held){
        if(second){pending=false;second=false;return LitePress::Double;}
        pending=true;releasedAt=now;
      }
    }
    if(armed&&stable&&!held&&now-pressedAt>=800){held=true;pending=false;return LitePress::Hold;}
    if(pending&&!second&&now-releasedAt>=300){pending=false;return LitePress::Tap;}
    return LitePress::None;
  }
};
}
