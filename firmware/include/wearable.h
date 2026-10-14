#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

namespace dragon {
// Elapsed time uses a monotonic 64-bit clock, independent of RTC/timezone edits.
class Stopwatch {
 public:
  struct Lap { uint32_t number=0; uint64_t split=0,total=0; };
  static constexpr size_t Capacity=50;
  bool running=false;
  uint32_t lapCount=0;
  uint64_t elapsed(uint64_t now) const { return accumulated+(running&&now>=started?now-started:0); }
  void start(uint64_t now) { if(!running){started=now;running=true;} }
  void pause(uint64_t now) { if(running){accumulated=elapsed(now);running=false;} }
  void toggle(uint64_t now) { if(running)pause(now);else start(now); }
  bool lap(uint64_t now) {
    uint64_t total=elapsed(now);if(!running||total<=lastLap)return false;
    laps[lapCount%Capacity]={lapCount+1,total-lastLap,total};++lapCount;lastLap=total;return true;
  }
  bool reset() { if(running)return false;*this=Stopwatch{};return true; }
  size_t retained() const {return std::min<size_t>(lapCount,Capacity);}
  const Lap* newest(size_t index=0) const {return index<retained()?&laps[(lapCount-1-index)%Capacity]:nullptr;}
 private:
  uint64_t started=0,accumulated=0,lastLap=0;
  Lap laps[Capacity]{};
};

struct Calendar {int year=2026,month=1,day=1,hour=0,minute=0,second=0;};
inline bool leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
inline int monthDays(int y,int m){static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};return m>=1&&m<=12?days[m-1]+(m==2&&leap(y)):0;}
inline bool validDate(const Calendar& d){return d.year>=2000&&d.year<=2099&&d.month>=1&&d.month<=12&&d.day>=1&&d.day<=monthDays(d.year,d.month)&&d.hour>=0&&d.hour<24&&d.minute>=0&&d.minute<60&&d.second>=0&&d.second<60;}
inline uint32_t dateEpoch(const Calendar& d){
  if(!validDate(d))return 0;uint32_t days=10957;
  for(int y=2000;y<d.year;++y)days+=leap(y)?366:365;
  for(int m=1;m<d.month;++m)days+=monthDays(d.year,m);
  return (days+d.day-1)*86400u+d.hour*3600u+d.minute*60u+d.second;
}
inline bool validEpoch(uint64_t t){return t>=946684800ull&&t<=4102444799ull;}

// LVGL 8's quarter-turn enum rotates content counterclockwise on the panel.
// Keep these transforms shared with tests and the display flush wrapper.
struct PixelPoint {int x,y;};
inline PixelPoint rotatePoint(int x,int y,int size,uint8_t r){
  switch(r&3){case 1:return {y,size-1-x};case 2:return {size-1-x,size-1-y};case 3:return {size-1-y,x};default:return {x,y};}
}
inline PixelPoint unrotatePoint(int x,int y,int size,uint8_t r){return rotatePoint(x,y,size,(4-r)&3);}

class StableOrientation {
 public:
  static constexpr uint32_t HoldMs=1200,CooldownMs=1500;
  uint8_t rotation=0,reference=0;
  bool automatic=false;
  float x=0,y=0,z=0;
  uint32_t changes=0;
  // Calibration establishes which sensor edge is the user's upright edge.
  bool calibrate(uint32_t now){if(!initialized||now-lastSample>200||!stable||now-stableAt<HoldMs)return false;reference=(quadrant()+4-rotation)&3;resetCandidate();return true;}
  void lock(uint8_t r){automatic=false;rotation=r&3;resetCandidate();}
  void setAuto(bool on){automatic=on;resetCandidate();}
  void missing(){initialized=false;stable=false;resetCandidate();}
  bool sample(float ax,float ay,float az,uint32_t now,bool interacting){
    if(!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)){missing();return false;}
    float magnitude=ax*ax+ay*ay+az*az;
    bool gap=!initialized||uint32_t(now-lastSample)>200;
    float motion=gap?1:std::fabs(ax-rawX)+std::fabs(ay-rawY)+std::fabs(az-rawZ);
    rawX=ax;rawY=ay;rawZ=az;lastSample=now;
    if(gap){x=ax;y=ay;z=az;initialized=true;resetCandidate();stable=false;}
    else{x+=(ax-x)*0.2f;y+=(ay-y)*0.2f;z+=(az-z)*0.2f;}
    float major=std::max(std::fabs(x),std::fabs(y)),minor=std::min(std::fabs(x),std::fabs(y));
    bool usable=magnitude>=0.64f&&magnitude<=1.44f&&motion<0.14f&&std::fabs(z)<0.75f&&major>0.65f&&major>minor*1.65f;
    if(!usable){stable=false;resetCandidate();return false;}
    if(!stable){stable=true;stableAt=now;}
    if(!automatic||interacting){resetCandidate();return false;}
    uint8_t desired=(quadrant()+4-reference)&3;
    if(desired==rotation){resetCandidate();return false;}
    if(candidate!=desired){candidate=desired;candidateAt=now;return false;}
    if(now-candidateAt<HoldMs|| (hasChanged&&now-changedAt<CooldownMs))return false;
    rotation=desired;changedAt=now;hasChanged=true;++changes;resetCandidate();return true;
  }
 private:
  uint8_t quadrant() const {return std::fabs(y)>=std::fabs(x)?(y>=0?0:2):(x>=0?1:3);}
  void resetCandidate(){candidate=255;}
  uint8_t candidate=255;
  bool initialized=false,stable=false,hasChanged=false;
  float rawX=0,rawY=0,rawZ=0;
  uint32_t lastSample=0,stableAt=0,candidateAt=0,changedAt=0;
};
}
