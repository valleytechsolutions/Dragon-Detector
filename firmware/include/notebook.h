#pragma once
#include "recon_analysis.h"
#include "ble_insights.h"
#ifdef BOARD_WAVESHARE_P4_KNOB
#include "p4_advert_fields.h"
#endif
namespace dragon::notebook {
constexpr unsigned Capacity=96,AdvertBytes=512;
struct Advert {
  bool used=false;uint8_t mac[6]{},type=0;int8_t rssi=-127;
  uint16_t length=0,boundary=0;uint32_t seen=0,count=0,changes=0,fingerprint=0;
  uint8_t data[AdvertBytes]{};
#ifdef BOARD_WAVESHARE_P4_KNOB
  uint8_t previous[p4::LegacyAdvertBytes]{},previousLength=0;
  uint32_t previousSeen=0;
  bool hasPrevious=false;
#endif
};
// Bounded complete advertisements only. A shorter prefix cannot masquerade as
// the full payload, and scan-response changes don't masquerade as advert churn.
inline bool observeAdvert(Advert* rows,unsigned capacity,const uint8_t* mac,uint8_t type,int rssi,const uint8_t* p,size_t n,size_t boundary,uint32_t now){
  if(!rows||!capacity||!mac||type>3||n>AdvertBytes||!lab::inspectAdvertisement(p,n,boundary).valid)return false;
  unsigned slot=0;uint32_t age=0;
  for(unsigned i=0;i<capacity;++i){auto& a=rows[i];if(a.used&&a.type==type&&!memcmp(a.mac,mac,6)){slot=i;break;}if(!a.used){slot=i;break;}if(now-a.seen>=age){age=now-a.seen;slot=i;}}
  auto& a=rows[slot];bool same=a.used&&a.type==type&&!memcmp(a.mac,mac,6);
  if(same&&int32_t(now-a.seen)<0)return false;
  uint32_t fingerprint=lab::checksum(p,boundary);
  if(!same)a={};
#ifdef BOARD_WAVESHARE_P4_KNOB
  else if(a.boundary!=boundary || memcmp(a.data,p,boundary)){
    ++a.changes;
    a.hasPrevious=a.boundary<=sizeof(a.previous);
    if(a.hasPrevious){
      a.previousLength=uint8_t(a.boundary);a.previousSeen=a.seen;
      memcpy(a.previous,a.data,a.previousLength);
    }
  }
#else
  else if(fingerprint!=a.fingerprint)++a.changes;
#endif
  a.used=true;memcpy(a.mac,mac,6);a.type=type;a.rssi=rssi;a.length=n;a.boundary=boundary;a.seen=now;a.fingerprint=fingerprint;++a.count;memcpy(a.data,p,n);return true;
}
}
#if defined(ARDUINO) && !defined(BOARD_RADAR_LITE)
#include <Arduino.h>
extern dragon::analysis::Counters notebookCounters;
extern dragon::analysis::Probe* notebookProbes;
extern dragon::notebook::Advert* notebookAdverts;
extern uint32_t notebookRejected;
void notebookBegin();
void notebookWifi(const uint8_t*,size_t,const dragon::WifiFrame&,int,uint8_t,uint32_t);
void notebookBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t);
bool notebookCommand(const String&);
#else
inline void notebookBegin(){}
inline void notebookWifi(const uint8_t*,size_t,const dragon::WifiFrame&,int,uint8_t,uint32_t){}
inline void notebookBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t){}
#endif
