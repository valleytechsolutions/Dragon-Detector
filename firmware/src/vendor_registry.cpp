#include "vendor_registry.h"
#include <stddef.h>
#include <stdlib.h>
#include "lz_block.h"
#ifdef ARDUINO
#include <esp32-hal-psram.h>
#endif
namespace dragon {
#include "vendor_registry_data.inc"
static uint8_t* vendorData=nullptr;
static bool vendorAttempted=false;
bool vendorRegistryBegin(){
 if(vendorData)return true;
 if(vendorAttempted)return false;vendorAttempted=true;
#ifdef ARDUINO
 auto* data=(uint8_t*)ps_malloc(VendorIndexBytes+VendorNamesBytes);
#else
 auto* data=(uint8_t*)malloc(VendorIndexBytes+VendorNamesBytes);
#endif
 if(!data)return false;if(!unpackBlock(vendorPacked,sizeof(vendorPacked),data,VendorIndexBytes+VendorNamesBytes)){free(data);return false;}
 uint32_t crc=0xffffffff;for(size_t i=0;i<VendorIndexBytes+VendorNamesBytes;++i){crc^=data[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
 if((crc^0xffffffff)!=VendorCrc){free(data);return false;}
 vendorData=data;return true;
}
unsigned vendorRegistryCount(){return VendorIndexBytes/6;}
const char* registeredVendor(const uint8_t* mac,bool ble,uint8_t type){
 if(!mac||(ble&&(type==1||type==3||type>3))||(mac[0]&3))return nullptr;
 if(!vendorRegistryBegin())return nullptr;const auto* vendorIndex=vendorData;const auto* vendorNames=vendorData+VendorIndexBytes;
 uint32_t key=uint32_t(mac[0])<<16|uint32_t(mac[1])<<8|mac[2];size_t lo=0,hi=vendorRegistryCount();
 while(lo<hi){size_t mid=lo+(hi-lo)/2;auto* p=vendorIndex+mid*6;uint32_t prefix=uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];if(prefix<key)lo=mid+1;else hi=mid;}
 if(lo==vendorRegistryCount())return nullptr;auto* p=vendorIndex+lo*6;
 if((uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2])!=key)return nullptr;
 size_t offset=p[3]|size_t(p[4])<<8|size_t(p[5])<<16;
 return offset<VendorNamesBytes?reinterpret_cast<const char*>(vendorNames+offset):nullptr;
}
}
