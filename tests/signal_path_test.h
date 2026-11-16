#include "contacts.h"
#include "radio_schedule.h"
// The 0.14.7 filter, kept only as the "before" reference for the step budget.
struct LegacySignalTrack {
  int8_t samples[5]{};uint8_t count=0,cursor=0;float smooth=-100;uint32_t last=0,filterAt=0;
  void add(int rssi,uint32_t now,uint16_t smoothingMs=1800){
    if(!count||uint32_t(now-last)>15000){*this={};smooth=rssi;last=filterAt=now;}
    last=now;if(count&&uint32_t(now-filterAt)<100)return;
    samples[cursor]=rssi;cursor=(cursor+1)%5;if(count<5)++count;
    int8_t sorted[5];for(unsigned i=0;i<count;++i)sorted[i]=samples[i];std::sort(sorted,sorted+count);
    smooth+=(1.0f-expf(-float(now-filterAt)/std::max<uint16_t>(smoothingMs,100)))*(sorted[count/2]-smooth);filterAt=now;
  }
};
struct StepTimes { uint32_t mean63=0,worst63=0,mean90=0,worst90=0; };
// Balanced reception as radio.cpp schedules it: BLE for the first 2.2 s of each
// 4.4 s cycle, then eleven 200 ms Wi-Fi channels; this access point sits on
// the sixth, so its 102 ms beacons are heard about twice per cycle. A 20 dB
// step (-80 to -60 dBm) is applied at 44 phases of the cycle. Loop timing and
// reception are ideal: this bounds the filter, not the hardware.
template<class Track> static StepTimes stepResponse(unsigned interval,bool accessPoint,uint16_t smoothingMs=1800){
  StepTimes r;uint64_t sum63=0,sum90=0;unsigned runs=0;
  for(uint32_t phase=0;phase<4400;phase+=100,++runs){
    Track t;const uint32_t step=44000+phase;uint32_t at63=0,at90=0;
    for(uint32_t at=7;at<step+60000&&!at90;at+=interval){
      const uint32_t slot=at%4400;if(accessPoint?slot<3200||slot>=3400:slot>=dragon::RadioSchedule::BleMs)continue;
      t.add(at<step?-80:-60,at,smoothingMs);if(at<step)continue;
      if(!at63&&t.smooth>=-67.4f)at63=at-step;if(t.smooth>=-62.f)at90=at-step;
    }
    assert(at63&&at90);sum63+=at63;sum90+=at90;r.worst63=std::max(r.worst63,at63);r.worst90=std::max(r.worst90,at90);
  }
  r.mean63=sum63/runs;r.mean90=sum90/runs;return r;
}
static void signalPathTests(){
  using namespace dragon;
  // After a gap the median restarts: the first fresh sample moves the value by
  // FirstWeight at most, the second spends the rest of the elapsed-time weight.
  SignalTrack g;for(int n=0;n<10;++n)g.add(-80,n*100);assert(g.fresh==5&&g.value()==-80);
  g.add(-60,3100);assert(g.fresh==1&&g.count==5&&fabsf(g.smooth-(-80+20*SignalTrack::FirstWeight))<.01f);
  g.add(-60,3150);assert(g.fresh==1&&g.raw==-60); // 100 ms gate still applies
  g.add(-60,3200);assert(g.fresh==2&&fabsf(g.smooth-(-80+20*(1-expf(-2200.f/1800))))<.15f);
  g.add(-60,3300);assert(g.fresh==3&&g.credit==0&&g.smooth<-64.5f); // no further credit
  assert(g.accepted==13&&g.gapMax==2200&&g.gapMean()==3300/12);
  // One faded sample after a gap: bounded, where the old filter ignored it and
  // equally ignored a real change. This is the cost of the faster response.
  SignalTrack fade;for(int n=0;n<10;++n)fade.add(-60,n*100);fade.add(-85,5000);assert(fade.smooth>=-60-25*SignalTrack::FirstWeight-.01f);
  fade.add(-60,5100);assert(fade.smooth>-73.f&&fade.smooth<-71.f);fade.add(-60,5200);fade.add(-60,5300);assert(fade.smooth>-73.f);
  // A short gap (under GapMs) keeps the full five-sample spike rejection.
  SignalTrack dense;for(int n=0;n<10;++n)dense.add(-70,n*100);dense.add(-30,1800);assert(dense.fresh==5&&dense.value()==-70);
  // True median for even counts: the old code took the upper of two samples.
  SignalTrack even;even.add(-70,0);even.add(-60,1000);assert(fabsf(even.smooth-(-70+5*(1-expf(-1000.f/1800))))<.01f);
  // A lone sample after silence under the 15 s restart cannot spend more than FirstWeight.
  SignalTrack lone;lone.add(-90,0);lone.add(-50,14000);assert(lone.count==2&&fabsf(lone.smooth-(-90+40*SignalTrack::FirstWeight))<.01f);
  assert(sizeof(SignalTrack)<=44); // 384 PSRAM contacts, 32 internal on Dial
  // Step-response budget, default 1.8 s smoothing, mean / worst over phase.
  //                        63% before      63% after      90% before      90% after
  // Access point beacons   6.6 / 8.7 s     2.3 / 4.3 s    6.6 / 8.7 s     2.3 / 4.4 s
  // BLE every 100 ms       3.1 / 4.1 s     2.1 / 4.1 s    5.0 / 6.7 s     3.7 / 4.4 s
  // BLE every 1 s          5.9 / 7.9 s     3.5 / 4.9 s    9.6 / 11.9 s    6.1 / 7.9 s
  // BLE every 2 s          9.9 / 11.9 s    5.9 / 7.9 s    13.9 / 15.9 s   9.9 / 11.9 s
  auto apOld=stepResponse<LegacySignalTrack>(102,true),ap=stepResponse<SignalTrack>(102,true);
  assert(apOld.mean63>=6000&&apOld.worst90>=8500); // the defect: two cycles before the dot moves
  assert(ap.mean63<=2500&&ap.worst63<=4500&&ap.mean90<=2500&&ap.worst90<=4500); // within one 4.4 s cycle
  auto fastOld=stepResponse<LegacySignalTrack>(100,false),fast=stepResponse<SignalTrack>(100,false);
  assert(fastOld.mean63>=3000&&fast.mean63<=2300&&fast.worst63<=fastOld.worst63&&fast.mean90<=4000&&fast.worst90<=4500);
  auto secondOld=stepResponse<LegacySignalTrack>(1000,false),second=stepResponse<SignalTrack>(1000,false);
  assert(secondOld.mean63>=5500&&second.mean63<=3700&&second.worst63<=5000&&second.worst90<=8000);
  auto slowOld=stepResponse<LegacySignalTrack>(2000,false),slow=stepResponse<SignalTrack>(2000,false);
  assert(slowOld.mean63>=9500&&slow.mean63<=6000&&slow.worst63<=8000&&slow.worst90<=12000);
  // Fast and Steady presets now differ for a sparse emitter (both were 6.6 s to 63%).
  assert(stepResponse<SignalTrack>(102,true,600).mean90<stepResponse<SignalTrack>(102,true,4000).mean90);
  for(uint16_t preset:{600,4000})for(unsigned interval:{100u,1000u,2000u}){
    auto before=stepResponse<LegacySignalTrack>(interval,false,preset),after=stepResponse<SignalTrack>(interval,false,preset);
    assert(after.mean63<=before.mean63&&after.worst63<=before.worst63&&after.mean90<=before.mean90&&after.worst90<=before.worst90);
  }
  // Main-loop drain: short pass for a small backlog, longer above a quarter of
  // the pool, and always a few items even when one of them was slow.
  assert(DrainBudget::limit(0,240)==4000&&DrainBudget::limit(60,240)==4000&&DrainBudget::limit(61,240)==20000&&DrainBudget::limit(240,240)==20000);
  assert(DrainBudget::limit(4,16)==4000&&DrainBudget::limit(5,16)==20000);
  assert(DrainBudget::more(1,900000,4000)&&DrainBudget::more(3,900000,4000)&&!DrainBudget::more(4,4000,4000)&&DrainBudget::more(200,3999,4000));
  assert(DrainBudget::more(100,19999,20000)&&!DrainBudget::more(100,20000,20000));
  // Advertised channel: an AP beacon heard on a neighbouring channel keeps the
  // channel it states; later data frames do not move it; a probing station's
  // stated channel applies to that frame only; out-of-range values are ignored.
  Contact accessPoint;noteChannel(accessPoint,5,6,true);assert(accessPoint.channel==6&&accessPoint.rxChannel==5&&accessPoint.advChannel==6);
  noteChannel(accessPoint,7,0,false);assert(accessPoint.channel==6&&accessPoint.rxChannel==7);
  noteChannel(accessPoint,11,11,true);assert(accessPoint.channel==11&&accessPoint.advChannel==11);
  Contact station;noteChannel(station,1,1,false);assert(station.channel==1&&!station.advChannel);noteChannel(station,6,0,false);assert(station.channel==6);
  noteChannel(station,6,7,false);assert(station.channel==7&&!station.advChannel);noteChannel(station,6,0,false);assert(station.channel==6);
  for(uint8_t bad:{0,12,13,14,36,255}){Contact c;noteChannel(c,3,bad,true);assert(c.channel==3&&!c.advChannel&&!radarChannel(bad));}
  Contact bluetooth;bluetooth.ble=true;noteChannel(bluetooth,0,0,false);assert(!bluetooth.channel);
  // Eviction order for a full table.
  auto contact=[](Family family,Confidence confidence,uint32_t seen){Contact c;c.used=true;c.packets=1;c.seen=seen;c.match={family,confidence,"fixture"};c.matchHits=confidence!=Confidence::None;c.matchedAt=seen;return c;};
  const uint32_t now=100000;
  Contact anonymous=contact(Family::Ble,Confidence::None,now-1000),tracker=contact(Family::Tracker,Confidence::Medium,now-20000),flock=contact(Family::Flock,Confidence::Strong,now-25000);
  auto score=[&](const Contact& c,bool watched=false,bool visible=false,Family incoming=Family::Flock){return evictionScore(c,now,watched,visible,incoming);};
  // Before this rule the classified tracker (priority 10) went before a hidden anonymous address (priority 20).
  assert(score(anonymous)>score(tracker)&&score(tracker)>score(flock));
  // A visible unclassified contact (All scope, traffic event) keeps the old family/age order.
  assert(score(anonymous,false,true)<score(tracker)&&score(anonymous,false,true)<score(anonymous));
  Contact older=anonymous;older.seen=now-9000;assert(score(older)>score(anonymous)); // age term kept
  // Long-silent contacts go before any live one, and watched contacts last.
  Contact stale=contact(Family::Flock,Confidence::Strong,now-30000);assert(score(stale)>score(anonymous));
  assert(score(anonymous,true)<score(flock)&&score(stale,true)>score(anonymous));
  // A live higher-priority detection is protected from a lower-priority newcomer.
  assert(score(flock,false,true,Family::Tracker)==0&&score(flock,false,true,Family::Ble)==0&&score(flock,false,true,Family::Flock)>0);
  assert(score(stale,false,false,Family::Tracker)>0&&score(tracker,false,true,Family::Tracker)>0&&score(anonymous,false,false,Family::Tracker)==0);
  Contact wrapped=contact(Family::Ble,Confidence::None,0xfffffff0);assert(evictionScore(wrapped,10,false,false,Family::Ble)==1+26+100000000u+50000000u+8000000u);
  // Wi-Fi Hunt dwell. The driver repeats radioLoop()'s slice and hop rules.
  struct HuntRun {unsigned lockMs,lockGap,channelGap,cycleMs;};
  auto hunt=[](uint8_t first,uint8_t lock,bool interleave,unsigned poll,unsigned jitter){
    RadioSchedule s;s.focus=RadioSchedule::Wifi;WifiSweep w;uint32_t now=0xfffff000u,next=now,cycleAt=now;s.begin(now,true,first);
    bool ble=true;uint8_t channel=first;unsigned cycles=0,rng=7,gap=0,gaps[12]={};HuntRun r{};
    for(;cycles<24;++now){
      if(int32_t(now-next)>=0){rng=rng*1103515245u+12345u;next=now+poll+(jitter?(rng>>16)%jitter:0);
        if(!ble&&s.due(now)&&w.full()){ble=true;s.begin(now,true,s.firstChannel);if(++cycles>2)r.cycleMs=std::max<unsigned>(r.cycleMs,now-cycleAt);cycleAt=now;}
        else if(ble&&s.due(now)){ble=false;s.begin(now,false,interleave?s.nextFirst():s.firstChannel%11+1);channel=s.firstChannel;w.begin(now,channel);}
        if(!ble&&w.hop(now,lock,interleave?s.left(now):0))channel=w.current;
      }
      if(cycles<2)continue;
      if(!ble&&channel==lock){++r.lockMs;gap=0;}else r.lockGap=std::max(r.lockGap,++gap);
      for(unsigned c=1;c<=11;++c)if(!ble&&channel==c)gaps[c]=0;else r.channelGap=std::max(r.channelGap,++gaps[c]);
    }
    r.lockMs/=22;return r;
  };
  // Exact timing: the lock channel keeps 2400 of 6600 ms (36%), but the longest
  // wait for it falls from 4200 to 2400 ms and no channel waits longer than
  // 6400 ms (8400 ms before, when the rotating sweep order wrapped).
  unsigned worstLock=0,worstChannel=0,oldLock=0,oldChannel=0;
  for(uint8_t first=1;first<=11;++first)for(uint8_t lock=1;lock<=11;++lock){
    auto after=hunt(first,lock,true,1,0),before=hunt(first,lock,false,1,0);
    assert(after.lockMs==2400&&before.lockMs==2400&&after.cycleMs==6600&&before.cycleMs==6600);
    assert(after.lockGap==(first==lock?2200u:2400u));
    worstLock=std::max(worstLock,after.lockGap);worstChannel=std::max(worstChannel,after.channelGap);oldLock=std::max(oldLock,before.lockGap);oldChannel=std::max(oldChannel,before.channelGap);
  }
  assert(worstLock==2400&&worstChannel==6400&&oldLock==4200&&oldChannel==8400);
  // A main loop that only reaches the radio every 170-254 ms (a radar frame)
  // drops visits before it delays the sweep: all eleven channels stay inside
  // the continuity check's eight seconds and the cycle is no longer than before.
  for(uint8_t first:{1,6,11})for(uint8_t lock:{1,6,11}){auto after=hunt(first,lock,true,170,85),before=hunt(first,lock,false,170,85);
    assert(after.channelGap<8000&&after.lockGap<3000&&after.lockGap<before.lockGap&&after.cycleMs<8000&&after.cycleMs<=before.cycleMs+WifiSweep::VisitMs);
  }
  // Without time left, or without a lock, the sweep is the plain one.
  {WifiSweep w;w.begin(0,3);uint32_t now=0;for(unsigned i=0;i<11;++i){now+=200;assert(w.hop(now,0,4400)&&w.resume==0);}assert(w.full()&&w.current==3);}
  {WifiSweep w;w.begin(0,3);assert(w.hop(200,9,4200)&&w.current==9&&w.resume==4&&!w.hop(399,9,4000)&&w.hop(400,9,4000)&&w.current==4&&w.completed==1);
    assert(w.hop(600,9,10*WifiSweep::VisitMs-1)&&w.current==5&&w.completed==2);} // too little time: no visit
  puts("PASS: gap-aware signal filter and step budget; drain budget; advertised channel; eviction order; Wi-Fi Hunt lock visits");
}
