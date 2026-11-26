#pragma once
#include <cstddef>
#include <cstdint>

namespace dragon {
inline bool ft3168Point(const uint8_t* report,size_t length,int& x,int& y){
  if(!report||length!=5)return false;
  const unsigned contacts=report[0]&15,event=report[1]>>6;
  if(!contacts||contacts>2||(event!=0&&event!=2))return false;
  const int nextX=((report[1]&15)<<8)|report[2];
  const int nextY=((report[3]&15)<<8)|report[4];
  if(nextX>=410||nextY>=502)return false;
  x=nextX;y=nextY;return true;
}
}
