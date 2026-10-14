#include "app.h"
void runSelfTests(){
  int passed=0,failed=0;auto check=[&](bool ok,const char* name){if(ok)++passed;else{++failed;Serial.printf("TEST FAILED: %s\n",name);}};
  using namespace dragon;
  check(!parseWifi(nullptr,0).valid,"empty Wi-Fi rejected");
  uint8_t deauth[26]{};deauth[0]=0xc0;deauth[24]=7;
  check(parseWifi(deauth,26).deauth && parseWifi(deauth,26).reason==7,"deauth parsed");
  check(!parseWifi(deauth,25).valid,"truncated deauth rejected");
  uint8_t beacon[39]{};beacon[0]=0x80;beacon[36]=0;beacon[37]=2;beacon[38]='A';
  check(!parseWifi(beacon,sizeof(beacon)).valid,"truncated SSID rejected");
  check(named("HaleHound",Family::Wifi).confidence==Confidence::Low,"tool name remains weak");
  check(named("home-network",Family::Wifi).family==Family::Wifi,"ordinary SSID remains generic");
  uint8_t meta[]={3,0xff,0x53,0x0d};check(parseBle(meta,4).match.family==Family::Meta,"Luxottica parsed");
  uint8_t broken[]={20,0xff,0x53,0x0d};check(!parseBle(broken,4).valid,"invalid AD rejected");
  uint8_t fake[]={5,9,0xff,0xc8,9,'X'};check(parseBle(fake,6).match.family==Family::Ble,"manufacturer bytes inside name ignored");
  uint8_t generic[]={3,0xff,0xc8,9};check(parseBle(generic,4).match.confidence==Confidence::Low,"generic Xuntong only weak clue");
  uint8_t flock[24]={3,0xff,0xc8,9,19,9};memcpy(flock+6,"Penguin-1234567890",18);
  check(parseBle(flock,24).match.family==Family::Flock,"Marauder Flock composite");
  uint8_t fast[]={3,3,0x2c,0xfe};check(parseBle(fast,4).fastPair,"Fast Pair UUID");
  Burst b;bool alarm=false;for(int i=0;i<19;++i)alarm|=b.add(100+i,20);check(!alarm,"under threshold quiet");check(b.add(200,20),"first-window alert");check(!b.add(201,20),"cooldown");
  Burst wrap;wrap.add(0xfffffff0,2);check(wrap.add(20,2),"millis rollover");
  uint32_t rng=0xabc123;uint8_t fuzz[300];volatile unsigned sink=0;for(int i=0;i<12000;++i){for(auto& v:fuzz){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;v=rng;}size_t n=rng%sizeof(fuzz);auto w=parseWifi(fuzz,n);auto b=parseBle(fuzz,n);sink=sink+unsigned(w.valid)+unsigned(b.valid);}check(sink>0,"12000 bounded random parser inputs");
  Serial.printf("SELFTEST passed=%d failed=%d\n",passed,failed);
}
