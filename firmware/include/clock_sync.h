#pragma once
#include <stdint.h>
namespace dragon {
// All intervals are below half the uint32_t range, so wraparound is harmless.
struct ClockSyncSchedule {
  uint32_t next=3000;
  bool due(uint32_t now) const {return int32_t(now-next)>=0;}
  void finished(uint32_t now,bool success){next=now+(success?6*60*60*1000u:15*60*1000u);}
};
}
