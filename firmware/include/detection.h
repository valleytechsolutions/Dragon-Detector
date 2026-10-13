#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

namespace dragon {
enum class Family : uint8_t { Wifi, Ble, Flock, Axon, Meta, Pineapple, Pwnagotchi, Nyanbox, Halehound, Tracker, Skimmer, Flipper, Biscuit, SmartTag };
enum class Confidence : uint8_t { None, Low, Medium, Strong };
struct Match { Family family; Confidence confidence; const char* evidence; };
inline const char* familyName(Family f) {
  static const char* n[] = {"Wi-Fi", "BLE", "Flock candidate", "Axon candidate", "Meta candidate", "Pineapple candidate", "Pwnagotchi signature", "Nyanbox name", "HaleHound name", "Tracker candidate", "Skimmer name", "Flipper candidate", "Biscuit candidate", "SmartTag candidate"};
  return n[static_cast<unsigned>(f)];
}
inline const char* confidenceName(Confidence c) { return c==Confidence::Strong ? "strong signature" : c==Confidence::Medium ? "medium confidence" : c==Confidence::Low ? "weak indicator" : "unclassified"; }
inline bool contains(const char* haystack, const char* needle) {
  for (; *haystack; ++haystack) { size_t i=0; while (needle[i] && haystack[i] && tolower((unsigned char)haystack[i])==tolower((unsigned char)needle[i])) ++i; if (!needle[i]) return true; } return false;
}
inline bool bytesContain(const uint8_t* p, size_t n, const char* s) { size_t l=strlen(s); if(l>n) return false; for(size_t i=0;i+l<=n;++i) if(!memcmp(p+i,s,l)) return true; return false; }
inline void safeText(char* out, size_t cap, const uint8_t* in, size_t n) { if(!cap)return; size_t i=0; for(;i<n && i<cap-1;++i)out[i]=(in[i]>=32 && in[i]<127)?in[i]:'?';out[i]=0; }
inline bool equalName(const char* a,const char* b){return strlen(a)==strlen(b) && contains(a,b);}
inline bool namedPrefix(const char* s,const char* prefix){size_t n=strlen(prefix);if(strlen(s)<n)return false;for(size_t i=0;i<n;++i)if(tolower((unsigned char)s[i])!=tolower((unsigned char)prefix[i]))return false;return s[n]==0 || s[n]==' ' || s[n]=='-' || s[n]=='_';}
// Reference facts: Flock Noir 0559417 / Colonel Panic Flock-You research.
// Shared chip-vendor OUIs are deliberately not enough for a default-radar match.
inline bool flockPrefix(const uint8_t* m){
 const uint32_t list[]={0x70c94e,0x3c9180,0xd8f3bc,0x803049,0xb83532,0x145afc,0x744ca1,0x083a88,0x9c2f9d,0xc03532,0x940853,0xe4aaea,0xf46add,0xe00af6,0x24b2b9,0x00f48d,0xd03957,0xe8d0fc,0xe04f43,0xb81ea4,0x700894,0x588e81,0xec1bbd,0x3c71bf,0x5800e3,0x9035ea,0x5c93a2,0x646e69,0x4827ea,0xa4cf12,0x14b5cd,0x826bf2,0xb41e52,0x00037f};
 if(m[0]&1)return false;uint32_t p=(uint32_t(m[0])<<16)|(uint32_t(m[1])<<8)|m[2];
 for(auto v:list)if(v==p)return true;return false;
}
inline Match addressHint(const uint8_t* mac,bool publicAddress,Match current){
 if(!publicAddress || current.confidence!=Confidence::None)return current;
 if(mac[0]==0 && mac[1]==0x25 && mac[2]==0xdf)return {Family::Axon,Confidence::Low,"public Axon OUI; model unknown"};
 if(flockPrefix(mac))return {Family::Flock,Confidence::Low,"Flock research OUI; shared chip vendor possible"};
 return current;
}
inline Match named(const char* name, Family base) {
  if(equalName(name,"flock noir") || namedPrefix(name,"dragon detector"))return {base,Confidence::None,"detector dashboard, not a camera"};
  if(namedPrefix(name,"biscuit"))return {Family::Biscuit,Confidence::Low,"Biscuit name; Pro/Ultra model unverified"};
  if(namedPrefix(name,"flipper"))return {Family::Flipper,Confidence::Low,"Flipper name only; spoofable"};
  if(contains(name,"smarttag"))return {Family::SmartTag,Confidence::Low,"SmartTag advertised name only"};
  if(contains(name,"airtag"))return {Family::Tracker,Confidence::Low,"AirTag advertised name only"};
  if(contains(name,"pwnagotchi"))return {Family::Pwnagotchi,Confidence::Low,"advertised name; spoofable"};
  if(contains(name,"pineapple") || contains(name,"hak5"))return {Family::Pineapple,Confidence::Low,"advertised name; model unverified"};
  if(contains(name,"nyanbox"))return {Family::Nyanbox,Confidence::Low,"advertised name only"};
  if(contains(name,"halehound"))return {Family::Halehound,Confidence::Low,"advertised name only"};
  if(contains(name,"ray-ban") || contains(name,"rayban") || contains(name,"oakley meta"))return {Family::Meta,Confidence::Low,"advertised glasses name"};
  if(contains(name,"axon") || !strncmp(name,"AB2-",4) || !strncmp(name,"AB3-",4) || !strncmp(name,"AB4-",4))return {Family::Axon,Confidence::Low,"camera-like advertised name"};
  if(contains(name,"flock"))return {Family::Flock,Confidence::Low,"advertised Flock name"};
  if(!strcmp(name,"HC-05") || !strcmp(name,"HC-06") || !strcmp(name,"HC-08"))return {Family::Skimmer,Confidence::Low,"generic serial module name, often benign"};
  return {base,Confidence::None,"no specific signature"};
}
struct WifiFrame {
  bool valid=false, beacon=false, probe=false, deauth=false, disassoc=false, eapol=false, protectedFrame=false, retry=false;
  uint8_t source[6]{}, bssid[6]{}, channel=0; uint16_t sequence=0, reason=0;
  char name[33]{}; Match match{Family::Wifi,Confidence::None,"802.11 frame"};
};
inline WifiFrame parseWifi(const uint8_t* p,size_t n) {
  WifiFrame f; if(!p || n<24 || (p[0]&3))return f;
  uint8_t type=(p[0]>>2)&3, sub=p[0]>>4;
  if(type!=0 && type!=2)return f;
  f.valid=true; memcpy(f.source,p+10,6); memcpy(f.bssid,p+16,6);
  f.sequence=(uint16_t(p[23])<<8)|p[22]; f.retry=p[1]&8; f.protectedFrame=p[1]&0x40;
  f.beacon=type==0 && (sub==8 || sub==5); f.probe=type==0 && sub==4;
  f.deauth=type==0 && sub==12; f.disassoc=type==0 && sub==10;
  if((f.deauth || f.disassoc) && !f.protectedFrame) { if(n<26){f.valid=false;return f;} f.reason=p[24]|(uint16_t(p[25])<<8); }
  if(f.beacon || f.probe) {
    size_t off=f.beacon?36:24; if(n<off){f.valid=false;return f;}
    while(off+2<=n) { uint8_t id=p[off],len=p[off+1];off+=2; if(len>n-off){f.valid=false;return f;}
      if(id==0 && len<=32)safeText(f.name,sizeof(f.name),p+off,len);
      if(id==3 && len==1)f.channel=p[off]; off+=len;
    }
    if(off!=n){f.valid=false;return f;}
    f.match=f.beacon?named(f.name,Family::Wifi):Match{Family::Wifi,Confidence::None,"probe request does not identify the advertised AP"};
    // Exact wildcard IE order and vendor bytes, together with transmitter OUI.
    if(f.probe && flockPrefix(f.source)){
      const uint8_t ids[]={0,2,12,127,221,45,191,221};
      const uint8_t v1[]={0x50,0x6f,0x9a,0x16,3,1,3},v2[]={0,0x50,0xf2,8,0,0,0};
      size_t at=24,k=0;bool exact=true;
      while(at+2<=n){uint8_t id=p[at++],z=p[at++];
        if(k>=sizeof(ids)||id!=ids[k])exact=false;
        if(k==0 && z!=0)exact=false;
        if(k==4 && (z!=sizeof(v1)||memcmp(p+at,v1,sizeof(v1))))exact=false;
        if(k==7 && (z!=sizeof(v2)||memcmp(p+at,v2,sizeof(v2))))exact=false;
        at+=z;++k;
      }
      if(exact && k==sizeof(ids))f.match={Family::Flock,Confidence::Medium,"Flock research OUI + wildcard probe IE fingerprint; spoofable"};
    }
    // Adapted from Marauder beaconSnifferCallback/processPwnagotchiBeacon.
    const uint8_t pwn[]={0xde,0xad,0xbe,0xef,0xde,0xad};
    if(f.beacon && !memcmp(f.source,pwn,6) && bytesContain(p+36,n-36,"\"pwnd_tot\"") && bytesContain(p+36,n-36,"\"name\""))
      f.match={Family::Pwnagotchi,Confidence::Strong,"Marauder peer MAC + Pwnagotchi fields; spoofable"};
    else if(f.match.confidence==Confidence::None && f.source[0]==0 && f.source[1]==0x13 && f.source[2]==0x37)
      f.match={Family::Pineapple,Confidence::Low,"Marauder OUI heuristic; Pager model unverified"};
  }
  if(type==2 && !f.protectedFrame) {
    size_t h=24; if((p[1]&3)==3)h+=6; if(sub&8)h+=2; if((sub&8)&&(p[1]&0x80))h+=4;
    static const uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};
    f.eapol=n>=h+8 && !memcmp(p+h,llc,8);
  }
  f.match=addressHint(f.source,true,f.match);
  return f;
}
struct BleFrame { bool valid=false, applePopup=false, fastPair=false, swiftPair=false; uint16_t company=0xffff; char name[64]{}; Match match{Family::Ble,Confidence::None,"BLE advertisement"}; };
inline BleFrame parseBle(const uint8_t* p,size_t n) {
  BleFrame f; if(!p || !n)return f; bool xuntong=false,lux=false,meta=false,findmy=false,flipper=false,metaService=false,dult=false,axonCompany=false,axonService=false,smarttag=false,biscuit=false,flockService=false; uint16_t appearance=0;
  for(size_t o=0;o<n;) {
    uint8_t l=p[o++]; if(!l)break; if(l>n-o)return f; uint8_t t=p[o]; const uint8_t* d=p+o+1; size_t z=l-1;
    if((t==2||t==3) && z%2)return f;
    if((t==6||t==7) && z%16)return f;
    if(t==0x16 && z<2)return f;
    if(t==0x21 && z<16)return f;
    if(t==0x19 && z==2)appearance=d[0]|uint16_t(d[1])<<8;
    if((t==8 || t==9) && z)safeText(f.name,sizeof(f.name),d,z);
    if(t==0xff && z>=2) {
      uint16_t id=d[0]|uint16_t(d[1])<<8; f.company=id;axonCompany|=id==0x034d;
      xuntong|=id==0x09c8; lux|=id==0x0d53; meta|=id==0x01ab || id==0x058e;
      if(id==0x004c && z>=4){ f.applePopup|=d[2]==0x07 || d[2]==0x0f; findmy|=d[2]==0x12 && d[3]==0x19 && z>=29; }
      f.swiftPair|=id==0x0006 && z>=3 && d[2]==3;
    }
    if((t==2 || t==3 || t==0x16) && z>=2) { size_t limit=t==0x16?2:z; for(size_t j=0;j+1<limit;j+=2) { uint16_t uuid=d[j]|uint16_t(d[j+1])<<8; f.fastPair|=uuid==0xfe2c;findmy|=uuid==0xfd44;flipper|=uuid>=0x3080&&uuid<=0x3083;axonService|=uuid==0xfc81||uuid==0xfe6b||uuid==0xfe6c;smarttag|=uuid==0xfd5a;metaService|=uuid==0xfd5f||uuid==0xfeb7||uuid==0xfeb8; } }
    if((t==6||t==7||t==0x21) && z>=16){
      const uint8_t biscuitId[]={0x4b,0x91,0x31,0xc3,0xc9,0xc5,0xcc,0x8f,0x9e,0x45,0xb5,0x1f,0x01,0xc2,0xaf,0x4f};
      const uint8_t flockId[]={0x6f,0x2e,0x17,0xdf,0x14,0x18,0xe5,0x9f,0xa8,0x46,0x32,0x95,0x38,0xbb,0xcc,0xe8};
      for(size_t j=0;j+16<=z;j+=16){biscuit|=!memcmp(d+j,biscuitId,16);flockService|=!memcmp(d+j,flockId,16);if(t==0x21)break;}
      // Marauder DULT UUID, little-endian BLE AD representation.
      const uint8_t uuid[]={0x85,0x2a,0x9f,0x57,0xc5,0x2a,0xed,0x88,0x26,0xc2,0xf4,0x12,0x01,0x00,0x19,0x15};
      for(size_t j=0;j+16<=z;j+=16){dult|=!memcmp(d+j,uuid,16);if(t==0x21)break;}
    }
    o+=l;
  }
  f.valid=true; f.match=named(f.name,Family::Ble);
  // Marauder isFlockCamera name + manufacturer rule, tightened to avoid unnamed generic modules.
  bool penguin=!strncmp(f.name,"Penguin-",8) && strlen(f.name)==18;
  if(penguin)for(int i=8;i<18;++i)penguin &= isdigit((unsigned char)f.name[i])!=0;
  bool numeric=strlen(f.name)==10; if(numeric)for(int i=0;i<10;++i)numeric &= isdigit((unsigned char)f.name[i])!=0;
  if(axonCompany || axonService)f.match={Family::Axon,axonCompany&&axonService?Confidence::Strong:Confidence::Medium,"Axon/TASER company or member service ID; model/recording unknown"};
  else if(flockService)f.match={Family::Flock,Confidence::Strong,"Flock accessory 128-bit service; camera presence unverified"};
  else if(xuntong && (penguin || numeric || equalName(f.name,"FS Ext Battery")))f.match={Family::Flock,Confidence::Medium,"Xuntong company + Flock battery name"};
  else if(lux && metaService)f.match={Family::Meta,Confidence::Medium,"Luxottica + Meta service composite; recording unknown"};
  else if(biscuit && namedPrefix(f.name,"biscuit"))f.match={Family::Biscuit,Confidence::Medium,"Biscuit name + service; Pro/Ultra model unverified"};
  else if(smarttag)f.match={Family::SmartTag,Confidence::Medium,"Samsung FD5A service; owner and model unknown"};
  else if(findmy)f.match={Family::Tracker,Confidence::Medium,"Find My protocol; may be AirTag or another accessory"};
  else if(dult)f.match={Family::Tracker,Confidence::Medium,"DULT tracker service; owner unknown"};
  else if(flipper)f.match={Family::Flipper,appearance==0x8600?Confidence::Medium:Confidence::Low,"Flipper service; appearance corroborates when present"};
  else if(lux && f.match.confidence==Confidence::None)f.match={Family::Meta,Confidence::Low,"Luxottica company only; product unverified"};
  return f;
}
// Fixed observation window; an alert is traffic evidence, never proof of intent.
struct Burst { uint32_t started=0,lastAlert=0; uint16_t count=0; bool active=false,alerted=false;
  bool add(uint32_t now,uint16_t threshold,uint32_t window=5000) {
    if(!active || uint32_t(now-started)>=window){started=now;count=0;active=true;}
    if(count<65535)++count;
    if(count>=threshold && (!alerted || uint32_t(now-lastAlert)>=30000)){lastAlert=now;alerted=true;return true;}return false;
  }
};
}
