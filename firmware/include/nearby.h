#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
namespace dragon {
// BLE radio addresses heard at a close level; never people. Times are BLE
// listening ms, so a Wi-Fi slice, a Hunt or a paused radio cannot make a
// present radio leave. Nothing here alerts, logs or stores an address.
struct NearRadio {uint32_t id=0,since=0,first=0,heard=0;int8_t rssi=-127;uint8_t state=0,hits=0,prior=0;bool rotating=false,back=false,weak=false,stale=false;};
struct NearbyRadios {
  enum:uint8_t {Free,Pending,Near,Left,Held};
  static constexpr unsigned Slots=24,PendingMax=8,WeakMax=2,SeenMax=8,QuietMax=6;
  static constexpr int ExitDb=12,HandoverDb=10;
  static constexpr uint32_t ConfirmMs=2500,PendingMs=6600,LeftMs=20000,ReturnMs=150000,HandoverMs=6600,SteadyMs=150000,WarmMs=10000,PauseWallMs=60000,ShownWallMs=15000;
  NearRadio radios[Slots]{};
  uint32_t seen[SeenMax]{}; // rotating addresses already heard below the level: not new on the air
  uint32_t clock=0,warmAt=0,lastWall=0,droppedAt=0,fresh=0,gone=0,handovers=0,returns=0,dropped=0;
  uint8_t near=0,steady=0,pending=0,held=0,weak=0,seenAt=0;int8_t enter=-70;bool on=true,started=false;
  int exitDbm()const{return enter-ExitDb;}
  bool warm()const{return uint32_t(clock-warmAt)<WarmMs;}
  bool capped()const{return dropped&&uint32_t(clock-droppedAt)<LeftMs;}
  const char* state(uint32_t wall)const{return !on?"off":!started||uint32_t(wall-lastWall)>=ShownWallMs?"paused":warm()?"warming":"ready";}
  bool shown(uint32_t wall)const{return on&&started&&!warm()&&uint32_t(wall-lastWall)<ShownWallMs;}
  void reset(uint32_t wall){for(auto& r:radios)r={};for(auto& s:seen)s=0;near=steady=pending=held=weak=0;dropped=0;warmAt=clock;lastWall=wall;started=true;}
  // elapsed: BLE listening ms since the last call. A long silence restarts the
  // picture; radios found while it warms up were already there. After a pause
  // that hid the line the table and its times are kept, but the picture warms
  // up again and a radio counts only once it has been heard again.
  void listen(uint32_t elapsed,uint32_t wall){const uint32_t gap=wall-lastWall;
    if(!started||gap>=PauseWallMs)reset(wall);
    else if(gap>=ShownWallMs){warmAt=clock;for(auto& r:radios)if(r.state==Near)r.stale=true;count();}
    clock+=elapsed;lastWall=wall;}
  void count(){near=steady=pending=held=weak=0;for(auto& r:radios){if(r.state==Pending)++pending;if(r.state==Held)++held;if(r.weak)++weak;if(r.state==Near&&!r.stale){++near;if(uint32_t(clock-r.since)>=SteadyMs)++steady;}}}
  bool known(uint32_t id)const{for(auto s:seen)if(s==id)return true;return false;}
  void note(uint32_t id){if(!known(id)){seen[seenAt]=id;seenAt=(seenAt+1)%SeenMax;}}
  // e may be the earlier address of newcomer n: rotating, counted long enough
  // to be steady (so short visits cannot hand their time on down a chain), at a
  // similar level and not heard since shortly before n first appeared.
  bool succeeds(const NearRadio& n,const NearRadio& e)const{return &e!=&n&&e.rotating&&e.state==Near&&uint32_t(clock-e.since)>=SteadyMs&&int32_t(n.first-e.heard)>=0&&uint32_t(n.first-e.heard)<=HandoverMs&&abs(e.rssi-n.rssi)<=HandoverDb;}
  // Timing and level only tell one radio from two when few are around; among
  // many, or when some were refused, no two addresses are ever joined.
  bool quiet()const{return unsigned(near)+pending+held<=QuietMax&&!capped();}
  // A confirmed newcomer on its own: counted as new, or dropped when it was only
  // admitted below the level as a possible successor.
  void alone(NearRadio& n){if(n.weak){note(n.id);n={};return;}n.state=Near;n.prior=0;if(warm())n.since=warmAt;else ++fresh;}
  void inherit(NearRadio& n,NearRadio& e){n.since=e.since;n.state=Near;n.prior=0;n.weak=false;e={};++handovers;}
  void observe(uint32_t id,int rssi,bool rotating){
    if(!on||!started||rssi<exitDbm())return; // weak adverts never touch the table
    NearRadio* r=nullptr;for(auto& e:radios)if(e.state&&e.id==id){r=&e;break;}
    if(!r||r->state==Left){
      bool weakOne=false;
      if(rssi<enter){
        // Below the level a new rotating address may only wait as the possible
        // successor of a steady one at a similar level, once, and two at a time.
        if(r||!rotating||known(id))return;
        bool later=false; // one odd reading must not rule a successor out
        if(weak<WeakMax&&pending<PendingMax&&quiet())for(auto& e:radios)if(e.state==Near&&e.rotating&&uint32_t(clock-e.since)>=SteadyMs&&uint32_t(clock-e.heard)<=HandoverMs){if(abs(e.rssi-rssi)<=HandoverDb){weakOne=true;break;}later=true;}
        if(!weakOne){if(!later)note(id);return;}
      }
      if(pending>=PendingMax){++dropped;droppedAt=clock;return;}
      const bool back=r;const uint32_t since=back?r->since:clock;
      if(!r){NearRadio* old=nullptr;for(auto& e:radios){if(!e.state){r=&e;break;}if(e.state==Left&&(!old||uint32_t(clock-e.heard)>uint32_t(clock-old->heard)))old=&e;}
        if(!r)r=old;if(!r){if(weakOne)note(id);else{++dropped;droppedAt=clock;}return;}}
      *r={};r->id=id;r->since=since;r->first=r->heard=clock;r->rssi=rssi;r->state=Pending;r->hits=1;r->rotating=rotating;r->back=back;r->weak=weakOne;count();return;
    }
    const bool again=clock!=r->heard;r->heard=clock;r->rssi=rssi;
    if(r->state==Near){ // still on the air: nothing took its place, and it counts again after a pause
      if(!r->stale&&!held)return;
      r->stale=false;const uint8_t slot=uint8_t(r-radios+1);for(auto& e:radios)if(e.state==Held&&e.prior==slot)alone(e);
      count();return;
    }
    if(r->state!=Pending)return;
    // Confirmation is three hits in a row at the level: one below it starts over.
    if(rssi<enter&&!r->weak){if(r->back){r->state=Left;r->back=false;}else *r={};count();return;}
    if(again&&r->hits<3)++r->hits;
    if(r->hits<3||uint32_t(clock-r->first)<ConfirmMs)return;
    if(r->back){r->state=Near;r->back=false;++returns;count();return;} // keeps its first "since"
    // One radio changing address, in a quiet place only: exactly one steady
    // rotating address fell silent shortly before this one appeared, at a
    // similar level, and no other newcomer fits it. A candidate cannot outlive
    // the leave timeout, so that address is still counted here. Until it has
    // stayed silent long enough to leave, this one waits uncounted; then it
    // takes its place and its time near. If the old one is heard again, or
    // the match is not unique, nothing is inherited.
    NearRadio* prior=nullptr;unsigned priors=0;
    if(r->rotating&&quiet())for(auto& e:radios)if(succeeds(*r,e)){prior=&e;++priors;}
    if(priors==1)for(auto& e:radios)if(&e!=r&&(e.state==Pending||e.state==Held||e.state==Near)&&e.rotating&&!e.back&&succeeds(e,*prior)){++priors;if(e.state==Held)alone(e);}
    if(priors!=1)alone(*r);else{r->state=Held;r->prior=uint8_t(prior-radios+1);}
    count();
  }
  // Once a second from the application task.
  void tick(){
    if(!started)return;
    for(auto& h:radios)if(h.state==Held){NearRadio& e=radios[h.prior-1];
      if(e.state!=Near)alone(h);else if(uint32_t(clock-e.heard)>=LeftMs)inherit(h,e);}
    for(auto& r:radios){uint32_t quiet=clock-r.heard;
      if(r.state==Pending&&quiet>=PendingMs){if(r.back){r.state=Left;r.back=false;}else{if(r.weak)note(r.id);r={};}}
      else if(r.state==Near&&quiet>=LeftMs){r.state=Left;r.stale=false;++gone;}
      else if(r.state==Left&&quiet>=LeftMs+ReturnMs)r={};
    }
    count();
  }
};
static_assert(sizeof(NearbyRadios)<=672,"near table");
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
