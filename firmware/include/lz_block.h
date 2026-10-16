#pragma once
#include <stddef.h>
#include <stdint.h>
namespace dragon {
// Bounded LZ4 block decoder for the generated registry. No dynamic allocation.
inline bool unpackBlock(const uint8_t* src,size_t length,uint8_t* dst,size_t size){
 if(!src||!dst)return false;size_t in=0,out=0;
 auto extend=[&](size_t& n){if(n!=15)return true;uint8_t b;do{if(in==length)return false;b=src[in++];if(n>size||b>size-n)return false;n+=b;}while(b==255);return true;};
 while(in<length){uint8_t token=src[in++];size_t literal=token>>4;if(!extend(literal)||literal>length-in||literal>size-out)return false;
  for(size_t i=0;i<literal;++i)dst[out++]=src[in++];if(in==length)return out==size;
  if(length-in<2)return false;size_t offset=src[in]|size_t(src[in+1])<<8;in+=2;if(!offset||offset>out)return false;
  size_t match=token&15;if(!extend(match)||match>size-out||size-out-match<4)return false;match+=4;
  for(size_t i=0;i<match;++i){dst[out]=dst[out-offset];++out;}
 }return out==size;
}
}
