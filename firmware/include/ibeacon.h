#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

namespace dragon::ibeacon {
struct Identity {
  uint8_t uuid[16]{0x44,0x52,0x41,0x47,0x4f,0x4e,0x4c,0x41,0x42,0,0,0,0,0,0,1};
  uint16_t major=1,minor=1;
  int8_t power=-59;
};
inline bool parseUuid(const char* text,uint8_t out[16]) {
  if(!text||!out||strlen(text)!=36)return false;
  uint8_t value[16];unsigned at=0;
  auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
  for(unsigned i=0;i<16;++i){if(i==4||i==6||i==8||i==10){if(text[at++]!='-')return false;}int a=hex(text[at++]),b=hex(text[at++]);if(a<0||b<0)return false;value[i]=(a<<4)|b;}
  memcpy(out,value,16);return true;
}
inline void uuidText(const Identity& value,char out[37]) {
  unsigned at=0;for(unsigned i=0;i<16;++i){if(i==4||i==6||i==8||i==10)out[at++]='-';snprintf(out+at,3,"%02x",value.uuid[i]);at+=2;}out[36]=0;
}
inline size_t encode(uint8_t* out,size_t capacity,const Identity& value) {
  if(!out||capacity<30)return 0;
  const uint8_t prefix[]={2,1,6,26,0xff,0x4c,0,2,0x15};memcpy(out,prefix,sizeof(prefix));memcpy(out+9,value.uuid,16);
  out[25]=value.major>>8;out[26]=value.major;out[27]=value.minor>>8;out[28]=value.minor;out[29]=uint8_t(value.power);return 30;
}
inline bool decode(const uint8_t* data,size_t length,Identity& out) {
  if(!data||length>31)return false;Identity found;bool match=false;
  for(size_t i=0;i<length;){size_t n=data[i++];if(!n){for(;i<length;++i)if(data[i])return false;break;}if(n>length-i)return false;
    if(n==26&&data[i]==0xff&&data[i+1]==0x4c&&data[i+2]==0&&data[i+3]==2&&data[i+4]==0x15){if(match)return false;memcpy(found.uuid,data+i+5,16);found.major=(uint16_t(data[i+21])<<8)|data[i+22];found.minor=(uint16_t(data[i+23])<<8)|data[i+24];found.power=int8_t(data[i+25]);match=true;}i+=n;
  }
  if(match)out=found;return match;
}
}
