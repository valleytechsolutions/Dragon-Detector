#pragma once
#include "monitor.h"

namespace dragon::tracker {
enum class AttemptResult:uint8_t {Waiting,Acknowledged,Unavailable,Unsupported,Rejected,Cancelled};
inline const char* resultName(AttemptResult r){
  switch(r){
    case AttemptResult::Acknowledged:return "write acknowledged";
    case AttemptResult::Unavailable:return "retry after backoff";
    case AttemptResult::Unsupported:return "no supported sound service";
    case AttemptResult::Rejected:return "tracker rejected sound";
    case AttemptResult::Cancelled:return "cancelled";
    default:return "waiting for fresh signal";
  }
}
// App-task scheduler. Connections run on the BLE worker, one at a time.
// The target set is frozen at Start; rotating/new addresses require a new Start.
template<unsigned Capacity> class BeeperSession {
public:
  struct Target {
    DeviceKey key{};
    uint32_t due=0;
    uint16_t attempts=0;
    uint8_t failures=0;
    AttemptResult result=AttemptResult::Waiting;
    bool disabled=false;
  };
  Target targets[Capacity]{};
  unsigned count=0,attempts=0,acknowledged=0,skipped=0;
  int current=-1,last=-1;
  bool active=false,all=false;
private:
  unsigned cursor_=0;
  uint32_t next_=0;
public:
  void reset(bool bulk,uint32_t now){
    // Do not materialize a full session temporary on the small app-task stack.
    for(auto& target:targets)target={};
    count=attempts=acknowledged=skipped=cursor_=0;current=last=-1;
    active=false;all=bulk;next_=now;
  }
  bool add(const DeviceKey& key,uint32_t now){
    if(count==Capacity)return false;
    for(unsigned i=0;i<count;++i)if(targets[i].key==key)return false;
    targets[count++].key=key;targets[count-1].due=now;return true;
  }
  bool begin(){active=count>0;return active;}
  void stop(){active=false;}
  template<class Fresh> int ready(uint32_t now,Fresh fresh){
    if(!active||current>=0||int32_t(now-next_)<0)return -1;
    // Do not scan the target array on every application-loop iteration.
    next_=now+250;
    for(unsigned n=0;n<count;++n){
      unsigned i=(cursor_+n)%count;auto& t=targets[i];
      if(!t.disabled&&int32_t(now-t.due)>=0&&fresh(t.key)){
        cursor_=(i+1)%count;return int(i);
      }
    }
    return -1;
  }
  void started(unsigned index){
    current=int(index);last=current;++attempts;
    if(targets[index].attempts<UINT16_MAX)++targets[index].attempts;
  }
  void complete(AttemptResult result,uint32_t now){
    if(current<0)return;
    auto& t=targets[current];t.result=result;
    if(result==AttemptResult::Acknowledged){++acknowledged;t.failures=0;t.due=now+10000;}
    else if(result==AttemptResult::Unsupported||result==AttemptResult::Rejected){
      t.disabled=true;++skipped;
    }else{
      if(t.failures<3)++t.failures;
      t.due=now+(15000u<<(t.failures?t.failures-1:0));
    }
    current=-1;next_=now+3000; // Allow scanning to refresh addresses between connections.
    bool available=false;for(unsigned i=0;i<count;++i)available|=!targets[i].disabled;
    if(!available)active=false;
  }
};
}
