#include "vendor_registry.h"
#include <stddef.h>
namespace dragon {
#include "vendor_registry_data.inc"
unsigned vendorRegistryCount(){return sizeof(vendorIndex)/6;}
const char* registeredVendor(const uint8_t* mac,bool ble,uint8_t type){
 if(!mac||(ble&&(type==1||type==3||type>3))||(mac[0]&3))return nullptr;
 uint32_t key=uint32_t(mac[0])<<16|uint32_t(mac[1])<<8|mac[2];size_t lo=0,hi=vendorRegistryCount();
 while(lo<hi){size_t mid=lo+(hi-lo)/2;auto* p=vendorIndex+mid*6;uint32_t prefix=uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];if(prefix<key)lo=mid+1;else hi=mid;}
 if(lo==vendorRegistryCount())return nullptr;auto* p=vendorIndex+lo*6;
 if((uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2])!=key)return nullptr;
 size_t offset=p[3]|size_t(p[4])<<8|size_t(p[5])<<16;
 return offset<sizeof(vendorNames)?reinterpret_cast<const char*>(vendorNames+offset):nullptr;
}
}
