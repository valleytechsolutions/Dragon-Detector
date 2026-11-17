#pragma once
#include "alert_policy.h"
#include "contacts.h"
#include "notifications.h"
namespace alertpolicytest {
using namespace dragon;
static DeviceKey key(uint16_t id,bool ble=false){DeviceKey k;k.mac[0]=0x02;k.mac[4]=id>>8;k.mac[5]=id&0xff;k.radio=ble;return k;}
// Mirrors radio.cpp update(): 30 s silence restarts the evidence run, every
// packet touches the ledger, and an eligible packet makes at most one decision.
struct Radio {
  VisitLedger* ledger;unsigned notices=0,repeats=0;uint16_t revision=0;
  void packet(Contact& c,const DeviceKey& k,uint32_t now,bool eligible=true){
    if(c.packets&&uint32_t(now-c.seen)>30000)c.signatureAlerted=false;
    c.seen=now;++c.packets;ledger->touch(k,now);
    if(!eligible||!alertDecisionDue(c.signatureAlerted,c.alertRevision,revision))return;
    if(ledger->claim(k,uint8_t(Family::Camera),now)==Verdict::Notify)++notices;else ++repeats;
  }
};
}
static void alertPolicyTests(){
  using namespace alertpolicytest;
  static Visit table[512];VisitLedger visits;visits.begin(table,512);
  assert(visitGoneMs(5)==900000&&visitGoneMs(10)==3600000&&visitGoneMs(0)==0&&visitGoneMs(7)==900000);
  // A watched AP beaconing 10/s for an hour: one notification, no decision per packet.
  {Radio r{&visits};Contact c;const auto k=key(1);
   for(uint32_t t=1000;t<=1000+3600000;t+=100)r.packet(c,k,t);
   assert(r.notices==1&&r.repeats==0&&visits.repeats==0);}
  // Even if something asks again every 30 s (settings changes), it stays one.
  {Radio r{&visits};Contact c;const auto k=key(2);
   for(uint32_t t=0;t<=7200000;t+=100){if(t%30000==0)++r.revision;r.packet(c,k,t);}
   assert(r.notices==1&&r.repeats==240);}
  // A camera AP present for three hours with 40 s gaps (contact expiry each
  // time): one notification; every return inside a visit is a silent repeat.
  {Radio r{&visits};Contact c;const auto k=key(3);
   for(uint32_t t=0;t<=3*3600000u;t+=41000)r.packet(c,k,t);
   assert(r.notices==1&&r.repeats>200);}
  // Away 14:59 is the same visit; away 16 min is a new one.
  {Radio r{&visits};Contact c;const auto k=key(4);
   r.packet(c,k,0);r.packet(c,k,899999);assert(r.notices==1&&r.repeats==1);
   r.packet(c,k,899999+960000);assert(r.notices==2);
   Contact again;r.packet(again,k,899999+960000+100);assert(r.notices==2);}
  // Contact eviction (a brand new Contact for the same key) is not a new visit.
  {Radio r{&visits};const auto k=key(5);
   for(uint32_t t=0;t<600000;t+=5000){Contact fresh;r.packet(fresh,k,t);}
   assert(r.notices==1&&r.repeats==119);}
  // 60 min setting, and 0 = once until restart (no packets for a day).
  {VisitLedger l;static Visit t[64];l.begin(t,64);const auto k=key(6);
   l.goneMs=visitGoneMs(10);assert(l.claim(k,1,0)==Verdict::Notify);assert(l.claim(k,1,3599999)==Verdict::Repeat);assert(l.claim(k,1,3599999+3600000)==Verdict::Notify);
   l.goneMs=visitGoneMs(0);assert(l.claim(k,1,86400000u*2)==Verdict::Repeat);l.expire(86400000u*3);assert(l.used()==1);}
  // A touch never revives a visit that has ended.
  {VisitLedger l;static Visit t[64];l.begin(t,64);const auto k=key(7);
   assert(l.claim(k,1,1000)==Verdict::Notify);l.touch(k,1000+VisitGoneMs);assert(l.claim(k,1,1000+VisitGoneMs+10)==Verdict::Notify);
   l.touch(key(8),5);assert(!l.find(key(8)));}
  // millis() rollover: one visit across the wrap, then gone 15 min after it.
  {VisitLedger l;static Visit t[64];l.begin(t,64);const auto k=key(9);Contact c;Radio r{&l};
   for(uint32_t t=0xFFFFF000u;t!=0x00010000u;t+=64)r.packet(c,k,t);
   assert(r.notices==1);assert(l.claim(k,1,0x0000FFC0u+VisitGoneMs-1)==Verdict::Repeat);
   assert(l.claim(k,1,0x0000FFC0u+3*VisitGoneMs)==Verdict::Notify);}
  // Full ledger: ended visits are reused before present ones are evicted, the
  // eviction counter counts only present entries, and a present key survives.
  {VisitLedger l;static Visit t[64];l.begin(t,64);
   for(uint16_t i=0;i<64;++i)l.claim(key(100+i),1,0);
   const auto keep=key(500);
   for(uint16_t i=0;i<600;++i){const uint32_t now=VisitGoneMs+1000+i*10;l.claim(keep,1,now);l.claim(key(1000+i),1,now);l.touch(keep,now);}
   assert(l.find(keep));assert(l.claim(keep,1,VisitGoneMs+7000)==Verdict::Repeat);
   assert(l.used()<=64&&l.evictions>0);
   // A table no larger than one probe window shows the exact order.
   VisitLedger g;static Visit u[8];g.begin(u,8);for(uint16_t i=0;i<8;++i)assert(g.claim(key(200+i),1,i)==Verdict::Notify);
   assert(g.evictions==0);g.touch(key(200),VisitGoneMs-1);
   for(uint16_t i=0;i<7;++i)g.claim(key(300+i),1,VisitGoneMs+10);
   assert(g.evictions==0&&g.find(key(200)));
   assert(g.claim(key(400),1,VisitGoneMs+20)==Verdict::Notify&&g.evictions==1&&!g.find(key(200))&&g.find(key(306)));
   g.expire(3*VisitGoneMs);assert(g.used()==0);}
  // Deauth bursts every 15 s for an hour on CH 6: one; quiet 10 min: one more;
  // CH 1 at the same time is its own incident.
  {IncidentLedger l;unsigned n=0;const Incident ch6{IncidentKind::Deauth,6,DeauthQuietMs},ch1{IncidentKind::Deauth,1,DeauthQuietMs};
   for(uint32_t t=0;t<=3600000;t+=15000)n+=l.claim(ch6,t)==Verdict::Notify;assert(n==1);
   assert(l.claim(ch1,30000)==Verdict::Notify);
   assert(l.claim(ch6,3600000+DeauthQuietMs-1)==Verdict::Repeat);
   assert(l.claim(ch6,3600000+2*DeauthQuietMs)==Verdict::Notify);}
  // BLE bursts every 30 s: one per group per quiet period.
  {IncidentLedger l;unsigned n[4]{};
   for(uint32_t t=0;t<=3600000;t+=30000)for(uint32_t g=0;g<4;++g)n[g]+=l.claim({IncidentKind::BleBurst,g,BleBurstQuietMs},t)==Verdict::Notify;
   for(unsigned g=0;g<4;++g)assert(n[g]==1);
   unsigned spaced=0;for(uint32_t t=0;t<=3*BleBurstQuietMs;t+=BleBurstQuietMs)spaced+=l.claim({IncidentKind::BleBurst,0,BleBurstQuietMs},5000000+t)==Verdict::Notify;assert(spaced==4);}
  // SSID conflict: the pair in either order is one incident, quiet 30 min.
  {uint8_t a[6]={1,2,3,4,5,6},b[6]={1,2,3,4,5,7};const uint8_t ssid[]="Cafe";
   assert(ssidPairId(a,b,ssid,4)==ssidPairId(b,a,ssid,4)&&ssidPairId(a,b,ssid,4)!=ssidPairId(a,b,ssid,3));
   IncidentLedger l;const Incident pair{IncidentKind::EvilTwin,ssidPairId(a,b,ssid,4),EvilTwinQuietMs},swapped{IncidentKind::EvilTwin,ssidPairId(b,a,ssid,4),EvilTwinQuietMs};
   assert(l.claim(pair,0)==Verdict::Notify);for(uint32_t t=60000;t<=7200000;t+=60000)assert(l.claim(t%120000?pair:swapped,t)==Verdict::Repeat);
   assert(l.repeats==120);}
  // Incident ledger full: ended incidents are reused first; then the oldest.
  {IncidentLedger l;for(uint32_t i=0;i<IncidentLedger::Size;++i)l.claim({IncidentKind::Deauth,i,1000},0);
   assert(l.claim({IncidentKind::Deauth,99,1000},2000)==Verdict::Notify&&l.evictions==0);
   for(uint32_t i=0;i<=IncidentLedger::Size;++i)l.claim({IncidentKind::BleBurst,i,DeauthQuietMs},3000+i);
   assert(l.evictions>0&&l.used()==IncidentLedger::Size);
   l.expire(0xFFFFFFF0u);assert(l.used()==0);}
  // Incident rollover.
  {IncidentLedger l;const Incident i{IncidentKind::Deauth,11,DeauthQuietMs};unsigned n=0;
   for(uint32_t t=0xFFFF0000u;t!=0x00100000u;t+=16384)n+=l.claim(i,t)==Verdict::Notify;assert(n==1);}
  // A dismissed device card cannot come back from the queue.
  {NoticeQueue q;DetectionNotice n;n.hasKey=true;n.key=key(1);n.at=1;q.push(n);n.key=key(2);q.push(n);
   q.drop(key(1));DetectionNotice out;assert(q.take(2,out)&&out.key==key(2)&&!q.take(2,out));}
}
