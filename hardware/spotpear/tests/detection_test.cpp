#include "detection.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
using namespace dragon;
int main(){
  assert(!parseWifi(nullptr,0).valid);assert(!parseBle(nullptr,0).valid);
  uint8_t d[26]{};d[0]=0xc0;d[24]=7;assert(parseWifi(d,26).reason==7);assert(parseWifi(d,26).deauth);assert(!parseWifi(d,25).valid);
  d[1]=0x40;assert(parseWifi(d,24).protectedFrame);assert(parseWifi(d,24).reason==0);
  std::vector<uint8_t> b(36);b[0]=0x80;b.insert(b.end(),{0,4,'t','e','s','t',3,1,6});auto f=parseWifi(b.data(),b.size());assert(f.valid&&f.channel==6&&!strcmp(f.name,"test"));
  assert(!parseWifi(b.data(),b.size()-1).valid);
  uint8_t m[]={3,0xff,0x53,0x0d};assert(parseBle(m,sizeof(m)).match.family==Family::Meta);
  for(size_t n=1;n<sizeof(m);++n)assert(!parseBle(m,n).valid);
  uint8_t fake[]={5,9,0xff,0xc8,9,'x'};assert(parseBle(fake,sizeof(fake)).match.family==Family::Ble);
  uint8_t generic[]={3,0xff,0xc8,9};assert(parseBle(generic,sizeof(generic)).match.family==Family::Ble);
  uint8_t flock[24]={3,0xff,0xc8,9,19,9};memcpy(flock+6,"Penguin-1234567890",18);auto fl=parseBle(flock,24);assert(fl.match.family==Family::Flock&&fl.match.confidence==Confidence::Medium);
  flock[10]='!';assert(parseBle(flock,24).match.family==Family::Ble);
  uint8_t fast[]={3,3,0x2c,0xfe};assert(parseBle(fast,4).fastPair);
  uint8_t flipper[]={3,3,0x82,0x30};assert(parseBle(flipper,4).match.family==Family::Flipper);
  uint8_t uuidMeta[]={3,3,0x5f,0xfd};assert(parseBle(uuidMeta,4).match.family==Family::Meta);
  uint8_t fmna[]={3,3,0x44,0xfd};assert(parseBle(fmna,4).match.family==Family::Tracker);
  uint8_t dult[]={17,7,0x85,0x2a,0x9f,0x57,0xc5,0x2a,0xed,0x88,0x26,0xc2,0xf4,0x12,0x01,0x00,0x19,0x15};assert(parseBle(dult,sizeof(dult)).match.family==Family::Tracker);
  uint8_t popup[]={5,0xff,0x4c,0,7,0};assert(parseBle(popup,6).applePopup);
  uint8_t apple[]={5,0xff,0x4c,0,0x10,0};assert(!parseBle(apple,6).applePopup);
  assert(named("HaleHound",Family::Wifi).confidence==Confidence::Low);
  assert(named("bedroom speaker",Family::Ble).confidence==Confidence::None);
  Burst burst;for(int i=0;i<19;++i)assert(!burst.add(i+1,20));assert(burst.add(20,20));assert(!burst.add(21,20));
  Burst window;assert(!window.add(1,2));assert(!window.add(6000,2));assert(window.add(6001,2));
  Burst wrap;assert(!wrap.add(0xfffffff0,2));assert(wrap.add(10,2));
  uint8_t eapol[32]{};eapol[0]=8;uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};memcpy(eapol+24,llc,8);assert(parseWifi(eapol,32).eapol);eapol[1]=0x40;assert(!parseWifi(eapol,32).eapol);
  // Pwnagotchi peer data must carry both the special address and peer fields.
  std::vector<uint8_t> pwn(36);pwn[0]=0x80;uint8_t mac[]={0xde,0xad,0xbe,0xef,0xde,0xad};memcpy(pwn.data()+10,mac,6);
  const char* peer="{\"name\":\"peer\",\"pwnd_tot\":1}";pwn.push_back(222);pwn.push_back(strlen(peer));pwn.insert(pwn.end(),peer,peer+strlen(peer));assert(parseWifi(pwn.data(),pwn.size()).match.family==Family::Pwnagotchi);
  pwn[10]=0;assert(parseWifi(pwn.data(),pwn.size()).match.family==Family::Wifi);
  uint32_t rng=0x12345678;uint8_t fuzz[1100];volatile unsigned sink=0;
  for(unsigned i=0;i<150000;++i){for(auto& c:fuzz){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;c=rng;}size_t len=rng%sizeof(fuzz);auto w=parseWifi(fuzz,len);auto v=parseBle(fuzz,len);sink=sink+unsigned(w.valid)+unsigned(v.valid);}
  puts("PASS: packet validation, confidence, composite signatures, deauth windows, rollover, EAPOL, 150000 fuzz inputs");
}
