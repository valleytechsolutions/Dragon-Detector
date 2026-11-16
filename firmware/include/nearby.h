#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
namespace dragon {
// BLE radio addresses heard at a close level; never people. Times are BLE
// listening ms, so a Wi-Fi slice, a Hunt or a paused radio cannot make a
// present radio leave. Nothing here alerts, logs or stores an address.
struct NearRadio {uint32_t id=0,since=0,first=0,heard=0;int8_t rssi=-127;uint8_t state=0,hits=0;bool rotating=false,back=false;};
struct NearbyRadios {
  enum:uint8_t {Free,Pending,Near,Left};
  static constexpr unsigned Slots=24,PendingMax=8;
  static constexpr int ExitDb=12,HandoverDb=10;
  static constexpr uint32_t ConfirmMs=2500,PendingMs=6600,LeftMs=20000,ReturnMs=150000,HandoverMs=2200,SteadyMs=150000,WarmMs=10000,PauseWallMs=60000,ShownWallMs=15000;
  NearRadio radios[Slots]{};
  uint32_t clock=0,warmAt=0,lastWall=0,droppedAt=0,fresh=0,gone=0,handovers=0,returns=0,dropped=0;
  uint8_t near=0,steady=0,pending=0;int8_t enter=-70;bool on=true,started=false;
  int exitDbm()const{return enter-ExitDb;}
  bool warm()const{return uint32_t(clock-warmAt)<WarmMs;}
  bool capped()const{return dropped&&uint32_t(clock-droppedAt)<LeftMs;}
  const char* state(uint32_t wall)const{return !on?"off":!started||uint32_t(wall-lastWall)>=ShownWallMs?"paused":warm()?"warming":"ready";}
  bool shown(uint32_t wall)const{return on&&started&&!warm()&&uint32_t(wall-lastWall)<ShownWallMs;}
  void reset(uint32_t wall){for(auto& r:radios)r={};near=steady=pending=0;dropped=0;warmAt=clock;lastWall=wall;started=true;}
  // elapsed: BLE listening ms since the last call. A long silence restarts the
  // picture; radios found while it warms up were already there.
  void listen(uint32_t elapsed,uint32_t wall){if(!started||uint32_t(wall-lastWall)>=PauseWallMs)reset(wall);clock+=elapsed;lastWall=wall;}
  void count(){near=steady=pending=0;for(auto& r:radios){if(r.state==Pending)++pending;if(r.state==Near){++near;if(uint32_t(clock-r.since)>=SteadyMs)++steady;}}}
  void observe(uint32_t id,int rssi,bool rotating){
    if(!on||!started||rssi<exitDbm())return; // weak adverts never touch the table
    NearRadio* r=nullptr;for(auto& e:radios)if(e.state&&e.id==id){r=&e;break;}
    if(!r||r->state==Left){
      if(rssi<enter)return;
      if(pending>=PendingMax){++dropped;droppedAt=clock;return;}
      const bool back=r;const uint32_t since=back?r->since:clock;
      if(!r){NearRadio* old=nullptr;for(auto& e:radios){if(!e.state){r=&e;break;}if(e.state==Left&&(!old||uint32_t(clock-e.heard)>uint32_t(clock-old->heard)))old=&e;}
        if(!r)r=old;if(!r){++dropped;droppedAt=clock;return;}}
      *r={};r->id=id;r->since=since;r->first=r->heard=clock;r->rssi=rssi;r->state=Pending;r->hits=1;r->rotating=rotating;r->back=back;++pending;return;
    }
    const bool again=clock!=r->heard;r->heard=clock;r->rssi=rssi;if(r->state!=Pending||rssi<enter)return;
    if(again&&r->hits<3)++r->hits;
    if(r->hits<3||uint32_t(clock-r->first)<ConfirmMs)return;
    r->state=Near;--pending;
    if(r->back){r->back=false;++returns;count();return;} // keeps its first "since"
    // A rotating address that fell silent just before this one first appeared,
    // at a similar level: one radio changed address. The count is unchanged
    // and the new address inherits how long the radio has been near. The old
    // address may already have left when a slow confirmation ends.
    NearRadio* prior=nullptr;
    if(r->rotating)for(auto& e:radios){
      if(&e==r||!e.rotating||(e.state!=Near&&e.state!=Left)||int32_t(r->first-e.heard)<0||uint32_t(r->first-e.heard)>HandoverMs||abs(e.rssi-r->rssi)>HandoverDb)continue;
      if(!prior||abs(e.rssi-r->rssi)<abs(prior->rssi-r->rssi))prior=&e;}
    if(prior){if(prior->state==Left&&gone)--gone;r->since=prior->since;*prior={};++handovers;}
    else if(warm())r->since=warmAt; else ++fresh;
    count();
  }
  // Once a second from the application task.
  void tick(){
    if(!started)return;
    for(auto& r:radios){uint32_t quiet=clock-r.heard;
      if(r.state==Pending&&quiet>=PendingMs){if(r.back){r.state=Left;r.back=false;}else r={};}
      else if(r.state==Near&&quiet>=LeftMs){r.state=Left;++gone;}
      else if(r.state==Left&&quiet>=LeftMs+ReturnMs)r={};
    }
    count();
  }
};
static_assert(sizeof(NearbyRadios)<=640,"near table");
// "5 BLE RADIOS NEAR / 2 STEADY"; compact drops "NEAR". Empty at zero, and
// while the radio is not listening to BLE or the picture is still warming up:
// no number is better than a stale or early one.
inline int nearbyText(char* text,size_t cap,const NearbyRadios& n,uint32_t wall,bool compact=false){
  if(cap)*text=0;if(!n.shown(wall))return 0;
  if(!n.near&&!n.capped())return 0;
  int used=snprintf(text,cap,"%u%s BLE RADIO%s%s",unsigned(n.near),n.capped()?"+":"",n.near==1?"":"S",compact&&n.steady?"":" NEAR");
  if(n.steady&&used>0&&size_t(used)<cap)used+=snprintf(text+used,cap-used," / %u STEADY",unsigned(n.steady));
  return used;
}
inline bool rotatingAddress(const uint8_t* mac,uint8_t type){return type==1&&(mac[0]>>6)!=3;} // random, not static: private addresses change
}
