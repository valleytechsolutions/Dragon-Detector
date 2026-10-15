#include "axon.h"
#include "contacts.h"
#include "radio_schedule.h"
static void axonTests(){
  const uint8_t random[]={0xc2,1,2,3,4,5},publicMac[]={0,0x25,0xdf,1,2,3},other[]={0x44,1,2,3,4,5};
  bool valid=false;
  auto observation=[&](const std::vector<uint8_t>& data,const uint8_t* mac,uint8_t type,size_t boundary=0){return parseBleObservation(data.data(),data.size(),boundary,mac,type,valid);};
  // All three assigned UUIDs, all AD encodings, unnamed rotating addresses.
  for(uint16_t id:{0xfc81,0xfe6b,0xfe6c})for(unsigned width:{2u,4u,16u})for(unsigned kind:{0u,1u,2u}){
    uint8_t type=kind==2?(width==2?0x16:width==4?0x20:0x21):(width==2?2:width==4?4:6)+kind;
    auto data=serviceField(id,width,type);auto f=observation(data,random,1);
    assert(valid&&f.valid&&f.match.family==Family::Axon&&f.match.confidence==Confidence::Medium);
    assert(inspectAxonBle(data.data(),data.size(),0,random,1).registered());
    append(data,{3,0xff,0x4d,3});f=observation(data,random,1);assert(f.match.confidence==Confidence::Strong);
  }
  auto f=observation({3,0xff,0x4d,3},random,1);assert(valid&&f.match.family==Family::Axon);
  assert(observation({3,0xff,3,0x4d},random,1).match.confidence==Confidence::None);
  // OUI evidence does not depend on a nonempty or well-formed payload.
  for(uint8_t type:{0,2}){f=observation({},publicMac,type);assert(!valid&&f.valid&&f.match.confidence==Confidence::Medium);}
  f=observation({},publicMac,1);assert(f.valid&&f.match.confidence==Confidence::Low);
  assert(!inspectAxonBle(nullptr,0,0,publicMac,1).registered());
  assert(!observation({},other,0).valid);
  // Complete company field survives a truncated unrelated name. Never use a
  // truncated company field, embedded text, or bytes across ADV/SCAN_RSP.
  f=observation({3,0xff,0x4d,3,9,9,'A'},random,1);assert(!valid&&f.valid&&f.match.confidence==Confidence::Medium);
  assert(!observation({4,0xff,0x4d,3},random,1).valid);
  assert(observation({5,9,3,0xff,0x4d,3},random,1).match.confidence==Confidence::None);
  assert(!observation({3,0xff,0x4d,3},random,1,3).valid);
  // Bad ADV segment cannot eat a valid, separately bounded scan response.
  f=observation({9,9,'X',3,0xff,0x4d,3},random,1,3);assert(!valid&&f.valid&&f.match.confidence==Confidence::Medium);
  f=observation({0,0,0,3,0xff,0x4d,3},random,1,3);assert(valid&&f.match.confidence==Confidence::Medium);
  auto custom=serviceField(0xfc81,16,7);custom[2]^=1;
  f=observation(custom,random,1);assert(valid&&f.match.family==Family::Axon&&f.match.confidence==Confidence::Low);
  assert(!inspectAxonBle(custom.data(),custom.size(),0,random,1).registered());
  // Partial-UUID compatibility is never extracted from arbitrary data bytes.
  assert(observation(serviceField(0x180f,2,0x16,{0x81,0xfc}),random,1).match.confidence==Confidence::None);
  // AXEOFF's Wi-Fi probes/beacons are header evidence, independent of IE parsing.
  for(uint8_t type:{0x40,0x80,0x50}){
    std::vector<uint8_t> packet(type==0x40?24:36);packet[0]=type;memcpy(packet.data()+10,publicMac,6);
    auto w=parseWifiObservation(packet.data(),packet.size(),false,valid);assert(valid&&w.match.family==Family::Axon);
    append(packet,{0,6,'X'});w=parseWifiObservation(packet.data(),packet.size(),false,valid);
    assert(!valid&&w.valid&&w.match.family==Family::Axon&&!*w.name&&!w.beacon&&!w.probe);
    w=parseWifiObservation(packet.data(),packet.size(),true,valid);assert(!valid&&w.valid);
    assert(!inspectAxonWifi(packet.data(),23).registered());
    packet[10]=0x44;assert(!parseWifiObservation(packet.data(),packet.size(),false,valid).valid);
  }
  // Normal radar immediately shows and alerts a one-hit assigned signature,
  // even if other families use strong-only. Priority OFF restores old policy.
  Match medium{Family::Axon,Confidence::Medium,"fixture"},weak{Family::Axon,Confidence::Low,"fixture"};
  assert(signatureAlertReady(medium,1,100,100,true));
  assert(alertConfidenceAllowed(medium,Confidence::Strong,true));
  assert(!signatureAlertReady(medium,1,100,100,false));
  assert(!alertConfidenceAllowed(medium,Confidence::Strong,false));
  assert(!signatureAlertReady(weak,1,100,100,true));
  assert(!alertConfidenceAllowed(weak,Confidence::Medium,true));
  assert(signatureAlertReady(weak,2,0xfffffff0,500,false));
  assert(!signatureAlertReady({Family::Tracker,Confidence::Medium,"fixture"},1,100,100,true));
  Contact contact;contact.used=true;contact.ble=true;contact.packets=contact.matchHits=1;contact.match=medium;contact.seen=contact.matchedAt=100;
  DetectorSettings settings;settings.minimum=Confidence::Strong;
  assert(candidateVisible(contact,100,settings,false,false,false));
  settings.families&=~(1u<<unsigned(Family::Axon));assert(!candidateVisible(contact,100,settings,false,false,false));
  AlertGate gate;assert(gate.allow(100,-60,5));assert(!gate.allow(101,-50,5));
  // Every Wi-Fi slice covers eleven channels at its planned deadlines, including
  // rollover; BLE-first and boundary transitions do not depend on loop cadence.
  for(uint32_t start:{0u,0xfffffff0u})for(uint8_t first=1;first<=11;++first){
    RadioSchedule s;s.begin(start);assert(s.ble&&!s.due(start+2199)&&s.due(start+2200));
    s.begin(start,false,first);unsigned channels=0;
    for(unsigned ms=0;ms<2200;ms+=200)channels|=1u<<s.channel(start+ms);
    assert(channels==0xffe&&s.due(start+2200));
  }
  // Fuzz the actual transport classifiers, not only the stricter AD decoder.
  uint32_t rng=0x34d;uint8_t data[128];
  for(unsigned i=0;i<50000;++i){for(auto& b:data){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;b=rng;}size_t size=rng%sizeof(data);size_t split=(rng>>8)%(size+2);
    parseBleObservation(data,size,split,random,1,valid);parseWifiObservation(data,size,i&1,valid);
  }
  puts("PASS: Axon transport signatures, malformed-tail recovery, weak compatibility clues, first-hit alert policy and 11-channel schedule; 50000 transport fuzz inputs");
}
