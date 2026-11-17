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
  // SSID conflict: one open AP sharing a mesh SSID is one incident, however
  // many protected nodes it pairs with; quiet 30 min.
  {uint8_t open[6]={1,2,3,4,5,6},n1[6]={9,2,3,4,5,7},other[6]={1,2,3,4,5,8};const uint8_t ssid[]="Cafe";
   assert(ssidConflictId(ssid,4,open)!=ssidConflictId(ssid,3,open)&&ssidConflictId(ssid,4,open)!=ssidConflictId(ssid,4,other));
   IncidentLedger l;const Incident pair{IncidentKind::EvilTwin,ssidConflictId(ssid,4,open),EvilTwinQuietMs};
   assert(l.claim(pair,0)==Verdict::Notify);for(uint32_t t=60000;t<=7200000;t+=60000)assert(l.claim(pair,t)==Verdict::Repeat);
   assert(l.repeats==120);(void)n1;}
  // Incident ledger full: ended incidents are reused first; then the oldest.
  {IncidentLedger l;for(uint32_t i=0;i<IncidentLedger::Size;++i)l.claim({IncidentKind::Deauth,i,1000},0);
   assert(l.claim({IncidentKind::Deauth,99,1000},2000)==Verdict::Notify&&l.evictions==0);
   for(uint32_t i=0;i<=IncidentLedger::Size;++i)l.claim({IncidentKind::BleBurst,i,DeauthQuietMs},3000+i);
   assert(l.evictions>0&&l.used()==IncidentLedger::Size);
   l.expire(0xFFFFFFF0u);assert(l.used()==0);}
  // Incident rollover.
  {IncidentLedger l;const Incident i{IncidentKind::Deauth,11,DeauthQuietMs};unsigned n=0;
   for(uint32_t t=0xFFFF0000u;t!=0x00100000u;t+=16384)n+=l.claim(i,t)==Verdict::Notify;assert(n==1);}
  // Not listening is not away: a watched AP beaconing 10/s, the radar paused
  // (clock, Room motion, a tool) for 20 or 60 min with the 60 s sweep still
  // running, then resumed: exactly one notification.
  for(uint32_t pause:{1200000u,3600000u}){static Visit t[64];VisitLedger l;l.begin(t,64);Radio r{&l};Contact c;const auto k=key(20);uint32_t now=0;
   for(;now<300000;now+=100)r.packet(c,k,now);
   l.listen(0,false,now);for(uint32_t end=now+pause;now<end;now+=60000)l.expire(now);
   assert(l.find(k)&&!l.gone(*l.find(k),now));l.listen(0,true,now);
   for(uint32_t end=now+600000;now<end;now+=100)r.packet(c,k,now);
   assert(r.notices==1);
   // The same visit still ends 15 listening minutes after its last packet.
   l.listen(0,false,now);l.listen(0,true,now+7200000);now+=7200000;assert(!l.gone(*l.find(k),now+VisitGoneMs-200));assert(l.claim(k,1,now+VisitGoneMs)==Verdict::Notify);}
  // Per radio: BLE-only mode pauses Wi-Fi visits only; a visit already over
  // at the pause stays over; a packet heard during the pause is not shifted.
  {static Visit t[64];VisitLedger l;l.begin(t,64);const auto w=key(21),b=key(22,true),old=key(23);
   l.claim(old,1,0);l.claim(w,1,VisitGoneMs);l.claim(b,1,VisitGoneMs);
   l.listen(0,false,VisitGoneMs+1000);l.expire(VisitGoneMs*3);
   assert(l.find(w)&&!l.find(b));assert(!l.find(old)||l.gone(*l.find(old),VisitGoneMs*3));
   l.listen(0,true,VisitGoneMs*3);assert(l.claim(w,1,VisitGoneMs*3+1000)==Verdict::Repeat);
   assert(l.claim(old,1,VisitGoneMs*3+1000)==Verdict::Notify);}
  // Incidents pause too: a deauth burst before and after a 20 min pause is one.
  {IncidentLedger l;const Incident i{IncidentKind::Deauth,7,incidentQuietMs(DeauthQuietMs,5)};
   assert(l.claim(i,0)==Verdict::Notify);l.listen(0,false,1000);l.expire(1300000);l.listen(0,true,1300000);
   assert(l.claim(i,1300000+60000)==Verdict::Repeat);
   // BLE paused leaves Wi-Fi incidents ageing.
   l.listen(1,false,1400000);assert(l.claim(i,1400000+VisitGoneMs)==Verdict::Notify);}
  // Incident quiet follows the visit setting: never under a visit, and "until
  // restart" never notifies twice.
  assert(incidentQuietMs(DeauthQuietMs,5)==VisitGoneMs&&incidentQuietMs(EvilTwinQuietMs,5)==EvilTwinQuietMs&&incidentQuietMs(DeauthQuietMs,10)==3600000&&incidentQuietMs(DeauthQuietMs,0)==0xFFFFFFFFu);
  {IncidentLedger l;const Incident i{IncidentKind::BleBurst,1,incidentQuietMs(BleBurstQuietMs,0)};unsigned n=0;
   for(uint32_t t=0;t<=86400000u;t+=7200000)n+=l.claim(i,t)==Verdict::Notify;l.expire(0xF0000000u);assert(n==1&&l.used()==1);}
  // A home AP heard all day with a disconnect burst every 11 or 20 minutes:
  // one notification. The incident is keyed by BSSID and channel, and the
  // participant's visit remembers it had one.
  for(uint32_t every:{660000u,1200000u}){static Visit t[64];VisitLedger visits;visits.begin(t,64);IncidentLedger incidents;
   const auto ap=key(30);const uint8_t bssid[6]={0x02,0,0,0,0,30};unsigned n=0;
   for(uint32_t now=0;now<=3*3600000u;now+=1000){visits.touch(ap,now);
     if(now%every==0){const Incident i{IncidentKind::Deauth,deauthId(bssid,6),incidentQuietMs(DeauthQuietMs,5)};
       auto v=incidents.claim(i,now);const auto own=visits.claimIncident(ap,uint8_t(i.kind),now);if(own!=Verdict::Notify)v=own;n+=v==Verdict::Notify;}}
   assert(n==1);
   // The incident is not the device's own notification.
   assert(visits.claim(ap,1,3*3600000u)==Verdict::Notify&&visits.claim(ap,1,3*3600000u+1)==Verdict::Repeat);}
  {const uint8_t a[6]={1,2,3,4,5,6},b[6]={1,2,3,4,5,7};assert(deauthId(a,6)!=deauthId(b,6)&&deauthId(a,6)!=deauthId(a,1));
   assert(incidentTag({IncidentKind::Deauth,0,0})!=0&&incidentTag({IncidentKind::Deauth,5,0})!=incidentTag({IncidentKind::BleBurst,5,0}));}
  // Sleep or restart: running visits come back from the RTC copy when the
  // time away keeps them under the gone time; the youngest are kept first.
  {static Visit t[64],u[64];VisitLedger a;a.begin(t,64);
   for(uint16_t i=0;i<10;++i)a.claim(key(40+i),1,i*60000);a.claimIncident(key(60),uint8_t(IncidentKind::Deauth),600000);
   VisitRecord keep[4];const unsigned n=a.save(keep,4,600000);assert(n==4);
   for(auto& r:keep)assert(r.age<=180000);
   VisitRecord all[16];const unsigned m=a.save(all,16,600000);assert(m==11);
   VisitLedger b;b.begin(u,64);b.restore(all,m,120000,500);assert(b.restored==11);
   assert(b.claim(key(49),1,600)==Verdict::Repeat);assert(b.claim(key(60),1,600)==Verdict::Notify);
   assert(b.claimIncident(key(60),uint8_t(IncidentKind::Deauth),700)==Verdict::Repeat);
   // key 40 was heard 10 min before the save: 10 + 6 min away is a new visit.
   VisitLedger c;c.begin(u,64);c.restore(all,m,360000,500);assert(c.claim(key(40),1,600)==Verdict::Notify&&c.claim(key(49),1,600)==Verdict::Repeat);
   VisitLedger d;d.begin(u,64);d.restore(all,m,VisitGoneMs,500);assert(d.restored==0);}
  // A Lite-size table holds a crowd: 160 present devices, none evicted.
  {static Visit t[256];VisitLedger l;l.begin(t,256);
   for(uint16_t i=0;i<160;++i)assert(l.claim(key(1000+i*7),1,i)==Verdict::Notify);
   for(uint16_t i=0;i<160;++i)assert(l.claim(key(1000+i*7),1,1000+i)==Verdict::Repeat);
   assert(l.evictions==0);}
  // A dismissed device card cannot come back from the queue.
  {NoticeQueue q;DetectionNotice n;n.hasKey=true;n.key=key(1);n.at=1;q.push(n);n.key=key(2);q.push(n);
   q.drop(key(1));DetectionNotice out;assert(q.take(2,out)&&out.key==key(2)&&!q.take(2,out));}
}
