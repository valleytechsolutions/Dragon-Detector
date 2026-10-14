#include "wearable.h"
#include <limits>
static void wearableTests(){
  Stopwatch s;assert(s.elapsed(99)==0&&!s.lap(99));s.start(1000000);s.start(2000000);
  assert(s.elapsed(2500000)==1500000);assert(s.lap(2500000));assert(!s.lap(2500000));
  s.pause(3000000);assert(s.elapsed(999000000)==2000000);assert(!s.lap(4000000));
  s.start(9000000000ull);assert(s.lap(9000500000ull));assert(s.newest()->split==1000000&&s.newest()->total==2500000);
  assert(!s.reset());s.pause(9000600000ull);assert(s.reset()&&s.retained()==0);
  s.start(0);for(int i=1;i<=75;++i)assert(s.lap(i*1000000ull));
  assert(s.retained()==50&&s.newest()->number==75&&s.newest(49)->number==26&&s.newest(50)==nullptr);
  assert(s.elapsed(50ull*86400000000ull)==50ull*86400000000ull); // beyond millis() wrap
  assert(dateEpoch({2000,1,1,0,0,0})==946684800u);
  assert(dateEpoch({2024,2,29,0,0,0})==1709164800u);
  assert(dateEpoch({2099,12,31,23,59,59})==4102444799u);
  for(Calendar d: {Calendar{2023,2,29},Calendar{2024,4,31},Calendar{2026,0,1},Calendar{2100,1,1},Calendar{2026,1,1,24},Calendar{2026,1,1,0,60},Calendar{2026,1,1,0,0,60}})assert(dateEpoch(d)==0);
  // Every pixel survives the display/input inverse, at both panel sizes.
  for(int size:{360,466})for(int r=0;r<4;++r)for(int y=0;y<size;++y)for(int x=0;x<size;++x){auto a=rotatePoint(x,y,size,r),b=unrotatePoint(a.x,a.y,size,r);assert(a.x>=0&&a.y>=0&&a.x<size&&a.y<size&&b.x==x&&b.y==y);}
  StableOrientation o;o.setAuto(true);uint32_t at=100;
  auto feed=[&](float x,float y,float z,int ms,bool touch=false){for(int i=0;i<ms/50;++i){at+=50;o.sample(x,y,z,at,touch);}};
  feed(1,0,0,1200);assert(o.rotation==0);feed(1,0,0,200);assert(o.rotation==1);
  feed(0,0,1,4000);assert(o.rotation==1); // flat
  feed(.71f,.71f,0,4000);assert(o.rotation==1); // diagonal ambiguity
  feed(0,-1,0,4000,true);assert(o.rotation==1); // touching freezes rotation
  feed(0,-1,0,1400);assert(o.rotation==2);
  o.lock(3);feed(0,1,0,4000);assert(o.rotation==3);
  assert(o.calibrate(at));o.setAuto(true);feed(0,1,0,2000);assert(o.rotation==3); // calibration preserves current angle
  o.lock(0);o.reference=0;o.setAuto(true);
  for(int i=0;i<30;++i){feed(1,0,0,500);feed(0,1,0,500);}assert(o.rotation==0); // dangling badge
  feed(1,0,0,1100);o.missing();feed(1,0,0,1100);assert(o.rotation==0);feed(1,0,0,300);assert(o.rotation==1);
  o.lock(0);o.setAuto(true);feed(2,0,0,3000);assert(o.rotation==0); // acceleration burst
  o.sample(std::numeric_limits<float>::quiet_NaN(),0,0,at+50,false);assert(!o.calibrate(at+50));
  // A timer rollover cannot skip the stable hold requirement.
  o=StableOrientation{};o.setAuto(true);at=0xfffffc00u;feed(-1,0,0,1200);assert(o.rotation==0);feed(-1,0,0,200);assert(o.rotation==3);
  puts("Wearable: stopwatch/laps, calendar, all rotation pixels, tilt jitter/motion/touch/gaps/wrap passed");
}
