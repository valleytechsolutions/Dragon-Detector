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
  bool running=false,finished=false;
  uint32_t lapCount=0;
  uint64_t elapsed(uint64_t now) const { return accumulated+(running&&now>=started?now-started:0); }
  void start(uint64_t now) { if(!running){started=now;running=true;finished=false;} }
  void pause(uint64_t now) { if(running){accumulated=elapsed(now);running=false;} }
  void toggle(uint64_t now) { if(running)pause(now);else start(now); }
  bool lap(uint64_t now) {
    uint64_t total=elapsed(now);if(!running||total<=lastLap)return false;
    laps[lapCount%Capacity]={lapCount+1,total-lastLap,total};++lapCount;lastLap=total;return true;
  }
  bool reset() { if(running)return false;*this=Stopwatch{};return true; }
  // Capture the final split and stop at the same input timestamp.
  bool finish(uint64_t now) { if(!lap(now))return false;pause(now);finished=true;return true; }
  size_t retained() const {return std::min<size_t>(lapCount,Capacity);}
  const Lap* newest(size_t index=0) const {return index<retained()?&laps[(lapCount-1-index)%Capacity]:nullptr;}
 private:
  uint64_t started=0,accumulated=0,lastLap=0;
  Lap laps[Capacity]{};
};

struct Calendar {int year=2026,month=1,day=1,hour=0,minute=0,second=0;};
// Calendar/RTC storage stays in 24-hour form; the normal UI is 12-hour.
inline int hour12(int hour){return hour%12==0?12:hour%12;}
inline int hour24(int hour,bool pm){return hour%12+(pm?12:0);}
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

}
