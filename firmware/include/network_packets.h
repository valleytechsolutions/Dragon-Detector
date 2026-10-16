#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace dragon::net {
// Host-order addresses throughout this helper; socket boundaries convert once.
inline bool ipv4(const char* s,uint32_t& value){
 value=0;if(!s)return false;
 for(unsigned part=0;part<4;++part){unsigned n=0,digits=0;while(*s>='0'&&*s<='9'){if(++digits>3)return false;n=n*10+unsigned(*s++-'0');if(n>255)return false;}if(!digits)return false;value=(value<<8)|n;if(part<3){if(*s++!='.')return false;}else if(*s)return false;}return true;
}
inline bool host(uint32_t address,uint32_t own,uint32_t mask){
 uint32_t inverse=~mask;return mask&&inverse>=3&&!(inverse&(inverse+1))&&address!=own&&(address&mask)==(own&mask)&&(address&inverse)!=0&&(address&inverse)!=inverse;
}
inline bool range(uint32_t first,uint32_t last,uint32_t own,uint32_t mask){
 // A scan is restricted to at most 254 addresses in the joined subnet.
 return first<=last&&last-first<254&&(first&mask)==(last&mask)&&host(first,own,mask)&&host(last,own,mask);
}
inline uint16_t checksum(const uint8_t* p,size_t n){uint32_t s=0;while(n>1){s+=(uint16_t(p[0])<<8)|p[1];p+=2;n-=2;}if(n)s+=uint16_t(*p)<<8;while(s>>16)s=(s&65535)+(s>>16);return uint16_t(~s);}
inline void put16(uint8_t* p,uint16_t n){p[0]=n>>8;p[1]=n;}
inline uint16_t get16(const uint8_t* p){return uint16_t(p[0])<<8|p[1];}
inline void echo(uint8_t* p,uint16_t id,uint16_t sequence,uint32_t nonce){
 memset(p,0,16);p[0]=8;put16(p+4,id);put16(p+6,sequence);memcpy(p+8,"DDP1",4);for(unsigned i=0;i<4;++i)p[12+i]=nonce>>(24-8*i);put16(p+2,checksum(p,16));
}
inline bool reply(const uint8_t* p,size_t n,uint16_t id,uint16_t sequence,uint32_t nonce){
 if(!p||n<36||(p[0]>>4)!=4)return false;size_t h=(p[0]&15)*4;size_t total=get16(p+2);
 if(h<20||h>60||total>n||total!=h+16||p[9]!=1||(get16(p+6)&0x3fff)||checksum(p,h))return false;
 p+=h;if(p[0]||p[1]||checksum(p,16)||get16(p+4)!=id||get16(p+6)!=sequence||memcmp(p+8,"DDP1",4))return false;
 uint32_t expected=uint32_t(p[12])<<24|uint32_t(p[13])<<16|uint32_t(p[14])<<8|p[15];return expected==nonce;
}
}
