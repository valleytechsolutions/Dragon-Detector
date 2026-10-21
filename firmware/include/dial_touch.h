#pragma once
#include <stddef.h>
#include <stdint.h>
namespace dragon {
struct DialTouch {bool pressed=false;int16_t x=0,y=0;};
// FT3267 registers 0x02..0x06: contact count, event/XH, XL, ID/YH, YL.
inline bool decodeDialTouch(const uint8_t* bytes,size_t size,DialTouch& out){
  if(!bytes||size!=5)return false;
  const unsigned count=bytes[0]&15,event=bytes[1]>>6;
  if(!count||event==1){out.pressed=false;return true;}
  if(count!=1||event==3)return false;
  const unsigned x=((bytes[1]&15)<<8)|bytes[2],y=((bytes[3]&15)<<8)|bytes[4];
  if(x>=240||y>=240)return false;
  out={true,int16_t(x),int16_t(y)};return true;
}
}
