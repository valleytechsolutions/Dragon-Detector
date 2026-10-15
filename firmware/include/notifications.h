#pragma once
#include "monitor.h"
namespace dragon {
struct DetectionNotice {
  char title[48]{},detail[128]{};DeviceKey key;
  Family family=Family::Wifi;bool hasKey=false,watched=false;uint32_t at=0;
  unsigned priority()const{return family==Family::Axon?100:watched?95:familyPriority(family);}
};
// Bounded bursts: preserve Axon, coalesce repeated device keys, and expire
// queued cards rather than displaying an old encounter as a new detection.
struct NoticeQueue {
  DetectionNotice entries[8];unsigned count=0;
  void push(const DetectionNotice& item){
    for(unsigned i=0;i<count;++i)if(item.hasKey&&entries[i].hasKey&&entries[i].key==item.key){entries[i]=item;return;}
    if(count<8){entries[count++]=item;return;}
    unsigned lowest=0;for(unsigned i=1;i<count;++i)if(entries[i].priority()<entries[lowest].priority())lowest=i;
    if(item.priority()>entries[lowest].priority())entries[lowest]=item;
  }
  bool take(uint32_t now,DetectionNotice& result){
    for(unsigned i=0;i<count;)if(uint32_t(now-entries[i].at)>=30000)entries[i]=entries[--count];else ++i;
    if(!count)return false;unsigned best=0;
    for(unsigned i=1;i<count;++i)if(entries[i].priority()>entries[best].priority()||(entries[i].priority()==entries[best].priority()&&int32_t(entries[i].at-entries[best].at)<0))best=i;
    result=entries[best];entries[best]=entries[--count];return true;
  }
};
}
