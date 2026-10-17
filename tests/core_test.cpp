#ifdef NDEBUG
#error Native test assertions must be enabled
#endif
#include "detection.h"
#include "tracking.h"
#include "monitor.h"
#include <cassert>
#include <vector>
#include <cstdio>
#include <string>
using namespace dragon;
static std::vector<uint8_t> name(const char* s){std::vector<uint8_t> p{uint8_t(strlen(s)+1),9};p.insert(p.end(),s,s+strlen(s));return p;}
static void append(std::vector<uint8_t>& a,const std::vector<uint8_t>& b){a.insert(a.end(),b.begin(),b.end());}
static Match ble(const std::vector<uint8_t>& p){auto f=parseBle(p.data(),p.size());assert(f.valid);return f.match;}
#include "parity_test.h"
#include "tones_test.h"
#include "coverage_test.h"
#include "traffic_test.h"
#include "service_encoding_test.h"
#include "radar_layout_test.h"
#include "wearable_test.h"
#include "axon_test.h"
#include "ble_profile_test.h"
#include "recon_test.h"
#include "recon_analysis_test.h"
#include "network_test.h"
#include "notebook_test.h"
#include "hid_decoder_test.h"
#include "mission_test.h"
#include "theme_button_test.h"
#include "lite_controls_test.h"

#include "dino_runner_test.h"
int main(){
  dinoRunnerTests();
  liteControlsTests();
  themeButtonTests();
  missionTests();
  hidDecoderTests();
  notebookTests();
  networkTests();
  reconAnalysisTests();
  reconTests();
  bleProfileTests();
  axonTests();
  wearableTests();
  radarLayoutTests();
  serviceEncodingTests();
  trafficTests();
  parityTests();toneTests();coverageTests();
  assert(!parseBle(nullptr,0).valid && !parseWifi(nullptr,0).valid);
  assert(ble({3,0xff,0x4d,3}).family==Family::Axon);
  assert(ble({3,3,0x81,0xfc}).confidence==Confidence::Medium);
  assert(ble({3,0xff,0x4d,3,3,3,0x81,0xfc}).confidence==Confidence::Strong);
  assert(ble({3,3,0x6b,0xfe}).family==Family::Axon);
  assert(ble({3,3,0x6c,0xfe}).family==Family::Axon);
  assert(ble({3,0xff,0x75,0}).confidence==Confidence::None); // Samsung phone != SmartTag
  assert(ble({3,3,0x5a,0xfd}).family==Family::SmartTag);
  assert(ble({3,3,0x5f,0xfd}).confidence==Confidence::Low); // Meta app on a phone: weak candidate only
  assert(ble({3,0xff,0x53,0x0d}).confidence==Confidence::Low);
  assert(ble({3,0xff,0x53,0x0d,3,3,0x5f,0xfd}).confidence==Confidence::Medium);
  assert(ble({3,0xff,0xc8,9}).confidence==Confidence::Low); // Generic module, explicitly unverified
  auto flock=name("Penguin-1234567890");append(flock,{3,0xff,0xc8,9});assert(ble(flock).family==Family::Flock);
  auto brokenName=name("Penguin-123x567890");append(brokenName,{3,0xff,0xc8,9});assert(ble(brokenName).confidence==Confidence::Low);
  const std::vector<uint8_t> biscuit={17,7,0x4b,0x91,0x31,0xc3,0xc9,0xc5,0xcc,0x8f,0x9e,0x45,0xb5,0x1f,0x01,0xc2,0xaf,0x4f};
  assert(ble(biscuit).confidence==Confidence::None); // Shared Arduino example
  auto b=name("Biscuit Ultra");assert(ble(b).confidence==Confidence::Low);append(b,biscuit);assert(ble(b).family==Family::Biscuit && ble(b).confidence==Confidence::Medium);
  auto unrelated=name("My Biscuit Recipe");append(unrelated,biscuit);assert(ble(unrelated).confidence==Confidence::None);
  assert(ble({3,3,0x80,0x30}).confidence==Confidence::Low);
  assert(ble({3,3,0x80,0x30,3,0x19,0,0x86}).confidence==Confidence::Medium);
  std::vector<uint8_t> apple(31);apple[0]=30;apple[1]=0xff;apple[2]=0x4c;apple[4]=0x12;apple[5]=0x19;
  assert(ble(apple).family==Family::Tracker);apple.resize(29);apple[0]=28;assert(ble(apple).confidence==Confidence::None);
  assert(ble({3,3,0x44,0xfd}).family==Family::Tracker);
  for(auto p:std::vector<std::vector<uint8_t>>{{2,3,0x5a},{2,0x16,0x5a},{2,7,0x44},{3,0x21,0,0},{10,9,'a'}})assert(!parseBle(p.data(),p.size()).valid);
  const uint8_t publicAxon[]={0,0x25,0xdf,1,2,3};Match empty{Family::Ble,Confidence::None,"none"};
  assert(addressHint(publicAxon,false,empty).confidence==Confidence::None);
  assert(addressHint(publicAxon,true,empty).family==Family::Axon);
  assert(named("Flock Noir",Family::Wifi).confidence==Confidence::None);
  assert(named("Nyanbox",Family::Wifi).confidence==Confidence::Low);
  // Probe SSIDs describe a requested network, not the identity of the sender.
  std::vector<uint8_t> probe(24);probe[0]=0x40;probe.insert(probe.end(),{0,6,'B','i','s','c','u','i'});
  assert(parseWifi(probe.data(),probe.size()).match.confidence==Confidence::None);
  std::vector<uint8_t> fingerprint(24);fingerprint[0]=0x40;fingerprint[10]=0xb4;fingerprint[11]=0x1e;fingerprint[12]=0x52;
  fingerprint.insert(fingerprint.end(),{0,0,2,0,12,0,127,0,221,7,0x50,0x6f,0x9a,0x16,3,1,3,45,0,191,0,221,7,0,0x50,0xf2,8,0,0,0});
  assert(parseWifi(fingerprint.data(),fingerprint.size()).match.confidence==Confidence::Medium);
  fingerprint.back()=1;assert(strstr(parseWifi(fingerprint.data(),fingerprint.size()).match.evidence,"OUI"));
  uint8_t deauth[26]{};deauth[0]=0xc0;deauth[24]=7;assert(parseWifi(deauth,26).reason==7);assert(!parseWifi(deauth,25).valid);
  Burst burst;for(int n=0;n<19;++n)assert(!burst.add(n,20));assert(burst.add(20,20));assert(!burst.add(21,20));
  Burst wrap;wrap.add(0xfffffff0,2);assert(wrap.add(20,2));
  SignalTrack t;for(int n=0;n<10;++n)t.add(-70,n*100);t.add(-30,1000);assert(t.value()==-70);
  for(int n=11;n<70;++n)t.add(-40,n*100);assert(t.value()>-45 && t.trend==1);
  t.add(-90,30000);assert(t.value()==-90 && t.count==1 && t.trend==0); // Reacquisition
  t.add(0,30100);assert(t.value()==-90); // Invalid RSSI never becomes near
  SignalTrack rollover;rollover.add(-50,0xfffffff0);rollover.add(-50,140);assert(rollover.count==2 && rollover.value()==-50);
  const uint8_t* check=(const uint8_t*)"123456789";
  assert((crc32(check,9)^0xffffffffu)==0xcbf43926u);
  assert((crc32(check+4,5,crc32(check,4))^0xffffffffu)==0xcbf43926u);
  uint32_t rng=0x12345678;uint8_t fuzz[1100];volatile unsigned sink=0;
  for(unsigned i=0;i<150000;++i){for(auto& c:fuzz){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;c=rng;}size_t n=rng%sizeof(fuzz);auto w=parseWifi(fuzz,n);auto v=parseBle(fuzz,n);sink=sink+unsigned(w.valid)+unsigned(v.valid);}
  puts("PASS: detector positive/negative fixtures, truncation, CRC, tracking spikes/reacquisition/rollover, 150000 fuzz inputs");
}
