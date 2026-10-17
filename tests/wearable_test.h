#include "wearable.h"
#include "radar_colors.h"
#include <limits>
static void wearableTests(){
  for(int h=0;h<24;++h){assert(hour12(h)>=1&&hour12(h)<=12);assert(hour24(hour12(h),h>=12)==h);}
  assert(hour24(12,false)==0&&hour24(12,true)==12&&hour12(0)==12&&hour12(23)==11);
  Stopwatch s;assert(s.elapsed(99)==0&&!s.lap(99));s.start(1000000);s.start(2000000);
  assert(s.elapsed(2500000)==1500000);assert(s.lap(2500000));assert(!s.lap(2500000));
  s.pause(3000000);assert(s.elapsed(999000000)==2000000);assert(!s.lap(4000000));
  s.start(9000000000ull);assert(s.lap(9000500000ull));assert(s.newest()->split==1000000&&s.newest()->total==2500000);
  assert(!s.reset());s.pause(9000600000ull);assert(s.reset()&&s.retained()==0);
  s.start(0);for(int i=1;i<=75;++i)assert(s.lap(i*1000000ull));
  assert(s.retained()==50&&s.newest()->number==75&&s.newest(49)->number==26&&s.newest(50)==nullptr);
  assert(s.elapsed(50ull*86400000000ull)==50ull*86400000000ull); // beyond millis() wrap
  Stopwatch pick;assert(!pick.finish(100));pick.start(100);assert(!pick.finish(100)&&pick.running);
  assert(pick.lap(1000100));pick.pause(2000100);pick.start(9000100);
  assert(pick.finish(10000100)&&!pick.running&&pick.finished);
  assert(pick.elapsed(999999999)==3000000&&pick.newest()->total==3000000&&pick.newest()->split==2000000);
  assert(!pick.finish(10000200)&&pick.lapCount==2);pick.start(11000100);assert(!pick.finished);
  pick.pause(12000100);assert(pick.elapsed(0)==4000000);assert(pick.reset()&&!pick.finished&&!pick.retained());
  assert(dateEpoch({2000,1,1,0,0,0})==946684800u);
  assert(dateEpoch({2024,2,29,0,0,0})==1709164800u);
  assert(dateEpoch({2099,12,31,23,59,59})==4102444799u);
  ClockEdit edit;edit.begin({2026,10,17,9,30,42},true);
  edit.draft.day=20;
  auto dateOnly=edit.merge({2026,10,17,9,35,16},true,true);
  assert(dateOnly.day==20&&dateOnly.hour==9&&dateOnly.minute==35&&dateOnly.second==16);
  edit.draft.hour=0;edit.draft.minute=5;
  auto timeOnly=edit.merge({2026,10,18,0,1,20},true,false);
  assert(timeOnly.day==18&&timeOnly.hour==0&&timeOnly.minute==5&&timeOnly.second==0);
  edit.begin({},false);assert(!edit.confirm(false));assert(edit.confirm(true));
  edit.begin({},false);assert(!edit.confirm(true));assert(edit.confirm(false));
  edit.draft={2028,2,31};edit.clampDay();assert(edit.draft.day==29);
  edit.draft.year=2027;edit.clampDay();assert(edit.draft.day==28);
  for(Calendar d: {Calendar{2023,2,29},Calendar{2024,4,31},Calendar{2026,0,1},Calendar{2100,1,1},Calendar{2026,1,1,24},Calendar{2026,1,1,0,60},Calendar{2026,1,1,0,0,60}})assert(dateEpoch(d)==0);
  // Every pixel survives the display/input inverse, at all panel sizes.
  for(int size:{240,360,466})for(int r=0;r<4;++r)for(int y=0;y<size;++y)for(int x=0;x<size;++x){auto a=rotatePoint(x,y,size,r),b=unrotatePoint(a.x,a.y,size,r);assert(a.x>=0&&a.y>=0&&a.x<size&&a.y<size&&b.x==x&&b.y==y);}
  auto luminance=[](uint32_t c){auto channel=[](int x){float v=x/255.f;return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};return .2126f*channel(c>>16)+.7152f*channel((c>>8)&255)+.0722f*channel(c&255);};
  auto rgb565=[](uint32_t c){return ((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31);};
  for(unsigned i=0;i<unsigned(Family::Count);++i){uint32_t c=familyColor(Family(i));assert((luminance(c)+.05f)/(luminance(RadarLabelBackground)+.05f)>=4.5f);for(unsigned j=0;j<i;++j)assert(rgb565(c)!=rgb565(familyColor(Family(j))));}
  puts("Wearable: stopwatch/laps, calendar and every rotation pixel passed");
}
