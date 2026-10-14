#pragma once
#include "coverage_facts.h"
#include "contacts.h"
static void coverageTests(){
  DetectorSettings settings;settings.minimum=Confidence::Strong;
  for(const auto& fact:coverageFacts){
    Match match{Family::Ble,Confidence::None,"none"};
    if(!strcmp(fact.kind,"address")){
      unsigned prefix=0;sscanf(fact.value,"%x",&prefix);uint8_t mac[]={uint8_t(prefix>>16),uint8_t(prefix>>8),uint8_t(prefix),1,2,3};
      auto packet=beacon(1,false);memcpy(packet.data()+10,mac,6);memcpy(packet.data()+16,mac,6);
      auto f=parseWifi(packet.data(),packet.size());assert(f.valid);match=f.match;
      assert(addressHint(mac,false,{Family::Ble,Confidence::None,"none"}).confidence==Confidence::None);
    }else if(!strcmp(fact.kind,"uuid")||!strcmp(fact.kind,"company")){
      unsigned value=0;sscanf(fact.value,"%x",&value);
      std::vector<uint8_t> data{uint8_t(value),uint8_t(value>>8)};
      if(!strcmp(fact.kind,"company")&&value==0x004c){data.resize(29);data[2]=0x12;data[3]=0x19;}
      match=ble(ad(!strcmp(fact.kind,"uuid")?3:0xff,data));
    }else if(!strcmp(fact.kind,"name"))match=ble(name(fact.value));
    else {
      std::vector<uint8_t> packet(36);packet[0]=0x80;
      std::string ssid=std::string(fact.value)+"unit123";packet.push_back(0);packet.push_back(ssid.size());packet.insert(packet.end(),ssid.begin(),ssid.end());
      auto f=parseWifi(packet.data(),packet.size());assert(f.valid);match=f.match;
    }
    if(match.confidence==Confidence::None || match.family!=fact.family){fprintf(stderr,"COVERAGE FAIL %s %s -> %s\n",fact.kind,fact.value,familyName(match.family));abort();}
    Contact c;c.used=true;c.packets=1;c.seen=c.matchedAt=c.matchSince=100;c.matchHits=1;c.match=match;
    assert(candidateVisible(c,100,settings,false,false,false)); // strong-only alerts must not hide clues
    assert(!candidateVisible(c,100,settings,false,true,false));
    assert(!candidateVisible(c,30100,settings,false,false,false));
  }
  // Non-table runtime rules: names, battery service, peer JSON, pairing form.
  for(const char* n:{"front-Axon-camera","my-Flipper-unit","a-Penguin-pack","Pigvision123","myFlockRadio","1234567890","FS Ext Battery"})assert(ble(name(n)).confidence!=Confidence::None);
  assert(named("Flock",Family::Wifi).family==Family::Flock);
  auto service=ad(7,{0x6f,0x2e,0x17,0xdf,0x14,0x18,0xe5,0x9f,0xa8,0x46,0x32,0x95,0x38,0xbb,0xcc,0xe8});assert(ble(service).family==Family::Flock);
  auto peer=beacon(4,false);const char* json="{\"name\": \"little-peer\",\"pwnd_tot\":12}";
  append(peer,{222,uint8_t(strlen(json))});peer.insert(peer.end(),json,json+strlen(json));
  auto pwn=parseWifi(peer.data(),peer.size());assert(pwn.match.family==Family::Pwnagotchi&&!strcmp(pwn.name,"little-peer"));
  peer.back()='"';assert(parseWifi(peer.data(),peer.size()).match.family==Family::Pwnagotchi); // irrelevant trailing field
  char out[20]{};const char* broken="{\"pwnd_tot\":12,\"name\":\"unterminated";assert(!peerName((const uint8_t*)broken,strlen(broken),out,sizeof(out)));
  std::vector<uint8_t> pairing(29);pairing[0]=0x4c;pairing[2]=7;pairing[3]=0x19;auto m=ble(ad(0xff,pairing));assert(m.family==Family::Tracker&&m.confidence==Confidence::Low);
  pairing.resize(28);assert(ble(ad(0xff,pairing)).confidence==Confidence::None);
  std::vector<uint8_t> response{3,0xff,0x53,0x0d,0,0,0};append(response,{3,3,0x5f,0xfd});
  auto combined=parseBle(response.data(),response.size(),7);assert(combined.valid&&combined.match.confidence==Confidence::Medium);
  assert(parseBle(response.data(),response.size()).match.confidence==Confidence::Low);
  response[0]=8;assert(!parseBle(response.data(),response.size(),7).valid);assert(!parseBle(response.data(),response.size(),100).valid);
  // A payload which embeds Apple bytes inside another field isn't Apple AD.
  assert(ble(ad(0x16,{0x12,0x18,0x4c,0,0x12,0x19})).confidence==Confidence::None);
  for(size_t i=1;i<sizeof(vendorPrefixes)/sizeof(*vendorPrefixes);++i)assert(vendorPrefixes[i-1].prefix<vendorPrefixes[i].prefix);
  // Event overlays remain visible even if the base family's alerts are off.
  Contact event;event.used=true;event.packets=1;event.seen=event.lastWifiAlert=99;event.wifiAlerts=1;event.trafficFamily=Family::BleSpam;
  settings.families=1u<<unsigned(Family::BleSpam);assert(candidateVisible(event,100,settings,false,false,false));assert(visibleFamily(event,100)==Family::BleSpam);
  settings.families=0;assert(!candidateVisible(event,100,settings,true,false,false));assert(candidateVisible(event,100,settings,false,false,true));
  assert(visibleFamily(event,100,0)==event.match.family);
  event.seen=0xfffffff0;event.lastWifiAlert=event.seen;settings.families=1u<<unsigned(Family::BleSpam);assert(candidateVisible(event,10,settings,false,false,false));
  // Cache collision, radio/type separation and reused slots cannot alias.
  std::vector<Contact> entries(MAX_CONTACTS);ContactCache cache;
  for(unsigned i=0;i<MAX_CONTACTS;++i){auto& c=entries[i];c.used=true;c.mac[4]=i>>8;c.mac[5]=i;c.ble=i%2;c.addressType=i%4;cache.remember(keyOf(c),i);}
  for(unsigned i=0;i<MAX_CONTACTS;++i){int found=cache.lookup(keyOf(entries[i]),entries.data());assert(found<0||found==int(i));}
  auto old=keyOf(entries[0]);cache.remember(old,0);entries[0].mac[0]=42;assert(cache.lookup(old,entries.data())<0);
  DisconnectMonitor disconnect;for(unsigned i=0;i<5;++i)assert(!disconnect.observe(i*400));assert(disconnect.observe(2000));assert(!disconnect.observe(2100));
  for(unsigned i=0;i<5;++i)assert(!disconnect.observe(18000+i*400));assert(disconnect.observe(20000));
  DisconnectMonitor sparse;for(unsigned i=0;i<10;++i)assert(!sparse.observe(i*1000));
  DisconnectMonitor rollover;for(unsigned i=0;i<5;++i)assert(!rollover.observe(0xfffffc00+i*400));assert(rollover.observe(976));
  Contact hunt;hunt.packets=1;hunt.seen=100;assert(!huntSweep(hunt,12099));assert(huntSweep(hunt,12100));hunt.ble=true;assert(!huntSweep(hunt,12100));
  puts("PASS: all 134 pinned SquachWatch table facts recognized and radar-visible; runtime rules, weak evidence, cache and visibility boundaries");
}
