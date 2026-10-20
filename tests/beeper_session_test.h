#pragma once
#include "beeper_session.h"
#include "radar_interaction.h"
#include "radar_colors.h"
#include <set>
static void beeperSessionTests(){
  using namespace dragon::tracker;
  DeviceKey a{},b{};a.radio=b.radio=1;a.mac[5]=1;b.mac[5]=2;
  BeeperSession<384> session;session.reset(true,100);
  assert(session.add(a,100)&&!session.add(a,100)&&session.add(b,100));
  assert(session.begin());auto fresh=[](const DeviceKey&){return true;};
  assert(session.ready(100,fresh)==0);session.started(0);
  assert(session.ready(50000,fresh)==-1); // Never overlap GATT operations.
  session.complete(AttemptResult::Acknowledged,200);
  assert(session.ready(3199,fresh)==-1&&session.ready(3200,fresh)==1);
  session.started(1);session.complete(AttemptResult::Unsupported,3300);
  assert(session.active&&session.skipped==1);
  assert(session.ready(10000,fresh)==-1&&session.ready(10250,fresh)==0);
  session.started(0);session.complete(AttemptResult::Unavailable,11000);
  assert(session.targets[0].due==26000);
  assert(session.ready(26000,[](const DeviceKey&){return false;})==-1);
  assert(session.ready(26250,fresh)==0);session.started(0);
  session.complete(AttemptResult::Unavailable,27000);assert(session.targets[0].due==57000);
  session.started(0);session.complete(AttemptResult::Unavailable,58000);assert(session.targets[0].due==118000);
  session.started(0);session.complete(AttemptResult::Rejected,119000);assert(!session.active&&session.skipped==2);
  session.reset(false,0xfffffff0);assert(session.add(a,0xfffffff0)&&session.begin());
  assert(session.ready(0xfffffff0,fresh)==0);session.started(0);session.complete(AttemptResult::Acknowledged,0xfffffff5);
  assert(session.ready(100,fresh)==-1&&session.ready(10000,fresh)==0);
  session.started(0);session.stop();session.complete(AttemptResult::Cancelled,10010);
  assert(!session.active&&session.ready(100000,fresh)==-1);
  session.reset(true,0);for(unsigned i=0;i<384;++i){a.mac[4]=i>>8;a.mac[5]=i;assert(session.add(a,0));}
  a.mac[3]=1;assert(!session.add(a,0)&&session.count==384);
  assert(session.begin());for(unsigned i=0;i<384;++i){assert(session.ready(i*3000,fresh)==int(i));session.started(i);session.complete(AttemptResult::Acknowledged,i*3000);}
  assert(session.attempts==384&&session.acknowledged==384);
  // Distinct shades survive the RGB565 panel; slot order, RSSI and theme do
  // not participate in their allocation. Exercise a worst-case single family.
  for(unsigned f=0;f<unsigned(Family::Count);++f){
    std::set<uint16_t> colors;
    for(unsigned i=0;i<384;++i){
      auto c=uniqueRadarColor(Family(f),i*9876543,[&](uint16_t c){return colors.count(c)>0;});
      assert(c&&colors.insert(c).second&&radarRgb565(radarRgb888(c))==c);
    }
  }
  RadarPress press;DeviceKey selected{};assert(!press.take(selected));press.begin(a);
  assert(!radarSelectionExpired(1000,1050)); // Redraw before the next animation tick.
  assert(!radarSelectionExpired(11049,1050)&&radarSelectionExpired(11050,1050));
  assert(!radarSelectionExpired(0xfffffff0,20)); // A tap just after clock wrap.
  assert(!radarSelectionExpired(200,0xfffffff0)&&radarSelectionExpired(10000,0xfffffff0));
  assert(press.take(selected)&&selected==a&&!press.take(selected));press.begin(b);press.clear();assert(!press.take(selected));
  for(int resolution:{240,360,466}){
    auto scale=[&](int n){return n*resolution/360;};
    int nearest=INT32_MAX;assert(radarHit(scale(100),scale(180),scale(120),scale(180),scale(24),nearest));
    assert(!radarHit(scale(140),scale(180),scale(100),scale(180),scale(24),nearest));
    assert(radarHit(scale(119),scale(180),scale(120),scale(180),scale(24),nearest));
  }
  puts("PASS: beeper fairness, freshness, rejection, backoff, Stop, wraparound; 384 distinct RGB565 shades per family; press identity and scaled hit targets");
}
