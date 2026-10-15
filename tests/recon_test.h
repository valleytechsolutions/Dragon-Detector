#include "recon_modes.h"
#include "packet_helpers.h"
#include "attack_frames.h"
static void reconTests(){
 using namespace dragon;using namespace dragon::tx;
 assert(!validMode(8)&&validMode(7));assert(modeAccepts(ReconMode::Axon,Family::Axon,true));assert(!modeAccepts(ReconMode::Axon,Family::Flipper,true));assert(modeAccepts(ReconMode::Alpr,Family::Raven,true));assert(!modeAccepts(ReconMode::Wifi,Family::Wifi,true));
 uint8_t own[]={2,1,2,3,4,5},ap[]={2,6,7,8,9,10},client[]={2,11,12,13,14,15};AttackTarget t;memcpy(t.bssid,ap,6);memcpy(t.client,client,6);t.hasAp=t.hasClient=true;t.channel=6;t.destinationChannel=11;
 uint8_t entropy[96]{},out[1200],base[1200]{};size_t beacon=makeBeacon(base,sizeof(base),ap,6,"Dragon-Test");assert(beaconValid(base,beacon));assert(!beaconValid(base,beacon-1));assert(beaconChannel(base,beacon,5)==6);assert(beaconChannel(base,beacon-1,5)==5);base[beacon-3]=61;assert(beaconChannel(base,beacon,5)==6);base[beacon-3]=3;
 for(unsigned i=0;i<unsigned(AttackMode::Count);++i){auto m=AttackMode(i);assert(validAttackTarget(m,t));memset(out,0xa5,sizeof(out));size_t n=attackDefinitions[i].ble?buildBleAttack(out,sizeof(out),m,entropy):buildWifiAttack(out,sizeof(out),m,t,own,4095,entropy);assert(n>0&&n<=256);if(attackDefinitions[i].ble)assert(validLegacyAdvert(out,n));else assert((out[22]>>4)==15&&out[23]==255);
  uint8_t before[1200];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));size_t z=attackDefinitions[i].ble?buildBleAttack(out,n-1,m,entropy):buildWifiAttack(out,n-1,m,t,own,4095,entropy);assert(z==0&&!memcmp(before,out,sizeof(out)));
 }
 AttackTarget invalid=t;invalid.bssid[0]=255;assert(!validAttackTarget(AttackMode::Deauthentication,invalid));invalid=t;invalid.hasClient=false;assert(!validAttackTarget(AttackMode::BadMessage,invalid));invalid=t;invalid.channel=12;assert(!validAttackTarget(AttackMode::Beacon,invalid));
 uint8_t parsed[6];memset(parsed,0xa5,6);assert(!parseMac("zz:11:22:33:44:55",parsed));assert(parsed[0]==0xa5);assert(!parseMac("ff:11:22:33:44:55",parsed));assert(parseMac("020102030405",parsed)&&!memcmp(parsed,own,6));
 size_t cloned=cloneBeacon(out,sizeof(out),base,beacon,13,37,11);assert(cloned==beacon+5&&!memcmp(out+36,base+36,beacon-36));assert(out[cloned-5]==37&&out[cloned-2]==11);assert(!cloneBeacon(out,cloned-1,base,beacon,0,37,11));
 uint8_t frame[48]{};frame[0]=8;frame[1]=1;memcpy(frame+4,ap,6);memcpy(frame+10,client,6);uint8_t a[6],c[6];assert(stationPair(frame,sizeof(frame),a,c)&&!memcmp(a,ap,6)&&!memcmp(c,client,6));frame[1]=3;assert(!stationPair(frame,sizeof(frame),a,c));frame[1]=2;memcpy(frame+4,client,6);memcpy(frame+10,ap,6);assert(stationPair(frame,sizeof(frame),a,c));frame[4]=255;assert(!stationPair(frame,sizeof(frame),a,c));assert(!stationPair(frame,23,a,c));
 pcapHeader(out);assert(out[20]==127&&out[4]==2&&out[6]==4);size_t record=pcapRecord(out,sizeof(out),base,beacon,beacon+10,1234567,-61,6);assert(record==beacon+29&&out[0]==1&&out[18]==13&&out[20]==0x28&&int8_t(out[28])==-61&&!memcmp(out+29,base,beacon));assert(!pcapRecord(out,record-1,base,beacon,beacon,0,-61,6));assert(!pcapRecord(out,sizeof(out),base,beacon,beacon-1,0,-61,6));
 uint32_t rng=123;for(unsigned i=0;i<10000;++i){for(auto& x:entropy){rng=rng*1664525+1013904223;x=rng>>24;}for(unsigned m=14;m<=20;++m){size_t n=buildBleAttack(out,31,AttackMode(m),entropy);assert(n&&validLegacyAdvert(out,n));}size_t n=rng%300;memcpy(base,entropy,96);beaconValid(base,n);cloneBeacon(out,sizeof(out),base,n,0,37,6);}
 puts("PASS recon modes, all 24 packet builders, bounds, AP/client identity, Radiotap and 70000 advertisement cases");
}
