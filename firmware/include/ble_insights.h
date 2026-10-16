#pragma once
// Independent BLE advertising and HID metadata decoders. No upstream firmware code.
#include "detection.h"
#include "ble_profile.h"
namespace dragon::lab {
inline const char* uuidName(uint16_t u){switch(u){
 case 0x1800:return "Generic Access";case 0x1801:return "Generic Attribute";case 0x180a:return "Device Information";
 case 0x180d:return "Heart Rate";case 0x180f:return "Battery";case 0x1812:return "Human Interface Device";
 case 0x1816:return "Cycling Speed/Cadence";case 0x181a:return "Environmental Sensing";case 0x1826:return "Fitness Machine";
 case 0x2a00:return "Device Name";case 0x2a19:return "Battery Level";case 0x2a29:return "Manufacturer Name";
 case 0x2a24:return "Model Number";case 0x2a25:return "Serial Number";case 0x2a26:return "Firmware Revision";
 case 0x2a4a:return "HID Information";case 0x2a4b:return "HID Report Map";case 0x2a4c:return "HID Control Point";
 case 0x2a4d:return "HID Report";case 0x2a4e:return "HID Protocol Mode";case 0x2901:return "User Description";
 case 0x2902:return "Notification Configuration";case 0x2908:return "HID Report Reference";
 case 0xfe2c:return "Google Fast Pair";case 0xfeaa:return "Eddystone / Find Hub";case 0xfcd2:return "BTHome";
 case 0xfd6f:return "Exposure Notification";case 0xfe95:return "Xiaomi MiBeacon";case 0xfeed:case 0xfeec:case 0xfd84:return "Tile";
 default:return nullptr;}}
inline const char* uuidName(const char* u){if(!uuidValid(u))return nullptr;char full[37];uuidCanonical(u,full);if(strncmp(full,"0000",4)||strcmp(full+8,"-0000-1000-8000-00805f9b34fb"))return nullptr;unsigned v=0;sscanf(full+4,"%4x",&v);return uuidName(uint16_t(v));}
inline const char* companyName(uint16_t c){switch(c){case 0x004c:return "Apple";case 0x0006:return "Microsoft";case 0x0075:return "Samsung";case 0x00e0:return "Google";case 0x034d:return "Axon / TASER";case 0x01ab:return "Meta";case 0x058e:return "Meta Technologies";case 0x0d53:return "Luxottica";case 0x03c2:return "Snap";case 0x060c:return "Vuzix";case 0x08c3:return "Chipolo";case 0x0e29:return "Flipper Devices (includes BUSY Bar)";case 0x09c8:return "Xuntong module";default:return "Unresolved company ID";}}
struct AdvertInsight {bool valid=false,hasPower=false;int8_t power=0;uint16_t appearance=0,company=0xffff;const char* protocol="Unclassified advertisement";const char* service=nullptr;unsigned fields=0;};
inline AdvertInsight inspectAdvertisement(const uint8_t* p,size_t n,size_t boundary){
 AdvertInsight r;if(!p||boundary>n||!advertisementValid(p,boundary)||!advertisementValid(p+boundary,n-boundary))return r;
 r.valid=true;unsigned priority=0;auto hint=[&](const char* s,unsigned rank){if(rank>priority){r.protocol=s;priority=rank;}};
 for(size_t at=0;at<n;){size_t end=at<boundary?boundary:n;unsigned len=p[at++];if(!len){at=end;continue;}uint8_t t=p[at++];const uint8_t* d=p+at;size_t z=len-1;at+=z;++r.fields;
  if(t==0x0a&&z==1){r.hasPower=true;r.power=int8_t(d[0]);}
  if(t==0x19&&z==2){r.appearance=d[0]|uint16_t(d[1])<<8;if((r.appearance>>6)==15)hint("HID input device appearance",1);}
  if(t==0xff&&z>=2){r.company=d[0]|uint16_t(d[1])<<8;if(r.company==0x004c&&z>=4){switch(d[2]){case 2:if(z>=25&&d[3]==0x15)hint("Apple iBeacon",4);break;case 0x12:if(z>=29&&d[3]==0x19)hint("Apple Find My",4);break;case 0x10:hint("Apple Nearby",3);break;case 0x0c:hint("Apple Handoff",3);break;case 7:hint("Apple proximity pairing",3);break;}}if(r.company==6&&z>=3&&d[2]==3)hint("Microsoft Swift Pair",4);}
  size_t width=(t==2||t==3||t==0x16)?2:(t==4||t==5||t==0x20)?4:(t==6||t==7||t==0x21)?16:0;bool data=t==0x16||t==0x20||t==0x21;
  if(width&&((data&&z>=width)||(!data&&z%width==0)))for(size_t j=0;j+width<=z;j+=width){uint16_t u;if(assignedUuid16(d+j,width,u)){const char* label=uuidName(u);if(label){r.service=label;hint(label,2);}if(data&&z>width){const uint8_t* b=d+width;size_t size=z-width;
    if(u==0xfeaa){if((b[0]==0x40||b[0]==0x41)&&(size==21||size==22||size==33||size==34))hint("Google Find Hub frame",5);else if(b[0]==0&&size==20)hint("Eddystone UID",5);else if(b[0]==0x10&&size>=4)hint("Eddystone URL",5);else if(b[0]==0x20&&size==14)hint("Eddystone telemetry",5);else if(b[0]==0x30&&size==10)hint("Eddystone EID",5);}
    if(u==0xfcd2)hint("BTHome sensor data",4);if(u==0xfd6f)hint("Exposure Notification data",4);if(u==0xfe95)hint("Xiaomi MiBeacon data",4);if(u==0xfe2c)hint("Google Fast Pair data",4);
   }}if(data)break;}
 }return r;
}
// Decode report layout, never assume the keyboard boot layout for arbitrary HID.
struct HidLayout {struct Report {uint8_t id=0;uint32_t bits[3]{};};Report reports[16]{};unsigned count=0,collections=0,items=0;bool valid=false,unsupported=false;const char* error="empty map";};
inline HidLayout parseHidMap(const uint8_t* p,size_t n){
 HidLayout r;struct Global {uint32_t size=0,count=0;uint8_t id=0;} g,stack[4];unsigned sp=0,depth=0;bool numbered=false,unnumbered=false;
 if(!p||!n||n>512)return r;
 for(size_t at=0;at<n;){uint8_t h=p[at++];++r.items;if(h==0xfe){if(n-at<2){r.error="truncated long item";return r;}size_t z=p[at];at+=2;if(z>n-at){r.error="truncated long data";return r;}at+=z;r.unsupported=true;continue;}
  size_t z=(h&3)==3?4:h&3;if(z>n-at){r.error="truncated item";return r;}uint32_t v=0;for(size_t j=0;j<z;++j)v|=uint32_t(p[at+j])<<(8*j);at+=z;unsigned type=(h>>2)&3,tag=h>>4;
  if(type==1){if(tag==7)g.size=v;else if(tag==9)g.count=v;else if(tag==8){if(!v||v>255){r.error="invalid report ID";return r;}g.id=v;numbered=true;}else if(tag==10){if(sp==4){r.error="global stack overflow";return r;}stack[sp++]=g;}else if(tag==11){if(!sp){r.error="global stack underflow";return r;}g=stack[--sp];}}
  if(type==0){if(tag==10){if(++depth>16){r.error="collection depth limit";return r;}++r.collections;}else if(tag==12){if(!depth){r.error="collection underflow";return r;}--depth;}
   else if(tag==8||tag==9||tag==11){if(!g.size||!g.count||g.size>65536||g.count>65536||g.size>65536/g.count){r.error="report size limit";return r;}unsigned i=0;for(;i<r.count&&r.reports[i].id!=g.id;++i){}if(i==r.count){if(r.count==16){r.error="report count limit";return r;}r.reports[r.count++].id=g.id;}unsigned direction=tag==8?0:tag==9?1:2;uint32_t bits=g.size*g.count;if(bits>65536-r.reports[i].bits[direction]){r.error="report size overflow";return r;}r.reports[i].bits[direction]+=bits;if(!g.id)unnumbered=true;}
  }
 }
 if(depth||sp){r.error="unclosed collection/global stack";return r;}if(numbered&&unnumbered){r.error="mixed numbered and unnumbered reports";return r;}r.valid=r.count>0;r.error=r.valid?(r.unsupported?"layout contains unsupported long items":"ok"):"no reports";return r;
}
inline bool decodeHex(const char* text,uint8_t* out,size_t capacity,size_t& count){count=0;int high=-1;for(;*text;++text){unsigned char c=*text;if(c==' '||c==':'||c=='-'){if(high>=0)return false;continue;}int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(digit<0)return false;if(high<0)high=digit;else{if(count>=capacity)return false;out[count++]=uint8_t(high*16+digit);high=-1;}}return high<0&&count>0;}
}
