#pragma once
#include <stdint.h>
#include <stddef.h>
namespace dragon::gpsuart {
// H2 expansion header only. Never offer native USB, SD, audio or display pins.
constexpr int Pins[]={18,17,16,44};
constexpr unsigned HeaderPins[]={7,6,8,4};
constexpr uint32_t Bauds[]={9600,19200,38400,57600,115200,4800};
struct Config {bool enabled=false;int rx=18;uint32_t baud=9600;};
template<class T,size_t N> int index(const T (&list)[N],T value){for(size_t i=0;i<N;++i)if(list[i]==value)return int(i);return -1;}
inline bool valid(const Config& c){return index(Pins,c.rx)>=0&&index(Bauds,c.baud)>=0;}
inline uint32_t encode(const Config& c){return valid(c)?0x47500100u|(uint32_t(index(Bauds,c.baud))<<3)|(uint32_t(index(Pins,c.rx))<<1)|unsigned(c.enabled):0;}
inline bool decode(uint32_t value,Config& c){
  if((value&0xffffffc0u)!=0x47500100u)return false;
  unsigned pin=(value>>1)&3,baud=(value>>3)&7;if(baud>=sizeof(Bauds)/sizeof(*Bauds))return false;
  c={bool(value&1),Pins[pin],Bauds[baud]};return true;
}
inline unsigned headerPin(int pin){int i=index(Pins,pin);return i<0?0:HeaderPins[i];}
}
