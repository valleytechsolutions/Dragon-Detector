#include "nearby.h"
#include "radio_schedule.h"
// Listening time and wall time advance together here unless a test says otherwise.
struct NearSim {
  dragon::NearbyRadios n;uint32_t wall;
  explicit NearSim(uint32_t start=0):wall(start){n.clock=start;n.listen(0,wall);}
  void adv(uint32_t ms){while(ms){const uint32_t step=ms<1000?ms:1000;n.listen(step,wall+=step);n.tick();ms-=step;}} // ticks once a second, as radioLoop does
  void hear(uint32_t id,int rssi=-60,bool rotating=true){n.observe(id,rssi,rotating);}
  // Hits at 0, 1000, 2000 and 2600 listening ms: the last one confirms.
  void confirm(uint32_t id,int rssi=-60,bool rotating=true){hear(id,rssi,rotating);adv(1000);hear(id,rssi,rotating);adv(1000);hear(id,rssi,rotating);adv(600);hear(id,rssi,rotating);}
  const dragon::NearRadio* find(uint32_t id)const{for(auto& r:n.radios)if(r.state&&r.id==id)return &r;return nullptr;}
  uint8_t state(uint32_t id)const{auto* r=find(id);return r?r->state:uint8_t(dragon::NearbyRadios::Free);}
  std::string text(bool compact=false)const{char s[48];dragon::nearbyText(s,sizeof(s),n,wall,compact);return s;}
};
// One advertiser every 2 s under the real schedule, 1 ms steps, one hour.
static void nearbySchedule(dragon::RadioSchedule::Focus focus){
  using namespace dragon;
  NearbyRadios n;RadioSchedule schedule;schedule.focus=focus;schedule.begin(0,true);uint32_t ticked=0,listened=0;
  for(uint32_t now=1;now<=3600000;++now){
    if(schedule.due(now))schedule.begin(now,!schedule.ble,schedule.firstChannel);
    if(schedule.ble){n.listen(1,now);++listened;if(now%2000==137)n.observe(7,-62,true);}
    if(now-ticked>=1000){ticked=now;n.tick();if(now>30000)assert(n.near==1);}
  }
  assert(listened>=(focus==RadioSchedule::Wifi?1190000u:1790000u)&&n.clock==listened);
  assert(n.near==1&&n.steady==1&&n.gone==0&&n.fresh==0&&n.returns==0&&n.handovers==0&&n.dropped==0);
}
static void nearbyCore(uint32_t start){
  using namespace dragon;using N=NearbyRadios;
  { // Confirmation: three hits and 2500 listening ms.
    NearSim s(start);s.hear(1);assert(s.state(1)==N::Pending&&s.n.pending==1);
    s.adv(1000);s.hear(1);s.adv(1000);s.hear(1);assert(s.state(1)==N::Pending&&s.find(1)->hits==3);
    s.adv(600);s.hear(1);assert(s.state(1)==N::Near&&s.n.near==1&&s.n.pending==0&&s.n.fresh==0); // found while warming
    assert(s.find(1)->since==start);
  }
  { // Leaving: 20000 listening ms, not 19999.
    NearSim s(start);s.adv(10000);s.confirm(1);assert(s.n.fresh==1);
    s.adv(19999);assert(s.state(1)==N::Near&&s.n.near==1&&s.n.gone==0);
    s.adv(1);assert(s.state(1)==N::Left&&s.n.near==0&&s.n.gone==1);
    s.adv(N::ReturnMs-1);assert(s.state(1)==N::Left);s.adv(1);assert(s.state(1)==N::Free);
  }
  { // A return inside the window is not a new address and keeps "since".
    NearSim s(start);s.adv(10000);s.confirm(1);const uint32_t since=s.find(1)->since;assert(since==start+10000);
    s.adv(20000);assert(s.state(1)==N::Left&&s.n.gone==1);s.adv(60000);
    s.confirm(1);assert(s.state(1)==N::Near&&s.n.near==1&&s.n.returns==1&&s.n.fresh==1&&s.find(1)->since==since);
    // A returning address that never confirms goes back to Left, not Free.
    s.adv(20000);assert(s.state(1)==N::Left);s.hear(1);assert(s.state(1)==N::Pending);s.adv(N::PendingMs);assert(s.state(1)==N::Left&&s.n.pending==0);
  }
  // Handover: gap between A's last advert and B's first, B's level and kind, and
  // whether A is heard once more after B appeared.
  auto handover=[&](uint32_t gap,int level,bool rotating,bool overlap){
    NearSim s(start);s.adv(10000);s.confirm(1,-60);const uint32_t since=s.find(1)->since;
    for(int i=0;i<30;++i){s.adv(1000);s.hear(1,-60);}s.adv(gap);s.hear(2,level,rotating);
    if(overlap){s.adv(100);s.hear(1,-60);}
    s.adv(1000);s.hear(2,level,rotating);s.adv(1000);s.hear(2,level,rotating);s.adv(600);s.hear(2,level,rotating);
    assert(s.state(2)==N::Near);
    const bool paired=s.n.handovers==1;
    if(paired)assert(s.n.near==1&&s.n.fresh==1&&s.state(1)==N::Free&&s.find(2)->since==since);
    else assert(s.n.near==2&&s.n.fresh==2&&s.n.handovers==0&&s.find(2)->since!=since);
    return paired;
  };
  assert(handover(1500,-66,true,false));assert(handover(2200,-70,true,false));
  assert(!handover(1500,-66,false,false)); // static or public address
  assert(!handover(2201,-66,true,false));
  assert(!handover(1500,-49,true,false));  // 11 dB apart
  assert(!handover(1500,-66,true,true));   // both on the air at once
  { // The new address confirms slowly, after the old one has left.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);const uint32_t since=s.find(1)->since;for(int i=0;i<30;++i){s.adv(1000);s.hear(1,-60);}
    s.adv(1500);s.hear(2,-66);s.adv(6000);s.hear(2,-66);s.adv(6000);s.hear(2,-75);s.adv(6000);s.hear(2,-75);
    assert(s.state(1)==N::Near&&s.state(2)==N::Pending&&s.n.gone==0);
    s.adv(1000);assert(s.state(1)==N::Left&&s.n.gone==1&&s.n.near==0);
    s.hear(2,-66);assert(s.state(2)==N::Near&&s.state(1)==N::Free&&s.n.near==1&&s.n.gone==0&&s.n.handovers==1&&s.n.fresh==1&&s.find(2)->since==since);
  }
}
static void nearbyTests(){
  using namespace dragon;using N=NearbyRadios;
  static_assert(sizeof(NearbyRadios)<=640,"near table");
  static_assert(N::Slots==24&&N::PendingMax==8&&N::LeftMs>4*RadioSchedule::BleMs,"left timeout spans several radio cycles");
  { // Far adverts never touch the table.
    NearSim s;s.adv(10000);for(uint32_t id=1;id<=1000;++id){s.hear(id,-83);if(id%50==0)s.adv(100);}
    for(auto& r:s.n.radios)assert(r.state==N::Free);assert(!s.n.near&&!s.n.pending&&!s.n.dropped&&s.text().empty());
  }
  { // One hit per clock value: a burst is one hit.
    NearSim s;for(int i=0;i<5;++i)s.hear(1);assert(s.find(1)->hits==1&&s.state(1)==N::Pending);
    s.adv(N::PendingMs-1);assert(s.state(1)==N::Pending);s.adv(1);assert(s.state(1)==N::Free&&s.n.pending==0);
  }
  nearbySchedule(RadioSchedule::Balanced);nearbySchedule(RadioSchedule::Wifi);
  { // A radio pause is not a departure; a long one restarts the picture.
    NearSim s;s.adv(10000);s.confirm(1);s.confirm(2,-65,false);assert(s.n.near==2&&s.n.fresh==2);
    s.wall+=50000;assert(s.text().empty()&&!strcmp(s.n.state(s.wall),"paused"));
    s.adv(100);assert(s.n.near==2&&s.n.gone==0&&s.text()=="2 BLE RADIOS NEAR");
    s.wall+=61000;s.adv(100);assert(s.n.near==0&&s.n.gone==0&&s.n.warm()&&s.text().empty()&&!strcmp(s.n.state(s.wall),"warming"));
    s.confirm(1);assert(s.n.near==1&&s.n.fresh==2&&s.text().empty()&&s.find(1)->since==s.n.warmAt);
    s.adv(8000);s.hear(1);assert(!s.n.warm()&&s.text()=="1 BLE RADIO NEAR");
  }
  { // Exit margin: -82 keeps a near radio, -83 does not.
    NearSim keep,lose;keep.adv(10000);lose.adv(10000);keep.confirm(1);lose.confirm(1);
    for(int i=0;i<30;++i){keep.hear(1,-82);lose.hear(1,-83);keep.adv(1000);lose.adv(1000);}
    assert(keep.state(1)==N::Near&&keep.n.gone==0&&lose.state(1)==N::Left&&lose.n.gone==1);
    NearSim weak;weak.hear(1,-71);weak.adv(1000);weak.hear(1,-82);assert(weak.state(1)==N::Free); // entry needs the level itself
  }
  nearbyCore(0);nearbyCore(0xffffff00u);
  { // Pending never evicts near; a refusal shows as "+".
    NearSim s;s.adv(10000);for(uint32_t id=1;id<=3;++id)s.confirm(id,-60,false);
    for(uint32_t id=1;id<=3;++id)s.hear(id,-60,false);assert(s.n.near==3);
    for(uint32_t id=100;id<2100;++id){s.hear(id,-50);assert(s.n.pending<=N::PendingMax&&s.n.near==3);
      if(id%100==0){s.adv(100);for(uint32_t k=1;k<=3;++k)s.hear(k,-60,false);}}
    assert(s.n.near==3&&s.n.capped()&&s.n.dropped>1900&&s.n.fresh==3&&s.text()=="3+ BLE RADIOS NEAR");
    for(int i=0;i<20;++i){s.adv(1000);for(uint32_t k=1;k<=3;++k)s.hear(k,-60,false);}
    assert(!s.n.capped()&&s.n.pending==0&&s.text()=="3 BLE RADIOS NEAR");
  }
  { // Forty radios: the table holds 24 and says "at least".
    NearSim s;for(int round=0;round<30;++round){for(uint32_t id=1;id<=40;++id)s.hear(id,-60,false);s.adv(1000);}
    for(uint32_t id=1;id<=40;++id)s.hear(id,-60,false);
    assert(s.n.near==24&&s.n.pending==0&&s.n.capped()&&s.text()=="24+ BLE RADIOS NEAR"&&s.text(true)=="24+ BLE RADIOS NEAR");
  }
  { // Steady after 150000 listening ms; exact strings.
    NearSim s;s.confirm(1,-60,false);s.confirm(2,-60,false);
    auto all=[&](unsigned count,uint32_t ms){for(uint32_t t=0;t<ms;t+=1000){for(uint32_t id=1;id<=count;++id)s.hear(id,-60,false);s.adv(1000);}};
    all(2,N::SteadyMs-6000);assert(s.n.near==2&&s.n.steady==0&&s.text()=="2 BLE RADIOS NEAR");
    all(2,6000);assert(s.n.steady==2&&s.text()=="2 BLE RADIOS NEAR / 2 STEADY");
    all(5,5000);assert(s.n.near==5&&s.n.steady==2&&s.n.fresh==3);
    assert(s.text()=="5 BLE RADIOS NEAR / 2 STEADY"&&s.text(true)=="5 BLE RADIOS / 2 STEADY");
    all(5,N::SteadyMs);assert(s.n.near==5&&s.n.steady==5&&s.text()=="5 BLE RADIOS NEAR / 5 STEADY"&&s.n.gone==0);
    char tiny[8];assert(nearbyText(tiny,sizeof(tiny),s.n,s.wall)>int(sizeof(tiny))&&strlen(tiny)==7); // truncated, terminated
  }
  { // Hidden when off, not started, warming, stale or zero.
    NearbyRadios idle;char t[48]="x";assert(nearbyText(t,sizeof(t),idle,0)==0&&!*t&&!strcmp(idle.state(0),"paused"));
    idle.observe(1,-40,true);assert(idle.radios[0].state==N::Free); // nothing before the first listen
    NearSim s;assert(!strcmp(s.n.state(s.wall),"warming"));s.adv(10000);assert(!strcmp(s.n.state(s.wall),"ready")&&s.text().empty()); // zero
    s.confirm(1);assert(s.text()=="1 BLE RADIO NEAR"&&s.text(true)=="1 BLE RADIO NEAR");
    s.wall+=N::ShownWallMs-1;assert(s.text()=="1 BLE RADIO NEAR");s.wall+=1;assert(s.text().empty()&&!strcmp(s.n.state(s.wall),"paused"));
    s.adv(100);assert(s.text()=="1 BLE RADIO NEAR");
    s.n.on=false;assert(s.text().empty()&&!strcmp(s.n.state(s.wall),"off"));
    s.hear(9,-40);assert(s.state(9)==N::Free); // off: no new entries
  }
  const uint8_t resolvable[6]={0x5a,1,2,3,4,5},unresolvable[6]={0x1a,1,2,3,4,5},fixed[6]={0xda,1,2,3,4,5};
  assert(!rotatingAddress(resolvable,0)&&!rotatingAddress(fixed,1)&&rotatingAddress(resolvable,1)&&rotatingAddress(unresolvable,1));
}
