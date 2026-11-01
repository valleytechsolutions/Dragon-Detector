#include "target_pages.h"
static void detectionRefreshTests(){
  // Legacy all-bits settings and watch rules cannot opt into a new adult category.
  DetectorSettings settings;assert(!settings.enabled(Family::Adult));
  for(uint32_t mask:{0u,0xffffffffu,0x03ffffffu}){
    settings.families=adultOptInMask(mask,false);assert(!settings.enabled(Family::Adult));
    assert((settings.families&~(1u<<unsigned(Family::Adult)))==(mask&~(1u<<unsigned(Family::Adult))));
  }
  Contact c;c.used=true;c.packets=c.matchHits=1;c.match={Family::Adult,Confidence::Medium,"fixture"};c.seen=c.matchedAt=100;
  assert(!candidateVisible(c,100,settings,true,false,true));assert(!recentCandidateVisible(c,30100,settings,false,true));
  settings.families=adultOptInMask(settings.families,true);assert(candidateVisible(c,100,settings,false,false,false));
  auto adult=[](const std::vector<uint8_t>& data,bool enabled=true,size_t boundary=0){return parseBle(data.data(),data.size(),boundary,enabled);};
  for(const char* value:{"LVS-A001","LOVE-Z12"}){
    auto data=name(value);assert(adult(data,false).match.family!=Family::Adult);
    assert(adult(data).match.family==Family::Adult&&adult(data).match.confidence==Confidence::Low);
    append(data,{3,3,0xf0,0xff});assert(adult(data).match.confidence==Confidence::Medium);
  }
  for(const char* value:{"Nova","Sync","Bloom","LVS-","LOVE-","LOVE-My Phone","I LOVE-X","LVS-A!"})assert(adult(name(value)).match.family!=Family::Adult);
  const std::vector<std::vector<uint8_t>> services={
    {0,0,0,0,0,0,0,0xb0,0,0x40,0x51,4,3,0xbb,0,0xf0},
    {0xe8,0x80,0x9d,0xc9,0xeb,0x47,0x6e,0x8a,0xc7,0x47,0xe7,0xc5,0,0x15,0x36,0x51},
    {0x53,0x56,0x4c,0x0e,0x92,0xa6,0xd5,0xbb,0xd4,0x4b,0x23,0,1,0,0x30,0x40}
  };
  for(auto id:services)for(uint8_t type:{6,7,0x21}){
    std::vector<uint8_t> data{17,type};append(data,id);auto f=adult(data);
    assert(f.valid&&f.match.family==Family::Adult&&f.match.confidence==Confidence::Medium);
    assert(adult(data,false).match.family!=Family::Adult);
    auto broken=data;broken.pop_back();assert(!adult(broken).valid);
    auto embedded=data;embedded[1]=0xff;assert(adult(embedded).match.family!=Family::Adult);
    auto badBoundary=data;assert(!adult(badBoundary,true,10).valid);
    append(data,{3,3,0x81,0xfc});assert(adult(data).match.family==Family::Axon);
  }
  assert(adult({3,3,0xf0,0xff}).match.family!=Family::Adult);
  std::vector<uint8_t> uart{17,7,0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,1,0,0x40,0x6e};
  assert(adult(uart).match.family!=Family::Adult);auto brand=name("LVS-A");size_t split=brand.size();append(brand,uart);assert(adult(brand,true,split).match.confidence==Confidence::Medium);
  for(bool radar:{false,true})for(bool pending:{false,true})assert(detectionPopupAllowed(radar,pending)==(radar&&pending));
  TargetPages pages;assert(pages.pages()==0&&pages.slice(0).count==0);
  for(int i=0;i<21;++i)pages.add(Family::Flock);pages.add(Family::Axon);pages.add(Family::Camera);pages.add(Family::Tracker);pages.add(Family::SmartTag);
  assert(pages.pages()==6);auto s=pages.slice(2);assert(s.group==TargetGroup::Alpr&&s.offset==20&&s.count==1&&s.page==2&&s.pages==3);
  s=pages.slice(3);assert(s.group==TargetGroup::Axon&&s.offset==21&&s.count==1);
  s=pages.slice(4);assert(s.group==TargetGroup::Cameras&&s.offset==22);
  s=pages.slice(5);assert(s.group==TargetGroup::Trackers&&s.offset==23&&s.count==2);assert(pages.slice(6).count==0);
  assert(familyPriority(Family::Alpr)>familyPriority(Family::Axon)&&familyPriority(Family::Axon)>familyPriority(Family::Camera));
  AxonEvidence clue;clue.signals=AxonResearch;
  Match drone{Family::Drone,Confidence::Strong,"decoded Remote ID"};assert(withAxon(drone,clue).family==Family::Drone);
  assert(!strongerMatch(clue.match(),drone)&&strongerMatch(drone,clue.match()));
  NoticeQueue q;DetectionNotice n;n.at=1;n.family=Family::Axon;q.push(n);n.family=Family::Alpr;q.push(n);DetectionNotice out;
  assert(q.take(2,out)&&out.family==Family::Alpr);assert(q.take(2,out)&&out.family==Family::Axon);
  puts("PASS: adult opt-in/migration/negative UUID and name fixtures; radar-only notices; category pages; ALPR-first priority; stronger protocol retention");
}
