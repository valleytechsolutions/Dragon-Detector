#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace dragon::tx {
inline size_t makeBeacon(uint8_t* out,size_t cap,const uint8_t mac[6],uint8_t channel,const char* ssid){
 if(!out||!mac||!ssid||channel<1||channel>11)return 0;
 size_t n=strlen(ssid);if(!n||n>32||cap<51+n)return 0;
 memset(out,0,51+n);out[0]=0x80;memset(out+4,255,6);memcpy(out+10,mac,6);memcpy(out+16,mac,6);
 out[32]=100;out[34]=1;out[37]=n;memcpy(out+38,ssid,n);
 const uint8_t rates[]={1,8,0x82,0x84,0x8b,0x96,12,18,24,36,3,1,channel};memcpy(out+38+n,rates,sizeof(rates));return 51+n;
}
inline bool validLegacyAdvert(const uint8_t* p,size_t n){if(!p||!n||n>31)return false;size_t i=0;while(i<n){unsigned z=p[i++];if(!z)return i==n;if(z>n-i)return false;i+=z;}return true;}
inline bool beaconValid(const uint8_t* p,size_t n){if(!p||n<36||n>1024||(p[0]!=0x80&&p[0]!=0x50)||(p[1]&0x40))return false;for(size_t i=36;i<n;){if(n-i<2||p[i+1]>n-i-2)return false;i+=2+p[i+1];}return true;}
// Adjacent-channel reception must not move an AP's actual operating channel.
inline uint8_t beaconChannel(const uint8_t* p,size_t n,uint8_t received){
 if(!beaconValid(p,n))return received;uint8_t ht=0;
 for(size_t i=36;i<n;i+=2+p[i+1]){uint8_t id=p[i],len=p[i+1];
  if(id==3&&len==1&&p[i+2]>=1&&p[i+2]<=11)return p[i+2];
  if(id==61&&len>=1&&p[i+2]>=1&&p[i+2]<=11)ht=p[i+2];
 }return ht?ht:received;
}
inline size_t cloneBeacon(uint8_t* out,size_t cap,const uint8_t* p,size_t n,uint16_t seq,uint8_t id,uint8_t destination){
 if(!out||out==p||!beaconValid(p,n)||(id&&id!=37&&id!=40)||(id==37&&(destination<1||destination>11)))return 0;
 size_t needed=36;for(size_t at=36;at<n;at+=2+p[at+1])if(!id||p[at]!=id)needed+=2+p[at+1];
 needed+=id==37?5:id==40?8:0;if(needed>cap)return 0;
 memcpy(out,p,36);out[0]=0x80;out[1]=0;memset(out+4,255,6);out[22]=(seq&15)<<4;out[23]=seq>>4;
 size_t pos=36;for(size_t at=36;at<n;at+=2+p[at+1])if(!id||p[at]!=id){size_t size=2+p[at+1];memcpy(out+pos,p+at,size);pos+=size;}
 if(id==37){uint8_t ie[]={37,3,1,destination,1};memcpy(out+pos,ie,sizeof(ie));}
 if(id==40){uint8_t ie[]={40,6,1,1,0xff,0x7f,0,0};memcpy(out+pos,ie,sizeof(ie));}return needed;
}
inline size_t saeChallenge(const uint8_t* p,size_t n,const uint8_t ap[6],const uint8_t client[6],uint8_t* out,size_t cap){
 if(!p||!ap||!client||!out||n<=32||n-32>cap||p[0]!=0xb0||(p[1]&0x40)||memcmp(p+4,client,6)||memcmp(p+10,ap,6)||memcmp(p+16,ap,6)||p[24]!=3||p[25]||p[26]!=1||p[27]||p[28]!=76||p[29]||p[30]!=19||p[31])return 0;
 memcpy(out,p+32,n-32);return n-32;
}
// One ordinary ToDS/FromDS frame identifies the AP/client relationship. WDS,
// multicast, AP-to-AP and malformed headers cannot become selectable clients.
inline bool stationPair(const uint8_t* p,size_t n,uint8_t ap[6],uint8_t client[6]){
 if(!p||n<24||((p[0]>>2)&3)!=2)return false;unsigned ds=p[1]&3;if(ds!=1&&ds!=2)return false;
 const uint8_t* a=p+(ds==1?4:10);const uint8_t* c=p+(ds==1?10:4);
 auto uni=[](const uint8_t* m){uint8_t v=0;for(unsigned i=0;i<6;++i)v|=m[i];return v&&!(m[0]&1);};
 if(!uni(a)||!uni(c)||!memcmp(a,c,6))return false;memcpy(ap,a,6);memcpy(client,c,6);return true;
}
inline bool targetsFrame(const uint8_t* p,size_t n,const uint8_t ap[6]){return p&&n>=24&&(!memcmp(p+4,ap,6)||!memcmp(p+10,ap,6)||!memcmp(p+16,ap,6));}
inline void little32(uint8_t* p,uint32_t x){for(unsigned i=0;i<4;++i)p[i]=x>>(i*8);}
inline void pcapHeader(uint8_t* p){memset(p,0,24);little32(p,0xa1b2c3d4);p[4]=2;p[6]=4;little32(p+16,1037);little32(p+20,127);}
inline size_t pcapRecord(uint8_t* out,size_t cap,const uint8_t* p,size_t n,size_t original,uint64_t us,int rssi,unsigned ch){
 if(!out||!p||n>1024||n>original||original>65535||ch<1||ch>11||rssi<-127||rssi>0||cap<n+29)return 0;
 little32(out,uint32_t(us/1000000));little32(out+4,us%1000000);little32(out+8,n+13);little32(out+12,original+13);
 memset(out+16,0,13);out[18]=13;out[20]=0x28;unsigned freq=2407+5*ch;out[24]=freq;out[25]=freq>>8;out[26]=0x80;out[27]=0;out[28]=int8_t(rssi);memcpy(out+29,p,n);return n+29;
}
}
