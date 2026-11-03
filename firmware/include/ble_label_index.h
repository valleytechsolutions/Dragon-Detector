#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace dragon {
// DDB1 contains display names, never detector rules. Its typed key prevents a
// Bluetooth company ID from being confused with an assigned service UUID.
// Caller owns immutable storage for the lifetime of returned label pointers.
class BleLabelIndex {
 const uint8_t* data_=nullptr;uint32_t count_=0;
 static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
 static uint32_t key(const uint8_t* p){return uint32_t(p[0])<<16|uint32_t(p[2])|uint32_t(p[3])<<8;}
public:
 enum Kind:uint8_t {Uuid16=1,Company16=2};
 static constexpr size_t Header=32,Record=8,MaxBytes=65536;
 const char* error="missing";uint32_t snapshot=0;
 bool load(const uint8_t* p,size_t n){
  data_=nullptr;count_=snapshot=0;error="invalid header";
  if(!p||n<Header||n>MaxBytes||memcmp(p,"DDB1",4)||u32(p+4)!=1||u32(p+24)!=Record||u32(p+28))return false;
  const uint32_t count=u32(p+8),names=u32(p+12),date=u32(p+20);
  if(!count||count>4096||!names||Header+uint64_t(count)*Record+names!=n||date<20261103||date>20991231)return false;
  uint32_t crc=0xffffffff;for(size_t i=0;i<n;++i){crc^=(i>=16&&i<20)?0:p[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
  error="corrupt checksum";if((crc^0xffffffff)!=u32(p+16))return false;
  const uint8_t* pool=p+Header+count*Record;unsigned length=0;error="invalid labels";
  // Printable ASCII is the version-one contract; no control sequences, invalid
  // UTF-8 or unbounded strings can reach the on-device UI or CSV metadata.
  for(uint32_t i=0;i<names;++i){if(!pool[i]){if(!length)return false;length=0;}else if(pool[i]<32||pool[i]>126||++length>96)return false;}
  if(length||pool[names-1])return false;
  uint32_t previous=0;error="invalid index";
  for(uint32_t i=0;i<count;++i){const uint8_t* r=p+Header+i*Record;uint32_t k=key(r),offset=u32(r+4);
   if((r[0]!=Uuid16&&r[0]!=Company16)||r[1]||(i&&k<=previous)||offset>=names||(offset&&pool[offset-1])||!pool[offset])return false;previous=k;
  }
  data_=p;count_=count;snapshot=date;error="ready";return true;
 }
 uint32_t count()const{return count_;}
 const char* lookup(uint8_t kind,uint16_t value)const{
  if(!data_||(kind!=Uuid16&&kind!=Company16))return nullptr;
  const uint32_t wanted=uint32_t(kind)<<16|value;uint32_t lo=0,hi=count_;
  while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(key(data_+Header+mid*Record)<wanted)lo=mid+1;else hi=mid;}
  if(lo==count_)return nullptr;const uint8_t* r=data_+Header+lo*Record;
  return key(r)==wanted?reinterpret_cast<const char*>(data_+Header+count_*Record+u32(r+4)):nullptr;
 }
};
}
