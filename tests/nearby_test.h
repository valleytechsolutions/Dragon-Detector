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
  // Address change: gap between A's last advert and B's first, B's level and
  // kind, and whether A is heard once more after B appeared. B waits uncounted
  // while A is still counted and takes A's place only when A has left.
  auto handover=[&](uint32_t gap,int level,bool rotating,bool overlap){
    NearSim s(start);s.adv(10000);s.confirm(1,-60);const uint32_t since=s.find(1)->since;
    for(int i=0;i<200;++i){s.adv(1000);s.hear(1,-60);}s.adv(gap);s.hear(2,level,rotating);
    if(overlap){s.adv(100);s.hear(1,-60);}
    s.adv(1000);s.hear(2,level,rotating);s.adv(1000);s.hear(2,level,rotating);s.adv(600);s.hear(2,level,rotating);
    const bool held=s.state(2)==N::Held;
    if(held)assert(s.n.near==1&&s.n.held==1&&s.n.fresh==1&&s.n.handovers==0&&s.state(1)==N::Near&&s.n.steady==1&&s.text()=="1 BLE RADIO NEAR / 1 STEADY");
    else assert(s.state(2)==N::Near&&s.n.near==2&&s.n.fresh==2&&s.n.held==0);
    for(int i=0;i<21;++i){s.adv(1000);s.hear(2,level,rotating);assert(s.n.near==(held||s.state(1)!=N::Near?1:2));} // the number never moves for a held change
    assert(s.state(2)==N::Near&&s.n.near==1&&s.n.held==0&&s.n.handovers==(held?1u:0u));
    if(held)assert(s.n.fresh==1&&s.n.gone==0&&s.state(1)==N::Free&&s.find(2)->since==since&&s.text()=="1 BLE RADIO NEAR / 1 STEADY");
    else assert(s.n.fresh==2&&s.n.gone==1&&s.state(1)==N::Left&&s.find(2)->since!=since&&s.text()=="1 BLE RADIO NEAR");
    return held;
  };
  assert(handover(1500,-66,true,false));assert(handover(6600,-70,true,false));
  assert(!handover(1500,-66,false,false)); // static or public address
  assert(!handover(6601,-66,true,false));
  assert(!handover(1500,-49,true,false));  // 11 dB apart
  assert(!handover(1500,-66,true,true));   // both on the air at once
  { // The old address is heard again while the new one waits: two radios, nothing inherited.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);for(int i=0;i<200;++i){s.adv(1000);s.hear(1,-60);}assert(s.n.steady==1);
    s.adv(3000);s.confirm(2,-62);assert(s.state(2)==N::Held&&s.n.near==1&&s.n.steady==1);
    s.adv(10000);s.hear(2,-62);assert(s.state(2)==N::Held);s.hear(1,-82); // a missed stretch or a dip, not an address change
    assert(s.state(1)==N::Near&&s.state(2)==N::Near&&s.n.near==2&&s.n.steady==1&&s.n.fresh==2&&s.n.handovers==0&&s.n.held==0);
    assert(s.find(2)->since==s.find(2)->first&&s.text()=="2 BLE RADIOS NEAR / 1 STEADY");
  }
  { // Two newcomers for one silent address: neither inherits, whichever confirms first.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);for(int i=0;i<200;++i){s.adv(1000);s.hear(1,-60);}
    s.adv(1000);s.hear(2,-62);s.adv(500);s.hear(3,-58);s.adv(1000);s.hear(2,-62);s.hear(3,-58);s.adv(1000);s.hear(2,-62);s.hear(3,-58);
    s.adv(500);s.hear(2,-62);assert(s.state(2)==N::Near&&s.state(3)==N::Pending&&s.n.near==2); // 3 is still a candidate
    s.adv(500);s.hear(3,-58);assert(s.state(3)==N::Near&&s.n.near==3&&s.n.held==0&&s.n.steady==1);
    for(int i=0;i<21;++i){s.adv(1000);s.hear(2,-62);s.hear(3,-58);}
    assert(s.n.near==2&&s.n.steady==0&&s.n.gone==1&&s.n.handovers==0&&s.n.fresh==3&&s.state(1)==N::Left);
  }
  { // A second newcomer arrives while the first already waits: the wait ends, nothing is inherited.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);for(int i=0;i<200;++i){s.adv(1000);s.hear(1,-60);}
    s.adv(1000);s.confirm(2,-62);assert(s.state(2)==N::Held);s.adv(1000);s.confirm(3,-58);
    assert(s.state(2)==N::Near&&s.state(3)==N::Near&&s.n.near==3&&s.n.held==0&&s.n.handovers==0&&s.n.fresh==3);
  }
  { // An address that is not steady yet has no time near worth guessing about.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);for(int i=0;i<140;++i){s.adv(1000);s.hear(1,-60);}assert(s.n.steady==0);
    s.adv(1000);s.confirm(2,-62);assert(s.state(2)==N::Near&&s.n.near==2&&s.n.held==0&&s.n.fresh==2);
    s.hear(3,-76);assert(s.state(3)==N::Free&&s.n.known(3)); // nor is a weaker newcomer admitted for it
  }
  { // More than six near or candidate radios, the newcomer included: no two addresses are joined.
    NearSim s(start);s.adv(10000);for(uint32_t id=1;id<=6;++id)s.confirm(id,-60);
    auto all=[&](uint32_t from){for(uint32_t id=from;id<=6;++id)s.hear(id,-60);};
    for(int i=0;i<200;++i){s.adv(1000);all(1);}assert(s.n.near==6&&s.n.steady==6);
    s.adv(1000);all(2);s.hear(7,-62);for(int i=0;i<3;++i){s.adv(1000);all(2);s.hear(7,-62);}
    assert(s.state(7)==N::Near&&s.n.near==7&&s.n.held==0&&s.n.fresh==7&&s.text()=="7 BLE RADIOS NEAR / 6 STEADY");
    s.hear(8,-76);assert(s.state(8)==N::Free);
    for(int i=0;i<20;++i){s.adv(1000);all(2);s.hear(7,-62);}assert(s.n.near==6&&s.n.steady==5&&s.n.gone==1&&s.n.handovers==0);
    // With five counted, the same change is joined.
    for(int i=0;i<200;++i){s.adv(1000);all(3);s.hear(7,-62);}assert(s.n.near==5&&s.n.steady==5&&s.n.gone==2);
    s.adv(1000);all(4);s.hear(7,-62);s.hear(9,-62);for(int i=0;i<3;++i){s.adv(1000);all(4);s.hear(7,-62);s.hear(9,-62);}
    assert(s.state(9)==N::Held&&s.n.near==5);for(int i=0;i<20;++i){s.adv(1000);all(4);s.hear(7,-62);s.hear(9,-62);}
    assert(s.state(9)==N::Near&&s.n.near==5&&s.n.steady==5&&s.n.handovers==1&&s.n.gone==2);
  }
  { // Two silent addresses and one newcomer: no way to tell which changed.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);s.confirm(2,-64);for(int i=0;i<200;++i){s.adv(1000);s.hear(1,-60);s.hear(2,-64);}
    s.adv(1000);s.confirm(3,-62);assert(s.state(3)==N::Near&&s.n.near==3&&s.n.held==0&&s.n.fresh==3);
    for(int i=0;i<21;++i){s.adv(1000);s.hear(3,-62);}assert(s.n.near==1&&s.n.gone==2&&s.n.handovers==0);
  }
  { // Confirmation is bounded: one advert below the level starts it over, so
    // isolated peaks of a radio that hovers below the level never add up.
    NearSim s(start);s.adv(10000);
    for(int peak=0;peak<3;++peak){s.hear(1,-69,false);assert(s.state(1)==N::Pending&&s.find(1)->hits==1);
      for(int i=0;i<600;++i){s.adv(1000);s.hear(1,-76,false);assert(s.state(1)==N::Free&&s.n.pending==0);}s.adv(1000);}
    assert(s.n.near==0&&s.n.fresh==0&&s.text().empty());
    s.hear(1,-69,false);s.adv(1000);s.hear(1,-69,false);s.adv(1000);s.hear(1,-71,false);assert(s.state(1)==N::Free); // two hits, then below
    s.adv(1000);s.confirm(1,-69,false);assert(s.state(1)==N::Near&&s.find(1)->since==s.find(1)->first&&uint32_t(s.n.clock-s.find(1)->since)==2600&&s.n.steady==0);
    // A returning address that dips while confirming goes back to Left.
    s.adv(20000);assert(s.state(1)==N::Left);s.hear(1,-69,false);s.adv(1000);s.hear(1,-75,false);assert(s.state(1)==N::Left&&s.n.pending==0);
  }
  { // A rotating radio resting between the exit and entry levels keeps its place
    // and its time near across an address change; a neighbour hovering at the
    // same level gets one look and is never counted.
    NearSim s(start);s.adv(10000);s.confirm(1,-60);
    auto rest=[&](uint32_t id,unsigned seconds){for(unsigned i=0;i<seconds;++i){s.adv(1000);s.hear(id,-76);s.hear(50,-78);assert(s.text().rfind("1 BLE RADIO NEAR",0)==0&&s.n.near==1);}};
    rest(1,200);assert(s.n.steady==1&&s.state(50)==N::Free&&s.n.known(50)&&s.n.weak==0);const uint32_t since=s.find(1)->since;
    for(uint32_t id=2;id<=5;++id){rest(id,4);assert(s.state(id)==N::Held&&s.find(id)->weak&&s.n.pending==0);
      rest(id,200);assert(s.state(id)==N::Near&&!s.find(id)->weak&&s.state(id-1)==N::Free&&s.find(id)->since==since&&s.n.steady==1&&s.n.handovers==id-1);}
    assert(s.n.fresh==1&&s.n.gone==0&&s.n.dropped==0&&s.state(50)==N::Free&&s.text()=="1 BLE RADIO NEAR / 1 STEADY");
    // Without a counted rotating radio at a similar level nothing below the level is admitted.
    NearSim t(start);t.adv(10000);t.confirm(1,-60,false);t.confirm(2,-50);for(int i=0;i<200;++i){t.adv(1000);t.hear(1,-60,false);t.hear(2,-50);}assert(t.n.steady==2);
    for(uint32_t id=10;id<40;++id){t.hear(id,-76);t.adv(100);t.hear(1,-60,false);t.hear(2,-50);}assert(t.n.pending==0&&t.n.weak==0&&t.n.near==2&&t.n.dropped==0);
    // At most two such candidates at once, refused quietly.
    NearSim u(start);u.adv(10000);u.confirm(1,-70);for(int i=0;i<200;++i){u.adv(1000);u.hear(1,-70);}for(uint32_t id=10;id<40;++id)u.hear(id,-76);
    assert(u.n.weak==2&&u.n.pending==2&&u.n.dropped==0&&!u.n.capped());u.adv(1000);u.hear(1,-70);u.adv(N::PendingMs);u.hear(1,-70);assert(u.n.pending==0&&u.n.weak==0&&u.n.near==1);
  }
}
static void nearbyTests(){
  using namespace dragon;using N=NearbyRadios;
  static_assert(sizeof(NearbyRadios)<=672,"near table");
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
    // Back after 50 s: times are kept, but no number until the picture has
    // warmed up again, and then only radios heard since the pause count.
    const uint32_t since=s.find(1)->since;s.adv(100);assert(s.n.near==0&&s.n.gone==0&&s.n.warm()&&s.text().empty()&&!strcmp(s.n.state(s.wall),"warming"));
    s.hear(1);assert(s.n.near==1&&s.text().empty()&&s.find(1)->since==since);
    s.adv(N::WarmMs);s.hear(1);assert(!s.n.warm()&&s.state(2)==N::Near&&s.n.near==1&&s.text()=="1 BLE RADIO NEAR"); // 2 went during the pause
    s.adv(9000);s.hear(1);assert(s.state(2)==N::Near&&s.n.gone==0);s.adv(1000);s.hear(1);assert(s.state(2)==N::Left&&s.n.gone==1&&s.n.near==1);
    s.confirm(2,-65,false);assert(s.n.near==2&&s.n.returns==1&&s.text()=="2 BLE RADIOS NEAR");
    { // Steady survives a 20 s pause, hidden while warming.
      NearSim p;p.confirm(1);for(int i=0;i<160;++i){p.adv(1000);p.hear(1);}assert(p.text()=="1 BLE RADIO NEAR / 1 STEADY");
      p.wall+=20000;p.adv(100);assert(p.text().empty()&&p.n.near==0&&p.n.steady==0);
      for(int i=0;i<10;++i){p.adv(1000);p.hear(1);}assert(p.text()=="1 BLE RADIO NEAR / 1 STEADY");
      p.wall+=N::ShownWallMs-1001;p.adv(1000);assert(!p.n.warm()&&p.text()=="1 BLE RADIO NEAR / 1 STEADY"); // a gap under 15 s changes nothing
    }
    s.wall+=61000;s.adv(100);assert(s.n.near==0&&s.n.gone==1&&s.n.warm()&&s.text().empty()&&!strcmp(s.n.state(s.wall),"warming"));
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
    s.adv(100);assert(s.text().empty()&&!strcmp(s.n.state(s.wall),"warming"));s.adv(N::WarmMs);s.hear(1);assert(s.text()=="1 BLE RADIO NEAR");
    s.n.on=false;assert(s.text().empty()&&!strcmp(s.n.state(s.wall),"off"));
    s.hear(9,-40);assert(s.state(9)==N::Free); // off: no new entries
  }
  const uint8_t resolvable[6]={0x5a,1,2,3,4,5},unresolvable[6]={0x1a,1,2,3,4,5},fixed[6]={0xda,1,2,3,4,5};
  assert(!rotatingAddress(resolvable,0)&&!rotatingAddress(fixed,1)&&rotatingAddress(resolvable,1)&&rotatingAddress(unresolvable,1));
}
