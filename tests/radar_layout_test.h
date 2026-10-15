#include "radar_layout.h"
#include "notifications.h"
static void radarLayoutTests(){
  for(int radius=18;radius<=158;++radius)for(int degrees=0;degrees<360;++degrees){float a=degrees*.0174532925f;auto p=radarPlotPoint(radius,cosf(a),sinf(a));assert(p.y>=104&&p.y<=242);assert(fabs(hypotf(p.x-180,p.y-180)-radius)<.8f);}
  // Dense tracker traffic must not win the primary label, sound or card.
  RadarLabelCandidate candidates[MAX_CONTACTS];for(auto& c:candidates)c={Family::Tracker};
  candidates[383]={Family::Axon};candidates[382]={Family::Flipper};candidates[381]={Family::Meta};candidates[10].selected=true;
  unsigned labels[RadarLabelLimit+1];labels[RadarLabelLimit]=999;
  for(unsigned limit:{RadarLabelLimit,LiteRadarLabelLimit}){auto count=selectRadarLabels(candidates,MAX_CONTACTS,labels,limit);
    assert(count==4&&labels[0]==383&&labels[1]==10&&labels[RadarLabelLimit]==999);
  }
  candidates[383].stale=true;assert(selectRadarLabels(candidates,MAX_CONTACTS,labels,6)==3&&labels[0]==10);
  assert(selectRadarLabels(candidates,MAX_CONTACTS,labels,6,false)==1&&labels[0]==10);
  assert(selectRadarLabels(candidates,MAX_CONTACTS,labels,0)==0);
  for(auto& c:candidates)c={Family::Tracker};assert(selectRadarLabels(candidates,MAX_CONTACTS,labels,6)==0);
  Contact tracker,axon;tracker.used=axon.used=true;tracker.seen=axon.seen=100;tracker.rssi=-30;axon.rssi=-90;
  tracker.match={Family::Tracker,Confidence::Strong,"test"};axon.match={Family::Axon,Confidence::Low,"test"};assert(preferContact(axon,tracker,100));
  for(Family family:{Family::Flipper,Family::Flock,Family::Alpr,Family::Meta,Family::Ring,Family::Camera,Family::Drone}){
    Match match{family,Confidence::Low,"test"};assert(signatureAlertReady(match,1,100,100,true)&&alertConfidenceAllowed(match,Confidence::Strong,true));
    Contact important=axon;important.match=match;assert(preferContact(important,tracker,100)&&preferContact(axon,important,100));
  }
  NoticeQueue queue;DetectionNotice item;item.at=100;item.hasKey=true;item.family=Family::Camera;
  for(unsigned i=0;i<8;++i){item.key.mac[5]=i;queue.push(item);}item.key.mac[5]=9;item.family=Family::Axon;queue.push(item);
  DetectionNotice out;assert(queue.take(101,out)&&out.family==Family::Axon);item.at=200;queue.push(item);queue.push(item);assert(queue.count==8);assert(!queue.take(30200,out));
  item.at=0xfffffff0;queue.push(item);assert(queue.take(10,out));
  // The larger Lite plates must fit inside the round panel, clear its status
  // and footer, and never overlap even when four targets cluster together.
  for(unsigned run=0;run<2000;++run){RadarPoint points[4];uint8_t slots[5]={255,255,255,255,231},again[4];unsigned count=1+run%4;
    for(unsigned i=0;i<count;++i)points[i]={25+int((run*71+i*43)%310),104+int((run*31+i*67)%139)};
    placeRadarLabels(points,count,slots,nullptr,true);placeRadarLabels(points,count,again,nullptr,true);uint16_t used=0;
    assert(slots[4]==231);
    for(unsigned i=0;i<count;++i){assert(slots[i]<6&&!(used&(1u<<slots[i]))&&slots[i]==again[i]);used|=1u<<slots[i];auto a=radarLabelSlot(slots[i],true);
      assert(a.y>=105&&a.y+a.h<282&&a.w>=138&&a.h>=46);
      for(int x:{a.x,a.x+a.w})for(int y:{a.y,a.y+a.h})assert(hypotf(x-180,y-180)<178);
      for(unsigned j=0;j<i;++j){auto b=radarLabelSlot(slots[j],true);assert(a.x+a.w<b.x||b.x+b.w<a.x||a.y+a.h<b.y||b.y+b.h<a.y);}
    }
  }
  for(unsigned run=0;run<2000;++run){RadarPoint points[6];uint8_t slots[6],again[6];unsigned count=1+run%6;
    for(unsigned i=0;i<count;++i)points[i]={25+int((run*71+i*43)%310),30+int((run*31+i*67)%300)};
    placeRadarLabels(points,count,slots);placeRadarLabels(points,count,again);uint16_t used=0;
    for(unsigned i=0;i<count;++i){assert(slots[i]<12&&!(used&(1u<<slots[i]))&&slots[i]==again[i]);used|=1u<<slots[i];auto a=radarLabelSlot(slots[i]);
      assert(a.x>=30&&a.x+a.w<=330&&a.y>=102&&a.y+a.h<=254);
      for(unsigned j=0;j<i;++j){auto b=radarLabelSlot(slots[j]);assert(a.x+a.w<b.x||b.x+b.w<a.x||a.y+a.h<b.y||b.y+b.h<a.y);}
    }
  }
  RadarPoint center{100,180};uint8_t preferred,stable;placeRadarLabels(&center,1,&preferred);++center.x;placeRadarLabels(&center,1,&stable,&preferred);assert(stable==preferred);
  RadarPoint near{100,180};uint8_t slot;placeRadarLabels(&near,1,&slot);auto b=radarLabelSlot(slot);assert(near.x<b.x-9||near.x>b.x+b.w+9||near.y<b.y-9||near.y>b.y+b.h+9);
  Contact c;c.used=true;c.packets=1;c.matchHits=1;c.match={Family::Pineapple,Confidence::Low,"synthetic fixture"};c.seen=c.matchedAt=100;
  DetectorSettings settings;assert(!recentCandidateVisible(c,30099,settings,false,false));assert(recentCandidateVisible(c,30100,settings,false,false));assert(recentCandidateVisible(c,120099,settings,false,false));assert(!recentCandidateVisible(c,120100,settings,false,false));
  assert(!recentCandidateVisible(c,40000,settings,true,true));settings.families=0;assert(!recentCandidateVisible(c,40000,settings,false,false));assert(recentCandidateVisible(c,40000,settings,false,true));
  settings=DetectorSettings{};c.matchHits=0;assert(!recentCandidateVisible(c,40000,settings,false,false));c.matchHits=1;c.matchedAt=0xffff0000;assert(!recentCandidateVisible(c,90000,settings,false,false));
  c.seen=c.matchedAt=0xfffffff0;assert(recentCandidateVisible(c,30000,settings,false,false));assert(!recentCandidateVisible(c,120000,settings,false,false));
  puts("PASS: 384-contact tracker crowd, Axon primary + tapped tag, important first-hit alerts, priority notification queue, label bounds and stale expiry");
}
