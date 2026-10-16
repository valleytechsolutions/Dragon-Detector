#pragma once
// Independent encoding of Marauder's documented RCN1 / PRB1 / REL1 records.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace dragon::mission {
constexpr size_t ObservationSize=21,ProbeSize=45,RelationSize=12;
inline void prefix(uint8_t* out,uint32_t at,const uint8_t* mac,int rssi,uint8_t channel){memset(out,0,20);for(unsigned i=0;i<4;++i)out[i]=at>>(i*8);memcpy(out+12,mac,6);out[18]=uint8_t(rssi);out[19]=channel;}
inline void observation(uint8_t out[ObservationSize],uint32_t at,const uint8_t* mac,int rssi,uint8_t channel,char type){prefix(out,at,mac,rssi,channel);out[20]=type;}
inline void probe(uint8_t out[ProbeSize],uint32_t at,const uint8_t* mac,int rssi,uint8_t channel,const uint8_t* ssid,size_t size){memset(out,0,ProbeSize);prefix(out,at,mac,rssi,channel);size=size>24?24:size;out[20]=size;if(size)memcpy(out+21,ssid,size);}
struct Seen {uint8_t mac[6]{},type=0,addressType=0;uint32_t at=0;int8_t rssi=0;};
// A bounded identity table never evicts: overflow is reported as dropped data,
// not counted repeatedly as newly discovered devices. Return 1=new, 2=repeat.
inline unsigned accept(Seen* rows,unsigned capacity,const uint8_t* mac,uint8_t type,uint8_t addressType,int rssi,uint32_t at,bool& full){full=false;for(unsigned i=0;i<capacity;++i){auto& r=rows[i];if(!r.type){memcpy(r.mac,mac,6);r.type=type;r.addressType=addressType;r.rssi=rssi;r.at=at;return 1;}if(r.type==type&&r.addressType==addressType&&!memcmp(mac,r.mac,6)){if(at-r.at<30000&&rssi-r.rssi<12&&r.rssi-rssi<12)return 0;r.at=at;r.rssi=rssi;return 2;}}full=true;return 0;}
}
