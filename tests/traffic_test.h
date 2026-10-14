#include "traffic.h"
static void trafficTests(){
  WifiFrame frame;frame.valid=true;frame.frameType=0xc0;frame.sequence=16;
  WifiRetryFilter retry;assert(!retry.duplicate(frame,0));
  auto other=frame;other.sequence=32;assert(!retry.duplicate(other,5));
  frame.retry=true;assert(retry.duplicate(frame,10)); // interleaved duplicate
  frame.destination[0]=2;assert(!retry.duplicate(frame,15));
  frame.frameType=0xa0;assert(!retry.duplicate(frame,20));
  assert(!retry.duplicate(frame,1020)); // sequence reuse is normal
  DisconnectMonitor d;for(unsigned i=0;i<5;++i)assert(!d.observe(i*400));assert(d.observe(2000));assert(d.recent(2000)==6);assert(d.recent(6000)==0);
  uint8_t protectedShort[24]{};protectedShort[0]=0xc0;protectedShort[1]=0x40;assert(!parseWifi(protectedShort,24).valid);
  BleBurstMonitor many;DeviceKey key;key.radio=1;
  for(unsigned i=0;i<12;++i){key.mac[5]=i;auto r=many.observe(key,0,123,i*100);assert(r.alert==(i==11));assert(r.variants==1);}
  BleBurstMonitor repeat;key.mac[5]=42;
  for(unsigned i=0;i<1000;++i)assert(!repeat.observe(key,0,123,i*4).alert);
  BleBurstMonitor churn;for(unsigned i=0;i<24;++i){auto r=churn.observe(key,1,i,i*100);assert(r.alert==(i==23));if(r.alert)assert(r.addresses==1&&r.variants==24&&r.payloadChurn);}
  assert(!churn.observe(key,1,100,2500).alert); // cooldown
  BleBurstMonitor telemetry;for(unsigned i=0;i<50;++i)assert(!telemetry.observe(key,3,i,i*90).alert);
  BleBurstMonitor sparse;for(unsigned i=0;i<30;++i){key.mac[5]=i;assert(!sparse.observe(key,2,123,i*1000).alert);}
  BleBurstMonitor wrap;for(unsigned i=0;i<12;++i){key.mac[5]=i;auto r=wrap.observe(key,2,123,0xffffff00u+i*100);assert(r.alert==(i==11));}
  assert(!wrap.observe(key,4,1,2000).alert);
  // Normal changes and crowded scans stay bounded even when the table turns over.
  BleBurstMonitor crowded;for(unsigned i=0;i<50000;++i){key.mac[4]=i>>8;key.mac[5]=i;auto r=crowded.observe(key,i%4,i,i*3);assert(r.addresses<=96&&r.variants<=96);}
  SignalTrack slow,fast;for(unsigned ms=0;ms<=9000;ms+=10){int value=ms<3000?-80:-50;fast.add(value,ms);if(ms%100==0)slow.add(value,ms);}
  assert(slow.value()==fast.value()&&slow.value()>-55); // independent of packet rate
  assert(radarRadius(-20)==18&&radarRadius(-120)==158&&radarRadius(-50)<radarRadius(-80));
  for(int db=-120;db<0;++db)assert(radarRadius(db)>=radarRadius(db+1));
  SignalHistory h;h.add(-60,0);h.add(-61,500);h.add(-55,2500);
  assert(h.at(2)==SignalHistory::Gap&&h.recent(47,2500)==-55&&h.recent(46,2500)==SignalHistory::Gap);
  assert(h.recent(47,4000)==SignalHistory::Gap&&h.recent(44,4000)==-55);
  assert(h.recent(47,30000)==SignalHistory::Gap);
  h.add(-70,31000);assert(h.count==48&&h.at(47)==-70&&h.at(46)==SignalHistory::Gap);
  SignalHistory hw;hw.add(-60,0xffffff00u);hw.add(-50,744);assert(hw.at(1)==SignalHistory::Gap&&hw.at(2)==-50);
  puts("PASS: deauth retry/window evidence, BLE address/payload bursts, bounded floods, packet-rate-independent RSSI, gap-aware hunt trace");
}
