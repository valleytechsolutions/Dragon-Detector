#pragma once
// Wardriving node link against simulated cores. The simulated cores apply the
// acceptance rules of the real ones: a JCMK core ignores short requests and
// text, a Biscuit Manager drops anything under 212 bytes and only registers a
// node that completes its identify / role / channel handshake.
#include "node_link.h"
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace nodetest {
using namespace dragon::node;
using NodeFamily=dragon::node::Family; // test files also see dragon::Family
using Mac=std::array<uint8_t,6>;
using Bytes=std::vector<uint8_t>;
struct Ap {Mac bssid;std::string ssid;Auth auth;uint8_t channel;int rssi;};
struct Heard {Mac from;Bytes data;uint32_t at;bool sealed;};
struct Sent {Mac to;Bytes data;uint32_t at;};
inline bool isBroadcast(const Mac& mac){return sameMac(mac.data(),BroadcastMac);}
inline std::string textOf(const Bytes& frame){Frame f;assert(parseFrame(frame.data(),frame.size(),f));return std::string((const char*)f.text,f.textLength);}
inline Bytes textFrame(uint8_t type,uint32_t counter,const std::string& text){Bytes b(FrameSize);buildFrame(b.data(),type,counter,text.data(),text.size());return b;}
inline Bytes adminFrame(uint8_t version,uint8_t index,uint8_t count,uint8_t first,uint8_t last){return Bytes{'E','N','O','W',Admin,version,index,count,first,last};}

struct Bench : Port {
  Link link;std::vector<Seen> table;Config config;uint32_t now=0;
  uint8_t channel=0,scanChannel=0;bool scanning=false,bleOn=false,inFlight=false,rejectTransmit=false,rejectScan=false,rejectPeer=false,loseScans=false;
  uint32_t scanEnds=0,flightEnds=0;Mac flightTo{};Bytes flightData;
  std::map<Mac,Peer> peers;std::vector<Sent> log;std::deque<Heard> inbox;
  std::map<uint8_t,std::vector<Ap>> world;std::vector<std::pair<Mac,int>> bleWorld;
  std::vector<uint8_t> scanned;unsigned scanStops=0,bleStarts=0,tunes=0;
  // The other end of the air. Returns whether a unicast frame was acknowledged.
  std::function<bool(const Mac&,const Bytes&)> air;

  explicit Bench(NodeFamily family,size_t entries=256,uint32_t start=1000){
    table.resize(entries);config.family=family;config.identity="DragonDetector:0.14.8";config.seen=table.data();config.seenCount=entries;
    config.seed=7;now=start;
  }
  bool begin(){return link.begin(*this,config,now);}
  bool tune(uint8_t c) override {assert(!scanning);channel=c;++tunes;return true;}
  bool peer(const uint8_t* mac,Peer mode) override {
    if(rejectPeer&&mode!=Peer::Remove)return false;
    Mac m;memcpy(m.data(),mac,6);if(mode==Peer::Remove)peers.erase(m);else peers[m]=mode;return true;
  }
  bool transmit(const uint8_t* to,const uint8_t* frame,size_t length) override {
    assert(length==FrameSize);                    // a Biscuit drops anything shorter
    assert(channel==HomeChannel&&!scanning&&!bleOn); // cores only listen on the home channel
    assert(!inFlight);                            // one frame at a time, paced by its report
    Mac m;memcpy(m.data(),to,6);
    assert(peers.count(m)||isBroadcast(m));
    if(rejectTransmit)return false;
    inFlight=true;flightTo=m;flightData.assign(frame,frame+length);flightEnds=now+2;return true;
  }
  bool scan(uint8_t c,uint16_t dwell) override {
    assert(!scanning&&!inFlight&&!bleOn);assert(c>=1&&c<=config.maxChannel);assert(dwell>=DwellMinMs&&dwell<=DwellMaxMs);
    if(rejectScan)return false;
    scanning=true;scanChannel=channel=c;scanEnds=now+dwell+12;scanned.push_back(c);return true;
  }
  void scanStop() override {scanning=false;++scanStops;}
  void ble(bool on,uint32_t) override {assert(!scanning);if(on&&!bleOn)++bleStarts;bleOn=on;}
  // sealed: encrypted by the sender, readable only while this node holds the key for it.
  void hear(const Mac& from,const Bytes& data,uint32_t delay=1,bool sealed=false){inbox.push_back({from,data,now+delay,sealed});}
  bool keyed(const Mac& mac) const {auto it=peers.find(mac);return it!=peers.end()&&it->second==Peer::Encrypted;}
  void step(){
    ++now;
    if(inFlight&&int32_t(now-flightEnds)>=0){
      inFlight=false;log.push_back({flightTo,flightData,now});
      const bool acknowledged=air?air(flightTo,flightData):false;
      link.sent(isBroadcast(flightTo)||acknowledged,now);
    }
    if(scanning&&!loseScans&&int32_t(now-scanEnds)>=0){
      for(const Ap& ap:world[scanChannel])link.wifi(ap.bssid.data(),(const uint8_t*)ap.ssid.data(),ap.ssid.size(),ap.auth,ap.channel,ap.rssi);
      scanning=false;link.scanned(true,now);
    }
    if(bleOn&&now%20==0)for(auto& d:bleWorld)link.ble(d.first.data(),d.second);
    for(size_t i=0;i<inbox.size();){
      if(int32_t(now-inbox[i].at)<0){++i;continue;}
      Heard h=inbox[i];inbox.erase(inbox.begin()+long(i));
      if(h.sealed&&!keyed(h.from))continue;                                                  // no key: unreadable
      if(channel==HomeChannel)link.received(h.from.data(),h.data.data(),h.data.size(),-48,now); // off channel: lost
    }
    link.loop(now);
  }
  void run(uint32_t ms){for(uint32_t i=0;i<ms;++i)step();}
  template<class F> bool until(F done,uint32_t limit=20000){for(uint32_t i=0;i<limit;++i){if(done())return true;step();}return done();}
  unsigned count(uint8_t type) const {unsigned n=0;for(auto& s:log)if(s.data[4]==type)++n;return n;}
  std::vector<std::string> texts() const {std::vector<std::string> out;for(auto& s:log)if(s.data[4]==Text)out.push_back(textOf(s.data));return out;}
};

// JustCallMeKoko core: replies to a full-size request, assigns an index range
// when its table changes, and only does so on a request or a heartbeat.
struct JcmkCore {
  Mac mac{{0x24,0x6f,0x28,0x10,0x20,0x30}};Bench* bench=nullptr;bool alive=true,acks=false,acksUnicast=true,dirty=true,known=false;
  // keyed: "Use Encryption" is on. The core answers a request in the clear, then
  // holds the node as an encrypted peer. mute: its radio acknowledges, its firmware does not answer.
  bool keyed=false,nodeKeyed=false,mute=false;unsigned dropReplies=0,unread=0;
  uint8_t version=1,first=0,last=13;std::vector<std::string> records;unsigned heartbeats=0,requests=0;std::vector<uint32_t> counters;
  bool operator()(const Mac& to,const Bytes& data){
    if(!alive)return false;
    if(!isBroadcast(to)&&to!=mac)return false;
    Frame f;if(!parseFrame(data.data(),data.size(),f))return true;
    if(data.size()<FrameSize)return true;                        // the core ignores short text messages
    if(keyed){
      if(f.type==CoreRequest)assert(!bench->keyed(mac));         // asked in the clear, so the clear answer is readable
      else{
        assert(!isBroadcast(to)&&bench->keyed(mac));             // everything else is sealed and addressed
        if(!nodeKeyed){++unread;return acksUnicast;}             // acknowledged by the radio, unreadable to the core
      }
    }
    if(mute)return acksUnicast;
    if(f.type==CoreRequest){
      ++requests;known=true;nodeKeyed=keyed;
      if(dropReplies)--dropReplies;else bench->hear(mac,textFrame(CoreReply,0,""));
      admin();
    }
    else if(f.type==Heartbeat){++heartbeats;counters.push_back(f.counter);if(!known){known=true;dirty=true;}admin();}
    else if(f.type==Text){
      const std::string line((const char*)f.text,f.textLength);
      unsigned commas=0;for(char c:line)if(c==',')++commas;
      assert(commas==5);                                         // exactly six fields, or the core rejects it
      records.push_back(line);
      if(acks)bench->hear(mac,textFrame(RecordAck,f.counter,line.back()=='B'?"B,0":"W,2437"),3);
    }
    return acksUnicast;
  }
  void admin(){if(dirty){dirty=false;bench->hear(mac,adminFrame(version,0,1,first,last),2,keyed);}}
};
// Biscuit Manager in Extended mode, as its handshake is known from the outside.
struct BiscuitCore {
  Mac mac{{0xd8,0x3b,0xda,0x01,0x02,0x03}};Bench* bench=nullptr;bool alive=true,secure=false,registered=false,roleAcked=false,heartbeat=true;
  std::string channels="channels=1,6,11;dwell=120",identity;std::vector<std::string> records;unsigned heartbeats=0,identifies=0;uint8_t roleEcho=0;
  bool operator()(const Mac& to,const Bytes& data){
    if(!alive)return false;
    if(!isBroadcast(to)&&to!=mac)return false;
    if(data.size()<FrameSize)return true;
    Frame f;if(!parseFrame(data.data(),data.size(),f))return true;
    const std::string text((const char*)f.text,f.textLength);
    switch(f.type){
      case CoreRequest:registered=roleAcked=false;bench->hear(mac,textFrame(CoreReply,0,secure?"v=3;encrypt=1":"v=3;encrypt=0;nodes=20"));break;
      case Identify:assert(!isBroadcast(to));++identifies;identity=text;{Bytes role=textFrame(Admin,0,std::string(1,char(1)));bench->hear(mac,role,4);}break;
      case RoleAck:assert(!isBroadcast(to));roleAcked=true;roleEcho=uint8_t(f.counter);bench->hear(mac,textFrame(ConfigUpdate,0,channels),4);break;
      case ConfigAck:assert(!isBroadcast(to));if(roleAcked)registered=true;break;
      case Heartbeat:++heartbeats;break;
      case Text:if(registered)records.push_back(text);break;
      default:break;
    }
    return true;
  }
};
inline Mac bssid(unsigned n){return Mac{{0xaa,0xbb,0xcc,uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)}};}
inline bool hasLine(const std::vector<std::string>& lines,const std::string& line){for(auto& l:lines)if(l==line)return true;return false;}
inline unsigned countLine(const std::vector<std::string>& lines,const std::string& line){unsigned n=0;for(auto& l:lines)if(l==line)++n;return n;}
}

static void nodeWireTests(){
  using namespace nodetest;
  // Wire layout: 212 bytes, little-endian counter and length, text at offset 11.
  uint8_t frame[FrameSize];memset(frame,0xee,sizeof(frame));
  assert(buildFrame(frame,Text,0x01020304,"hello",5)==212&&FrameSize==212&&AdminSize==10&&ChannelCount==40);
  assert(!memcmp(frame,"ENOW",4)&&frame[4]==4&&frame[5]==4&&frame[6]==3&&frame[7]==2&&frame[8]==1&&frame[9]==5&&frame[10]==0);
  assert(!memcmp(frame+11,"hello",5)&&frame[16]==0&&frame[211]==0);
  char big[300];memset(big,'x',sizeof(big));buildFrame(frame,Text,1,big,sizeof(big));
  assert(get16(frame+9)==TextMax&&frame[11+199]=='x'&&frame[211]==0);          // the terminator byte survives
  Frame f;assert(parseFrame(frame,FrameSize,f)&&f.type==Text&&f.counter==1&&f.textLength==200&&f.full());
  // Truncated and hostile frames: never read past what arrived.
  assert(!parseFrame(nullptr,212,f)&&!parseFrame(frame,4,f));
  assert(parseFrame(frame,5,f)&&!f.text&&f.textLength==0&&f.counter==0&&!f.full());
  assert(parseFrame(frame,9,f)&&f.counter==1&&!f.text);
  assert(parseFrame(frame,11,f)&&f.text&&f.textLength==0);
  assert(parseFrame(frame,20,f)&&f.textLength==9);                              // declared 200, only 9 arrived
  frame[9]=0xff;frame[10]=0xff;assert(parseFrame(frame,FrameSize,f)&&f.textLength==200);
  uint8_t other[FrameSize]={};memcpy(other,"ENOX",4);assert(!parseFrame(other,FrameSize,f));
  // Core families.
  auto kind=[](const std::string& text){Bytes b=textFrame(CoreReply,0,text);Frame r;parseFrame(b.data(),b.size(),r);return classifyReply(r);};
  assert(kind("")==Core::Jcmk&&kind("encrypt=0")==Core::Biscuit&&kind("v=2;encrypt=0;x")==Core::Biscuit);
  assert(kind("encrypt=1")==Core::BiscuitSecure&&kind("encrypt=")==Core::BiscuitSecure&&kind("PIGLET1\x02v3.08")==Core::Piglet);
  assert(kind("PIGLET")==Core::Jcmk&&kind("xPIGLET1")==Core::Jcmk);
  const uint8_t shortReply[]={'E','N','O','W',CoreReply};Frame r;assert(parseFrame(shortReply,5,r)&&classifyReply(r)==Core::Jcmk);
  // Record text. The core splits on commas, so a comma or line break in a name must not survive.
  const uint8_t mac[6]={0xde,0xad,0xbe,0xef,0x00,0x7f};char line[TextMax+1];
  size_t n=wifiLine(line,sizeof(line),mac,(const uint8_t*)"Cafe, upstairs",14,Auth::Wpa2,6,-61,false);
  assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,Cafe_ upstairs,[WPA2_PSK],6,-61,W");
  n=wifiLine(line,sizeof(line),mac,(const uint8_t*)"a\nb\rc\x7f" "d",7,Auth::Open,11,-90,true);
  assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,a_b_c_d,[OPEN],11,-90,W");
  n=wifiLine(line,sizeof(line),mac,nullptr,0,Auth::Wpa2Enterprise,1,-40,false);assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,,[WPA2],1,-40,W");
  n=wifiLine(line,sizeof(line),mac,(const uint8_t*)"",0,Auth::Wpa2Enterprise,1,-40,true);assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,,[WPA2_ENTERPRISE],1,-40,W");
  const uint8_t longName[40]={'L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','L','X','X','X','X','X','X','X','X'};
  n=wifiLine(line,sizeof(line),mac,longName,40,Auth::Wep,3,-1,false);assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL,[WEP],3,-1,W");
  n=wifiLine(line,sizeof(line),mac,(const uint8_t*)"ab\0cd",5,Auth::Wpa3,2,-5,false);assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,ab,[WPA3_PSK],2,-5,W");
  n=wifiLine(line,sizeof(line),mac,(const uint8_t*)"caf\xc3\xa9",5,Auth::WpaWpa2,2,-5,false);assert(std::string(line,n)=="DE:AD:BE:EF:00:7F,caf\xc3\xa9,[WPA_WPA2_PSK],2,-5,W");
  n=bleLine(line,sizeof(line),mac,-77);assert(std::string(line,n)=="de:ad:be:ef:00:7f,,[BLE],0,-77,B");
  char tiny[8];n=wifiLine(tiny,sizeof(tiny),mac,nullptr,0,Auth::Open,1,-1,false);assert(n==7&&tiny[7]==0);
  // Security names, family by family.
  const char* jcmk[]={"[OPEN]","[WEP]","[WPA_PSK]","[WPA2_PSK]","[WPA_WPA2_PSK]","[WPA2]","[WPA3_PSK]","[WPA2_WPA3_PSK]","[WAPI_PSK]","[UNDEFINED]"};
  const char* biscuit[]={"[OPEN]","[WEP]","[WPA_PSK]","[WPA2_PSK]","[WPA_WPA2_PSK]","[WPA2_ENTERPRISE]","[WPA3_PSK]","[WPA2_WPA3_PSK]","[UNDEFINED]","[UNDEFINED]"};
  for(unsigned i=0;i<10;++i){assert(!strcmp(authText(Auth(i),false),jcmk[i]));assert(!strcmp(authText(Auth(i),true),biscuit[i]));}
  assert(knownChannel(1)&&knownChannel(14)&&knownChannel(36)&&knownChannel(104)&&knownChannel(108)&&knownChannel(177));
  assert(!knownChannel(0)&&!knownChannel(15)&&!knownChannel(37)&&!knownChannel(181));
  assert(ChannelTable[0]==1&&ChannelTable[13]==14&&ChannelTable[14]==36&&ChannelTable[22]==100&&ChannelTable[23]==112&&ChannelTable[39]==177);
}

static void nodeJcmkTests(){
  using namespace nodetest;
  Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.alive=false;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
  // A table that is missing, too small or not a power of two is refused.
  {Bench bad(NodeFamily::Jcmk);bad.config.seen=nullptr;assert(!bad.begin());bad.config.seen=bad.table.data();bad.config.seenCount=100;assert(!bad.begin());bad.config.seenCount=4;assert(!bad.begin());}
  assert(b.begin()&&b.link.active()&&b.link.status().phase==Phase::Search&&b.channel==HomeChannel);
  // Search: full-size broadcast requests, first within 300 ms, then 300, 600, 1200, 2400, 4800, 5000 apart.
  b.run(20000);
  std::vector<uint32_t> at;for(auto& s:b.log){assert(s.data[4]==CoreRequest&&isBroadcast(s.to)&&s.data.size()==212&&get16(s.data.data()+9)==0);at.push_back(s.at);}
  assert(at.size()>=8&&at[0]-1000<=RequestFirstMs+3);
  const uint32_t gaps[]={300,600,1200,2400,4800,5000,5000};
  for(unsigned i=0;i<7;++i)assert(at[i+1]-at[i]==gaps[i]);
  assert(b.scanned.empty()&&b.bleStarts==0&&!b.link.status().linked);               // nothing to report to: only search
  // The core appears: reply, then the assignment it sends right behind it.
  for(uint8_t ch=1;ch<=11;++ch)for(unsigned i=0;i<3;++i)b.world[ch].push_back({bssid(ch*16+i),"Net "+std::to_string(ch)+"-"+std::to_string(i),i==0?Auth::Open:i==1?Auth::Wpa2:Auth::Wpa2Enterprise,ch,-40-int(ch)-int(i)});
  b.world[6].push_back({bssid(0x999),"Cafe, upstairs",Auth::Wpa3,6,-52});
  b.bleWorld.push_back({Mac{{0x4c,0x00,0x12,0xab,0xcd,0xef}},-70});
  b.log.clear();core.alive=true;
  assert(b.until([&]{return b.link.status().linked;},6000));
  assert(b.link.status().core==Core::Jcmk&&b.peers.at(core.mac)==Peer::Plain&&b.link.status().phase==Phase::Join);
  assert(b.until([&]{return b.link.status().cycles>=2;}));
  const Status& s=b.link.status();
  assert(s.assigned&&!s.ownPlan&&s.planCount==11&&s.plan[0]==1&&s.plan[10]==11&&s.nodeCount==1); // 1..14 offered, 1..11 reachable
  assert(s.wifi==34&&s.ble==1&&s.sent==35&&s.failed==0&&s.dropped==0);
  assert(core.records.size()==35);                                                   // each network told exactly once
  assert(hasLine(core.records,"AA:BB:CC:00:00:60,Net 6-0,[OPEN],6,-46,W"));
  assert(hasLine(core.records,"AA:BB:CC:00:00:B2,Net 11-2,[WPA2],11,-53,W"));
  assert(hasLine(core.records,"AA:BB:CC:00:09:99,Cafe_ upstairs,[WPA3_PSK],6,-52,W"));
  assert(hasLine(core.records,"4c:00:12:ab:cd:ef,,[BLE],0,-70,B"));
  for(unsigned i=0;i<11;++i)assert(b.scanned[i]==i+1);                               // channels in assigned order
  assert(b.bleStarts>=2&&core.heartbeats>=2&&core.counters[0]==0&&core.counters[1]==1);
  // Once the core is known every report goes straight to it and its radio answers.
  for(auto& sent:b.log)if(sent.data[4]==Text||sent.data[4]==Heartbeat)assert(sent.to==core.mac);
  assert(!s.broadcast&&b.count(Identify)==0&&b.count(RoleAck)==0&&b.count(ConfigAck)==0);
  // Text counters count up from 1 so a core can acknowledge each record.
  {uint32_t expect=1;for(auto& sent:b.log)if(sent.data[4]==Text){assert(get32(sent.data.data()+5)==expect);++expect;}}
  // A new assignment mid-run is followed. A repeated one changes nothing.
  core.version=2;core.first=5;core.last=7;core.dirty=true;
  assert(b.until([&]{return b.link.status().planCount==3;}));
  assert(s.plan[0]==6&&s.plan[1]==7&&s.plan[2]==8);
  b.run(400);b.scanned.clear();b.run(3000);assert(b.scanned.size()>=6);for(uint8_t c:b.scanned)assert(c>=6&&c<=8);
  // Only 5 GHz offered: nothing this radio can reach, so it covers all of 2.4 GHz itself.
  core.version=3;core.first=20;core.last=39;core.dirty=true;
  assert(b.until([&]{return b.link.status().ownPlan;}));assert(s.planCount==11&&s.assigned);
  // A nonsense range means "everything", as on the reference node.
  core.version=4;core.first=30;core.last=2;core.dirty=true;
  assert(b.until([&]{return !b.link.status().ownPlan;}));assert(s.planCount==11&&s.plan[0]==1);
  core.version=5;core.first=0;core.last=200;core.dirty=true;b.run(4000);assert(s.planCount==11&&!s.ownPlan);
  // An assignment that differs only in range, same version (a core that restarted), is still applied.
  core.first=0;core.last=0;core.dirty=true;assert(b.until([&]{return b.link.status().planCount==1;}));assert(s.plan[0]==1);
  // Frames from anyone but the linked core cannot steer the node.
  const Mac stranger{{2,2,2,2,2,2}};
  b.hear(stranger,adminFrame(9,0,1,3,3));b.hear(stranger,textFrame(ConfigUpdate,0,"channels=9"));b.hear(stranger,textFrame(Admin,0,"\x02"));
  b.run(700);assert(s.planCount==1&&s.plan[0]==1&&b.count(RoleAck)==0&&b.count(ConfigAck)==0);
  // Nothing new around: no repeats on the air, heartbeats continue.
  const size_t told=core.records.size();const unsigned beats=core.heartbeats;b.run(5000);
  assert(core.records.size()==told&&core.heartbeats>beats+2&&s.wifi==34);
  // The core goes away. After 30 s of silence the node searches again and scans nothing.
  core.alive=false;const uint32_t lostFrom=b.now;
  assert(b.until([&]{return !b.link.status().linked;},40000));
  assert(b.now-lostFrom>=CoreLostMs-3000&&b.now-lostFrom<=CoreLostMs+3000&&s.losses==1&&s.phase==Phase::Search&&!b.peers.count(core.mac));
  assert(s.failed==0);                                                               // nothing was waiting to be told
  b.scanned.clear();b.run(8000);assert(b.scanned.empty()&&!b.bleOn&&!b.scanning);
  // It comes back (restarted, so it knows nothing): everything is told again, nothing is counted twice.
  core.alive=true;core.dirty=true;core.known=false;core.version=1;core.first=0;core.last=13;core.records.clear();
  assert(b.until([&]{return b.link.status().linked;},7000));
  assert(b.until([&]{return core.records.size()>=35;}));b.run(3000);
  assert(core.records.size()==35&&s.wifi==34&&s.ble==1&&s.links==2);
  // Leaving node mode stops every radio activity and forgets the core.
  b.until([&]{return b.scanning;});b.link.end();
  assert(!b.link.active()&&!b.scanning&&!b.bleOn&&!b.peers.count(core.mac)&&s.phase==Phase::Off);
  const size_t quiet=b.log.size();b.run(2000);assert(b.log.size()==quiet);
}

static void nodeBiscuitTests(){
  using namespace nodetest;
  Bench b(NodeFamily::Biscuit);BiscuitCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
  for(uint8_t ch=1;ch<=11;++ch)b.world[ch].push_back({bssid(ch),"B"+std::to_string(ch),ch==6?Auth::Wpa2Enterprise:ch==11?Auth::Wapi:Auth::Wpa2,ch,-60});
  b.bleWorld.push_back({Mac{{1,2,3,4,5,6}},-50});
  assert(b.begin());
  assert(b.until([&]{return core.registered;},3000));
  // Identify, role acknowledgement and channel acknowledgement, each straight to the Manager.
  assert(core.identifies==1&&core.identity=="DragonDetector:0.14.8"&&core.roleEcho==1&&b.count(Identify)==1&&b.count(RoleAck)==1&&b.count(ConfigAck)==1);
  const Status& s=b.link.status();
  assert(s.linked&&s.core==Core::Biscuit&&s.assigned&&s.role==1&&s.planCount==3&&s.plan[0]==1&&s.plan[1]==6&&s.plan[2]==11&&s.dwellMs==120);
  assert(b.until([&]{return b.link.status().cycles>=3;}));
  assert(core.records.size()==3&&s.wifi==3&&s.ble==0&&b.bleStarts==0);               // Wi-Fi only towards a Biscuit
  assert(hasLine(core.records,"AA:BB:CC:00:00:06,B6,[WPA2_ENTERPRISE],6,-60,W")&&hasLine(core.records,"AA:BB:CC:00:00:0B,B11,[UNDEFINED],11,-60,W"));
  for(uint8_t c:b.scanned)assert(c==1||c==6||c==11);
  for(auto& sent:b.log)if(sent.data[4]!=CoreRequest)assert(sent.to==core.mac);
  assert(core.heartbeats>=3);
  // Slightly stronger: not repeated. Clearly stronger: told again so the Manager can place it better.
  b.world[6][0].rssi=-55;b.run(3000);assert(core.records.size()==3);
  b.world[6][0].rssi=-52;assert(b.until([&]{return core.records.size()==4;}));
  assert(core.records.back()=="AA:BB:CC:00:00:06,B6,[WPA2_ENTERPRISE],6,-52,W");b.run(3000);assert(core.records.size()==4&&s.wifi==3);
  // The Manager changes the plan: listed order is kept, channels this radio cannot reach are skipped.
  b.hear(core.mac,textFrame(ConfigUpdate,0,"channels=149,11,36,3,104,3;dwell=9000"));
  assert(b.until([&]{return b.link.status().planCount==2;}));
  assert(s.plan[0]==11&&s.plan[1]==3&&s.dwellMs==DwellMaxMs);
  assert(b.until([&]{return b.count(ConfigAck)==2;},2000));                          // acknowledged on the next visit home
  b.hear(core.mac,textFrame(ConfigUpdate,0,"channels=36,40,44;dwell=10"));           // nothing reachable
  assert(b.until([&]{return b.link.status().ownPlan;}));assert(s.planCount==11&&s.dwellMs==DwellMinMs&&s.assigned);
  // Malformed lists are discarded whole and still acknowledged.
  const char* bad[]={"channels=1,x,3","channels=1,,3","channels=1,2,","channels=15","channels=1 2","chan=1,2","channels=","channels=99999999999","","channels=1;2"};
  unsigned acked=b.count(ConfigAck);
  for(const char* text:bad){
    if(!strcmp(text,"channels=1;2"))continue;
    b.hear(core.mac,textFrame(ConfigUpdate,0,"channels=5"));b.until([&]{return b.link.status().planCount==1;});
    b.hear(core.mac,textFrame(ConfigUpdate,0,text));
    assert(b.until([&]{return b.link.status().planCount==11;}));
    assert(!s.assigned&&!s.ownPlan);acked+=2;
    b.until([&]{return b.count(ConfigAck)==acked;});assert(b.count(ConfigAck)==acked);
  }
  b.hear(core.mac,textFrame(ConfigUpdate,0,"channels=1;2"));assert(b.until([&]{return b.link.status().planCount==1;}));assert(s.assigned);
  // A short type-10 frame is not a Biscuit config at all.
  b.run(900);
  {Bytes cut=textFrame(ConfigUpdate,0,"channels=7");cut.resize(100);const unsigned before=b.count(ConfigAck);b.hear(core.mac,cut);b.run(900);assert(s.plan[0]==1&&b.count(ConfigAck)==before);}
  // Role byte missing: the counter's low byte is echoed instead.
  {const unsigned before=b.count(RoleAck);b.hear(core.mac,textFrame(Admin,0x1234,""));assert(b.until([&]{return b.count(RoleAck)==before+1;}));
   uint32_t echoed=0;for(auto& sent:b.log)if(sent.data[4]==RoleAck)echoed=get32(sent.data.data()+5);assert(echoed==0x34);}
  // The Manager switches to Secure mode: this node cannot follow, says so, and stops reporting.
  core.secure=true;b.hear(core.mac,textFrame(CoreReply,0,"encrypt=1"));
  assert(b.until([&]{return !b.link.status().linked;}));assert(s.other==Core::BiscuitSecure&&s.phase==Phase::Search);
  b.run(6000);assert(!s.linked&&s.other==Core::BiscuitSecure);                       // every answer renews the hint
  core.alive=false;b.run(OtherCoreMs+6000);assert(s.other==Core::None);              // and it fades once the core is gone
}

static void nodeFamilyTests(){
  using namespace nodetest;
  // BISCUIT NODE never joins a JCMK core, JCMK Node never joins a Biscuit. Each tells the user who answered.
  {Bench b(NodeFamily::Biscuit);JcmkCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());b.run(12000);const Status& s=b.link.status();
   assert(!s.linked&&s.other==Core::Jcmk&&core.requests>=5&&core.records.empty()&&b.scanned.empty()&&b.peers.empty());}
  {Bench b(NodeFamily::Jcmk);BiscuitCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());b.run(12000);const Status& s=b.link.status();
   assert(!s.linked&&s.other==Core::Biscuit&&core.identifies==0&&b.scanned.empty());}
  // A Piglet core signs its reply and sends Biscuit-style role and channel messages to full-size nodes.
  {Bench b(NodeFamily::Jcmk);const Mac piglet{{0x34,0x85,0x18,1,2,3}};std::vector<std::string> records;bool roleAck=false,configAck=false;
   b.air=[&](const Mac& to,const Bytes& d){
     if(!isBroadcast(to)&&to!=piglet)return false;
     Frame f;parseFrame(d.data(),d.size(),f);
     if(f.type==CoreRequest){b.hear(piglet,textFrame(CoreReply,0,std::string("PIGLET1\x01v3.08\0\0\0",16)));b.hear(piglet,textFrame(Admin,0,"\x01"),20);b.hear(piglet,textFrame(ConfigUpdate,0,"channels=1,2,3,4,5,6,7;dwell=80"),30);}
     if(f.type==RoleAck)roleAck=true;
     if(f.type==ConfigAck)configAck=true;
     if(f.type==Text)records.push_back(std::string((const char*)f.text,f.textLength));
     return true;};
   b.world[7].push_back({bssid(7),"seven",Auth::Wpa2Enterprise,7,-33});b.world[8].push_back({bssid(8),"eight",Auth::Open,8,-33});
   assert(b.begin());assert(b.until([&]{return b.link.status().cycles>=2;}));const Status& s=b.link.status();
   assert(s.core==Core::Piglet&&roleAck&&configAck&&s.planCount==7&&s.plan[6]==7&&b.count(Identify)==0);
   assert(records.size()==1&&records[0]=="AA:BB:CC:00:00:07,seven,[WPA2],7,-33,W");}   // JCMK wording, channel 8 not assigned
  // Bound to one core: requests go to it alone and other cores are not joined.
  {Bench b(NodeFamily::Jcmk);JcmkCore wanted,loud;wanted.bench=loud.bench=&b;loud.mac=Mac{{9,9,9,9,9,9}};wanted.alive=false;
   b.config.bound=true;memcpy(b.config.boundCore,wanted.mac.data(),6);
   b.air=[&](const Mac& to,const Bytes& d){const bool a=wanted(to,d);const bool c=loud(to,d);return a||c;};
   assert(b.begin());assert(b.peers.at(wanted.mac)==Peer::Plain);
   b.hear(loud.mac,textFrame(CoreReply,0,""));b.run(9000);
   assert(!b.link.status().linked&&loud.requests==0);for(auto& sent:b.log)assert(sent.to==wanted.mac);
   wanted.alive=true;assert(b.until([&]{return b.link.status().linked;},7000));assert(sameMac(b.link.status().coreMac,wanted.mac.data()));
   wanted.alive=false;assert(b.until([&]{return !b.link.status().linked;},40000));assert(b.peers.at(wanted.mac)==Peer::Plain);
   b.link.end();assert(b.peers.empty());}
  // With a key the core is an encrypted peer, and reports never fall back to broadcast.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.acksUnicast=false;b.config.encrypted=true;
   b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};b.world[1].push_back({bssid(1),"one",Auth::Open,1,-30});
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));assert(b.peers.at(core.mac)==Peer::Encrypted);
   b.run(9000);assert(!b.link.status().broadcast);for(auto& sent:b.log)if(sent.data[4]==Text||sent.data[4]==Heartbeat)assert(sent.to==core.mac);}
  {Bench b(NodeFamily::Biscuit);b.config.encrypted=true;BiscuitCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));assert(b.peers.at(core.mac)==Peer::Plain);} // keys are a JCMK feature
  // A peer the radio refuses means no link, and the search simply continues.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;b.rejectPeer=true;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());b.run(5000);assert(!b.link.status().linked&&b.link.status().links==0);b.rejectPeer=false;assert(b.until([&]{return b.link.status().linked;},7000));}
}

static void nodeDeliveryTests(){
  using namespace nodetest;
  // A core whose radio never acknowledges unicasts still gets everything by broadcast,
  // and stays linked for as long as it answers a request now and then.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.acksUnicast=false;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   for(uint8_t ch=1;ch<=11;++ch)b.world[ch].push_back({bssid(ch),"n",Auth::Open,ch,-60});
   assert(b.begin());assert(b.until([&]{return b.link.status().broadcast;},9000));
   auto different=[&]{std::map<std::string,unsigned> seen;for(auto& r:core.records)++seen[r];return seen.size();};
   assert(b.until([&]{return different()==11;}));const Status& s=b.link.status();
   b.run(5000);const size_t settled=core.records.size();b.run(5000);assert(core.records.size()==settled); // and then no repeats
   const unsigned requests=core.requests;b.run(45000);
   assert(s.linked&&s.losses==0&&core.requests>=requests+3&&core.requests<=requests+6);   // one probe per 10 s of silence
   assert(s.failed<=6);                                                                  // only the first tries, before it switched
   core.alive=false;assert(b.until([&]{return !b.link.status().linked;},40000));}
  // Record acknowledgements from a core prove it is alive and are counted.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.acks=true;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   for(uint8_t ch=1;ch<=11;++ch)b.world[ch].push_back({bssid(ch),"n",Auth::Open,ch,-60});
   assert(b.begin());assert(b.until([&]{return b.link.status().cycles>=3;}));const Status& s=b.link.status();
   assert(s.acknowledged&&s.acked==11&&s.sent==11);}
  // One lost frame: the record is offered again and arrives. Nothing is lost, nothing doubled.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;unsigned texts=0;
   b.air=[&](const Mac& to,const Bytes& d){if(d[4]==Text&&++texts==2)return false;return core(to,d);};
   for(uint8_t ch=1;ch<=3;++ch)b.world[ch].push_back({bssid(ch),"n",Auth::Open,ch,-60});
   assert(b.begin());assert(b.until([&]{return b.link.status().cycles>=3;}));const Status& s=b.link.status();
   assert(core.records.size()==3&&s.sent==3&&s.failed==0&&b.count(Text)==4);}
  // The core is out of reach for a while: after three tries a record is given up,
  // and told again the next time that network is seen.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;bool deaf=false;
   b.air=[&](const Mac& to,const Bytes& d){if(deaf)return false;return core(to,d);};
   b.world[4].push_back({bssid(4),"four",Auth::Open,4,-60});
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));b.until([&]{return b.link.status().phase==Phase::Scan;});deaf=true;
   assert(b.until([&]{return b.link.status().failed==1;}));assert(core.records.empty()&&b.count(Text)==SendTries);
   deaf=false;assert(b.until([&]{return core.records.size()==1;}));assert(b.link.status().wifi==1&&b.link.status().linked);}
  // The radio refuses to send at all (driver busy): no evidence against the core, record retried then dropped.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   b.world[2].push_back({bssid(2),"two",Auth::Open,2,-60});
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));b.until([&]{return b.link.status().phase==Phase::Scan;});b.rejectTransmit=true;
   assert(b.until([&]{return b.link.status().failed>=1;}));assert(!b.link.status().broadcast);
   b.rejectTransmit=false;assert(b.until([&]{return core.records.size()==1;}));}
  // A send report that never arrives does not stall the node.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));
   b.until([&]{return b.inFlight;});b.inFlight=false;const uint32_t cycles=b.link.status().cycles;
   assert(b.until([&]{return b.link.status().cycles>=cycles+2;}));}
  // More networks on one channel than the queue holds: the rest follow on later passes.
  {Bench b(NodeFamily::Jcmk,1024);JcmkCore core;core.bench=&b;core.first=0;core.last=0;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   for(unsigned i=0;i<150;++i)b.world[1].push_back({bssid(0x1000+i),"crowd"+std::to_string(i),Auth::Wpa2,1,-70});
   assert(b.begin());assert(b.until([&]{return core.records.size()>=150;},60000));b.run(3000);const Status& s=b.link.status();
   std::map<std::string,unsigned> seen;for(auto& r:core.records)++seen[r];
   assert(seen.size()==150&&core.records.size()==150&&s.wifi==150&&s.dropped>0&&s.sent==150);}
  // A full table replaces old entries instead of failing; counting continues.
  {Bench b(NodeFamily::Jcmk,8);JcmkCore core;core.bench=&b;core.first=0;core.last=0;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   for(unsigned i=0;i<40;++i)b.world[1].push_back({bssid(0x2000+i),"t",Auth::Open,1,-70});
   assert(b.begin());b.run(8000);assert(b.link.status().wifi>=40&&core.records.size()>=40&&b.link.status().linked);}
  // Scans that fail to start, or never finish, are counted and the cycle still completes.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));b.rejectScan=true;const uint32_t cycles=b.link.status().cycles;
   assert(b.until([&]{return b.link.status().cycles>=cycles+2;}));assert(b.link.status().scanErrors>=11&&b.link.status().linked);
   b.rejectScan=false;b.loseScans=true;const uint32_t errors=b.link.status().scanErrors;
   assert(b.until([&]{return b.link.status().scanErrors>=errors+2;},9000));assert(b.scanStops>=2&&b.link.status().linked);
   b.loseScans=false;b.scanning=false;const uint32_t scans=b.link.status().scans;assert(b.until([&]{return b.link.status().scans>=scans+11;}));}
  // The millisecond clock wrapping around mid-session changes nothing.
  {Bench b(NodeFamily::Jcmk,256,0xffffd000u);JcmkCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   b.world[9].push_back({bssid(9),"wrap",Auth::Open,9,-60});
   assert(b.begin());b.run(30000);const Status& s=b.link.status();
   assert(b.now<0x10000&&s.linked&&s.losses==0&&s.links==1&&core.records.size()==1&&s.cycles>10);}
  {Bench b(NodeFamily::Biscuit,256,0xfffff800u);BiscuitCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());b.run(20000);assert(b.link.status().linked&&b.link.status().losses==0&&core.registered);}
}

static void nodeHostileTests(){
  using namespace nodetest;
  // Anything can arrive on an open radio. Random and truncated frames, from the
  // core's address and from strangers, in every phase, must leave the node sane.
  for(NodeFamily family:{NodeFamily::Jcmk,NodeFamily::Biscuit}){
    Bench b(family,64);JcmkCore jcmk;BiscuitCore biscuit;jcmk.bench=biscuit.bench=&b;
    const Mac core=family==NodeFamily::Jcmk?jcmk.mac:biscuit.mac;
    b.air=[&](const Mac& to,const Bytes& d){return family==NodeFamily::Jcmk?jcmk(to,d):biscuit(to,d);};
    for(uint8_t ch=1;ch<=11;++ch)b.world[ch].push_back({bssid(ch),"n",Auth::Open,ch,-60});
    assert(b.begin());
    uint32_t seed=12345;auto next=[&]{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    for(unsigned round=0;round<6000;++round){
      Bytes junk(next()%260);for(auto& byte:junk)byte=uint8_t(next());
      if(junk.size()>=5&&next()%4){memcpy(junk.data(),"ENOW",4);const uint8_t types[]={1,2,3,4,5,6,7,10,11,15,0,255};junk[4]=types[next()%12];}
      // Exact-size heap copies so the sanitizer sees any read past the end.
      uint8_t* exact=junk.empty()?nullptr:new uint8_t[junk.size()];if(exact)memcpy(exact,junk.data(),junk.size());
      const Mac from=next()%2?core:Mac{{uint8_t(next()),2,3,4,5,6}};
      b.link.received(from.data(),exact,junk.size(),int(next()%200)-150,b.now);delete[] exact;
      b.run(next()%7);
      const Status& s=b.link.status();
      assert(s.planCount>=1&&s.planCount<=11);for(uint8_t i=0;i<s.planCount;++i)assert(s.plan[i]>=1&&s.plan[i]<=11);
      assert(s.dwellMs>=DwellMinMs&&s.dwellMs<=DwellMaxMs&&s.channel>=1&&s.channel<=11);
    }
    b.link.received(nullptr,nullptr,0,0,b.now);b.link.received(core.data(),nullptr,50,0,b.now);
    b.link.wifi(nullptr,nullptr,0,Auth::Open,1,0);b.link.ble(nullptr,0);
    const uint8_t zero[6]={};b.link.wifi(zero,nullptr,0,Auth::Open,1,-10);          // an all-zero address is not a network
    // After the storm a clean core is still joined and served.
    b.inbox.clear();assert(b.until([&]{return b.link.status().linked;},45000));
    const uint32_t cycles=b.link.status().cycles;assert(b.until([&]{return b.link.status().cycles>=cycles+2;},20000));
  }
  // Calls before begin and after end are ignored.
  {Bench b(NodeFamily::Jcmk);const uint8_t mac[6]={1,2,3,4,5,6};
   b.link.loop(5);b.link.sent(true,5);b.link.scanned(true,5);b.link.wifi(mac,nullptr,0,Auth::Open,1,-1);b.link.ble(mac,-1);b.link.end();
   Bytes reply=textFrame(CoreReply,0,"");b.link.received(mac,reply.data(),reply.size(),-1,5);
   assert(!b.link.active()&&b.log.empty()&&b.peers.empty()&&b.link.status().wifi==0);}
}

static void nodeRepeatTests(){
  using namespace nodetest;
  // A network is told once, again when clearly stronger, and again after about a minute in view.
  for(NodeFamily family:{NodeFamily::Jcmk,NodeFamily::Biscuit}){
    Bench b(family);JcmkCore jcmk;BiscuitCore biscuit;jcmk.bench=&b;biscuit.bench=&b;
    std::vector<std::string>& records=family==NodeFamily::Jcmk?jcmk.records:biscuit.records;
    b.air=[&](const Mac& to,const Bytes& d){return family==NodeFamily::Jcmk?jcmk(to,d):biscuit(to,d);};
    b.world[1].push_back({bssid(1),"near",Auth::Wpa2,1,-70});b.world[6].push_back({bssid(2),"steady",Auth::Open,6,-60});
    if(family==NodeFamily::Jcmk)b.bleWorld.push_back({Mac{{0x10,0x20,0x30,0x40,0x50,0x60}},-80});
    const unsigned first=family==NodeFamily::Jcmk?3:2;
    assert(b.begin());assert(b.until([&]{return records.size()==first;}));const uint32_t told=b.now;
    b.world[1][0].rssi=-63;b.run(20000);assert(records.size()==first);                // 7 dB: not worth a message
    b.world[1][0].rssi=-62;assert(b.until([&]{return records.size()==first+1;},5000)); // 8 dB: told again
    assert(records.back().find(",-62,W")!=std::string::npos);
    b.world[1][0].rssi=-75;b.run(5000);assert(records.size()==first+1);               // weaker: nothing new to say
    while(b.now-told<55000)b.step();
    assert(countLine(records,"AA:BB:CC:00:00:02,steady,[OPEN],6,-60,W")==1);          // not yet a minute
    assert(b.until([&]{return countLine(records,"AA:BB:CC:00:00:02,steady,[OPEN],6,-60,W")==2;},20000));
    if(family==NodeFamily::Jcmk)assert(b.until([&]{return countLine(records,"10:20:30:40:50:60,,[BLE],0,-80,B")==2;},20000));
    const Status& s=b.link.status();
    assert(s.wifi==2&&s.ble==(family==NodeFamily::Jcmk?1u:0u)&&s.failed==0&&s.dropped==0); // repeats never count as new networks
    // After the refresh the weaker reading is the baseline, so the next approach is reported again.
    assert(b.until([&]{return countLine(records,"AA:BB:CC:00:00:01,near,[WPA2_PSK],1,-75,W")==1;},70000));
    b.world[1][0].rssi=-66;assert(b.until([&]{return countLine(records,"AA:BB:CC:00:00:01,near,[WPA2_PSK],1,-66,W")==1;},5000));
  }
  // The report clock survives the 49-day millisecond rollover.
  {Bench b(NodeFamily::Jcmk,256,0xffffffffu-30000);JcmkCore core;core.bench=&b;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   b.world[3].push_back({bssid(3),"wrap",Auth::Open,3,-50});
   assert(b.begin());assert(b.until([&]{return core.records.size()==1;}));b.run(50000);assert(core.records.size()==1);
   assert(b.until([&]{return core.records.size()==2;},25000));b.run(40000);assert(core.records.size()==2);}
}

static void nodeKeyedTests(){
  using namespace nodetest;
  // JCMK "Use Encryption". The radio acknowledges a sealed frame whether or not the
  // core can read it, so the link is only kept alive by answers from the core itself.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.keyed=true;b.config.encrypted=true;b.config.ble=false;
   b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};b.world[1].push_back({bssid(1),"one",Auth::Open,1,-30});
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));const Status& s=b.link.status();
   assert(b.keyed(core.mac));assert(b.until([&]{return s.assigned;},1000));           // the sealed assignment right behind the answer was readable
   assert(b.until([&]{return core.records.size()==1;}));
   const unsigned asked=core.requests;b.run(45000);
   assert(s.linked&&s.links==1&&s.losses==0&&core.unread==0);
   assert(core.requests>=asked+3&&core.requests<=asked+5);                            // asks again about every ten seconds
   assert(b.keyed(core.mac));
   // The core restarts and forgets the key. The next question puts it back; what was
   // said into the gap is said again at the next refresh.
   core.nodeKeyed=false;core.known=false;core.dirty=true;core.version=2;core.first=0;core.last=0;
   b.world[1].push_back({bssid(9),"gap",Auth::Wpa2,1,-40});
   assert(b.until([&]{return core.unread>0;},12000));
   assert(b.until([&]{return core.nodeKeyed;},12000));assert(s.linked&&s.losses==0);
   assert(b.until([&]{return s.planCount==1&&s.plan[0]==1;},1000));
   assert(b.until([&]{return hasLine(core.records,"AA:BB:CC:00:00:09,gap,[WPA2_PSK],1,-40,W");},80000));
   // An answer that is lost: the key still goes back on before anything else is sent.
   core.dropReplies=1;const unsigned before=core.requests;assert(b.until([&]{return core.requests>before;},15000));
   b.run(3000);assert(b.keyed(core.mac)&&s.linked);
   // A core whose radio still acknowledges but whose firmware no longer answers is lost.
   core.mute=true;const uint32_t at=b.now;assert(b.until([&]{return !s.linked;},45000));
   assert(b.now-at>=10000&&b.now-at<=CoreLostMs+2000&&s.losses==1&&!b.peers.count(core.mac)); // thirty seconds after its last answer
   core.mute=false;assert(b.until([&]{return s.linked;},7000));assert(b.keyed(core.mac)&&s.links==2);}
  // The same silent core on an open link stays linked: there the acknowledgement is proof enough.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;b.config.ble=false;b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));const unsigned asked=core.requests;
   b.run(45000);assert(b.link.status().linked&&b.link.status().losses==0&&core.requests==asked);}
  // Bound to one core and keyed: the question goes to that core alone, in the clear.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.keyed=true;b.config.encrypted=true;b.config.bound=true;memcpy(b.config.boundCore,core.mac.data(),6);
   b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));b.run(40000);
   assert(b.link.status().linked&&b.link.status().losses==0&&core.unread==0);
   for(auto& sent:b.log)assert(sent.to==core.mac);}
  // The radio refuses to take the key back: the link is dropped instead of talking in the clear.
  {Bench b(NodeFamily::Jcmk);JcmkCore core;core.bench=&b;core.keyed=true;b.config.encrypted=true;b.config.ble=false;
   b.air=[&](const Mac& to,const Bytes& d){if(d[4]==CoreRequest&&b.link.status().linked)b.rejectPeer=true;return core(to,d);};
   assert(b.begin());assert(b.until([&]{return b.link.status().linked;}));
   assert(b.until([&]{return b.link.status().losses==1;},20000));assert(!b.link.status().linked);
   b.air=[&](const Mac& to,const Bytes& d){return core(to,d);};b.rejectPeer=false;
   assert(b.until([&]{return b.link.status().linked;},9000));assert(b.keyed(core.mac));}
}

static void nodeLinkTests(){
  nodeWireTests();nodeJcmkTests();nodeBiscuitTests();nodeFamilyTests();nodeDeliveryTests();nodeHostileTests();nodeRepeatTests();nodeKeyedTests();
  puts("PASS wardriving node link: wire format, JCMK and Biscuit handshakes, channel plans, repeats, keyed links, delivery, loss, rollover and hostile frames");
}
