#pragma once
#include "csi_networks.h"
namespace csinetworkstest {
using dragon::csi::Candidate;using dragon::csi::Evidence;
static Candidate ap(uint8_t id,int8_t rssi,uint8_t channel=6,uint32_t age=1000,Evidence e=Evidence::Unknown,bool last=false){Candidate c;c.mac[5]=id;c.rssi=rssi;c.channel=channel;c.age=age;c.evidence=e;c.last=last;c.slot=id;return c;}
}
static void csiNetworksTests(){
 using namespace csinetworkstest;using namespace dragon::csi;uint16_t order[8];
 // Signal order before any evidence; stale and channel-less entries are hidden.
 {Candidate c[]={ap(1,-70),ap(2,-40),ap(3,-55),ap(4,-30,6,FreshMs),ap(5,-20,0)};
  assert(rank(c,5,order,8)==3&&c[order[0]].mac[5]==2&&c[order[1]].mac[5]==3&&c[order[2]].mac[5]==1);
  assert(pick(c,order,3)==1);}
 // OFDM-proven networks lead, no-data networks sink; ties break on BSSID.
 {Candidate c[]={ap(1,-40,6,0,Evidence::NoData),ap(2,-80,1,0,Evidence::Ofdm),ap(3,-60),ap(4,-60)};
  assert(rank(c,4,order,8)==4&&order[0]==1&&order[1]==2&&order[2]==3&&order[3]==0);
  assert(pick(c,order,4)==1);}
 // Auto never picks a no-data network; Next skips the current one.
 {Candidate c[]={ap(1,-30,6,0,Evidence::NoData),ap(2,-50),ap(3,-60)};unsigned n=rank(c,3,order,8);
  assert(pick(c,order,n)==1&&pick(c,order,n,c[1].mac)==2);
  assert(pick(c,order,n,c[2].mac)==1);
  c[1].age=c[2].age=FreshMs+5000;assert(rank(c,3,order,8)==1&&rank(c,3,order,8,NextFreshMs)==3);n=rank(c,3,order,8,NextFreshMs);assert(pick(c,order,n,c[1].mac)==2);
  Candidate d[]={ap(1,-30,6,0,Evidence::NoData)};n=rank(d,1,order,8);assert(n==1&&pick(d,order,n)==-1);
  assert(rank(d,0,order,8)==0&&pick(d,order,0)==-1);}
 // A bounded list keeps the best entries in order.
 {Candidate c[12];for(unsigned i=0;i<12;++i)c[i]=ap(uint8_t(i+1),int8_t(-90+int(i)*5));
  assert(rank(c,12,order,4)==4&&c[order[0]].mac[5]==12&&c[order[1]].mac[5]==11&&c[order[2]].mac[5]==10&&c[order[3]].mac[5]==9);
  assert(rank(c,12,order,0)==0);}
 // Last-used network is offered only when heard (listable).
 {Candidate c[]={ap(1,-50),ap(2,-70,11,500,Evidence::Unknown,true)};unsigned n=rank(c,2,order,8);assert(lastHeard(c,order,n)==1);
  c[1].age=FreshMs+1;n=rank(c,2,order,8);assert(lastHeard(c,order,n)==-1);}
 // Evidence table: update in place, fill, then evict the oldest.
 {EvidenceTable<2> t;uint8_t a[6]{1},b[6]{2},x[6]{3};assert(t.get(a)==Evidence::Unknown);
  t.note(a,Evidence::Ofdm,10);t.note(b,Evidence::NoData,20);t.note(a,Evidence::NoData,30);assert(t.get(a)==Evidence::NoData&&t.get(b)==Evidence::NoData);
  t.note(x,Evidence::Ofdm,40);assert(t.get(b)==Evidence::Unknown&&t.get(a)==Evidence::NoData&&t.get(x)==Evidence::Ofdm);
  assert(!strcmp(evidenceName(Evidence::NoData),"no_data"));}
}
