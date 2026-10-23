#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <initializer_list>
namespace dragon {
// DDR1: LE header, sorted (prefix width, 36-bit left-aligned prefix) records.
// Pure parser shared by firmware and hostile-input host tests. Owns no memory.
class ResourceIndex {
 const uint8_t* data_=nullptr; uint32_t count_=0,names_=0;
 static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
 static uint64_t key(const uint8_t* p){return uint64_t(p[1])<<32|uint64_t(p[2])<<24|uint64_t(p[3])<<16|uint64_t(p[4])<<8|p[5];}
public:
 static constexpr size_t Header=32,Record=10,MaxBytes=3*1024*1024;
 static constexpr uint32_t MinimumSnapshot=20261015;
 const char* error="missing";
 uint32_t snapshot=0;
 bool load(const uint8_t* p,size_t n){
  data_=nullptr;count_=names_=snapshot=0;error="invalid header";
  if(!p||n<Header||n>MaxBytes||memcmp(p,"DDR1",4)||u32(p+4)!=1||u32(p+28)!=0)return false;
  uint32_t count=u32(p+8),names=u32(p+12),date=u32(p+20);
  if(!count||count>100000||!names||Header+uint64_t(count)*Record+names!=n||u32(p+24)!=Record)return false;
  error="outdated snapshot";if(date<MinimumSnapshot||date>20991231)return false;
  uint32_t crc=0xffffffff;
  for(size_t i=Header;i<n;++i){crc^=p[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
  error="corrupt checksum";if((crc^0xffffffff)!=u32(p+16))return false;
  const uint8_t* pool=p+Header+count*Record;
  error="invalid labels";
  // Validate each string once, then offsets need only point at string starts.
  unsigned length=0;for(uint32_t i=0;i<names;++i){if(!pool[i]){if(!length)return false;length=0;}else{if(pool[i]<32||pool[i]==127||++length>255)return false;}}
  if(length||pool[names-1])return false;
  uint8_t previousBits=0;uint64_t previousKey=0;
  error="invalid index";
  for(uint32_t i=0;i<count;++i){const uint8_t* r=p+Header+i*Record;uint8_t bits=r[0];uint64_t k=key(r);uint32_t off=u32(r+6);
   if((bits!=24&&bits!=28&&bits!=36)||k>0xfffffffffULL||(k&((1ULL<<(36-bits))-1))||off>=names||(off&&pool[off-1])||!pool[off])return false;
   if(i&&(bits<previousBits||(bits==previousBits&&k<=previousKey)))return false;
   previousBits=bits;previousKey=k;
  }
  data_=p;count_=count;names_=names;snapshot=date;error="ready";return true;
 }
 uint32_t count()const{return count_;}
 const char* lookup(const uint8_t* mac,bool ble=false,uint8_t type=0)const{
  if(!data_||!mac||(mac[0]&3)||(ble&&(type==1||type==3||type>3)))return nullptr;
  uint64_t address=uint64_t(mac[0])<<28|uint64_t(mac[1])<<20|uint64_t(mac[2])<<12|uint64_t(mac[3])<<4|mac[4]>>4;
  for(uint8_t bits:{uint8_t(36),uint8_t(28),uint8_t(24)}){
   uint64_t wanted=address&~((1ULL<<(36-bits))-1);uint32_t lo=0,hi=count_;
   while(lo<hi){uint32_t mid=lo+(hi-lo)/2;const uint8_t* r=data_+Header+mid*Record;if(r[0]<bits||(r[0]==bits&&key(r)<wanted))lo=mid+1;else hi=mid;}
   if(lo<count_){const uint8_t* r=data_+Header+lo*Record;if(r[0]==bits&&key(r)==wanted)return reinterpret_cast<const char*>(data_+Header+count_*Record+u32(r+6));}
  }return nullptr;
 }
};
}
