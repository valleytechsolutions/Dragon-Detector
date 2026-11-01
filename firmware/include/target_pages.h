#pragma once
#include "detection.h"
namespace dragon {
enum class TargetGroup : uint8_t {Alpr,Axon,Cameras,Drones,Acoustic,Tools,Activity,Other,Adult,Trackers,Count};
inline TargetGroup targetGroup(Family f){
  if(trackerFamily(f))return TargetGroup::Trackers;
  switch(f){
    case Family::Flock:case Family::Alpr:return TargetGroup::Alpr;
    case Family::Axon:return TargetGroup::Axon;
    case Family::Camera:case Family::Ring:case Family::Meta:return TargetGroup::Cameras;
    case Family::Drone:return TargetGroup::Drones;
    case Family::Raven:return TargetGroup::Acoustic;
    case Family::Deauth:case Family::EvilTwin:case Family::BleSpam:return TargetGroup::Activity;
    case Family::Wifi:case Family::Ble:return TargetGroup::Other;
    case Family::Adult:return TargetGroup::Adult;
    default:return TargetGroup::Tools;
  }
}
inline const char* targetGroupName(TargetGroup g){
  static const char* names[]={"ALPR / FLOCK","AXON","CAMERAS","DRONES / REMOTE ID","ACOUSTIC SENSORS","RADIO TOOLS","RADIO ACTIVITY","OTHER SIGNALS","ADULT CANDIDATES","TRACKERS"};
  return unsigned(g)<unsigned(TargetGroup::Count)?names[unsigned(g)]:"LISTENING";
}
// Each category starts on its own page; empty categories take no navigation.
// Offset indexes a list sorted by category then signal priority.
struct TargetPages {
  unsigned counts[unsigned(TargetGroup::Count)]{};
  static constexpr unsigned Rows=10;
  void add(Family f){++counts[unsigned(targetGroup(f))];}
  unsigned pages()const{unsigned n=0;for(auto count:counts)n+=(count+Rows-1)/Rows;return n;}
  struct Slice {TargetGroup group=TargetGroup::Count;unsigned offset=0,count=0,page=0,pages=0;};
  Slice slice(unsigned index)const{
    unsigned offset=0;
    for(unsigned g=0;g<unsigned(TargetGroup::Count);++g){unsigned n=(counts[g]+Rows-1)/Rows;
      if(index<n){unsigned left=counts[g]-index*Rows;return {TargetGroup(g),offset+index*Rows,left<Rows?left:Rows,index,n};}
      index-=n;offset+=counts[g];
    }
    return {};
  }
};
}
