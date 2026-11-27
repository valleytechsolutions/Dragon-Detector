#pragma once
#include "mission_format.h"
#include <algorithm>

namespace dragon::p4 {
constexpr uint32_t SessionMinutes=5;
constexpr uint32_t SessionPeriod=SessionMinutes*60u*1000u;
inline bool inSession(uint32_t start,uint32_t at){return uint32_t(at-start)<SessionPeriod;}
inline bool isIBeacon(const uint8_t* data,size_t n){
  for(size_t pos=0;pos<n;){
    const size_t len=data[pos++];
    if(!len || len>n-pos)return false;
    if(len>=26 && data[pos]==0xff && data[pos+1]==0x4c && data[pos+2]==0 &&
        data[pos+3]==0x02 && data[pos+4]==0x15)return true;
    pos+=len;
  }
  return false;
}
// Rewrites the whole uncommitted suffix at its last acknowledged offset after
// a short write. Keeping the buffer until readback prevents duplicate records.
template<class File> bool commitRecords(File& file,uint32_t offset,const uint8_t* bytes,size_t size){
  if(!file || file.size()<offset || file.size()>offset+size || !file.seek(offset))return false;
  const bool wrote=file.write(bytes,size)==size;
  file.flush();
  if(!wrote || file.getWriteError() || file.size()!=offset+size || !file.seek(offset))return false;
  uint8_t check[96];
  for(size_t i=0;i<size;){
    const size_t part=std::min(sizeof(check),size-i);
    if(file.read(check,part)!=part || memcmp(check,bytes+i,part))return false;
    i+=part;
  }
  return true;
}
}
