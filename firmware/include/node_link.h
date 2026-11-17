#pragma once
// Wardriving node link.
//
// Lets a Dragon Detector join a wardriving core as a scan-only node over the
// ESP-NOW "ENOW" protocol. Two core families are spoken:
//   JCMK     JustCallMeKoko's ESP32DualBandWardriver core. The message layout,
//            numbering, channel table and timing follow that MIT project.
//   Biscuit  The Biscuit Manager's unencrypted ("Extended") node dialect.
// This is an original Dragon Detector implementation. The Biscuit dialect and
// Piglet core detection were learned from Piglet's published source (Hamspiced,
// CC BY-NC-SA 4.0) and reimplemented here from those protocol facts alone; no
// Piglet code or text is included. See CREDITS.md and docs/NODE-MODES.md.
//
// The file is hardware free. The radio is reached through Port, so the whole
// link is exercised by the native tests against simulated cores.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dragon::node {

constexpr uint8_t HomeChannel=6;            // every core listens here
constexpr size_t TextMax=200;               // text bytes in one message
constexpr size_t FrameSize=212;             // magic[4] type counter[4] length[2] text[201]
constexpr size_t AdminSize=10;              // magic[4] type version index count first last
constexpr uint8_t BroadcastMac[6]={0xff,0xff,0xff,0xff,0xff,0xff};
// The core hands out index ranges into this table. 104 and 108 are absent on purpose.
constexpr uint8_t ChannelTable[]={
  1,2,3,4,5,6,7,8,9,10,11,12,13,14,
  36,40,44,48,52,56,60,64,
  100,112,116,120,124,128,132,136,140,144,
  149,153,157,161,165,169,173,177};
constexpr size_t ChannelCount=sizeof(ChannelTable);

enum Message : uint8_t {
  CoreRequest=1,CoreReply=2,Heartbeat=3,Text=4,Admin=5,RecordAck=6,
  // Biscuit Extended mode additions. Type 5 doubles as its role assignment.
  Identify=7,ConfigUpdate=10,ConfigAck=11,RoleAck=15};

constexpr uint32_t RequestFirstMs=300;      // core search backoff, doubling
constexpr uint32_t RequestMaxMs=5000;
constexpr uint32_t JcmkWindowMs=300;        // home-channel wait for an assignment
constexpr uint32_t BiscuitWindowMs=500;
constexpr uint32_t BiscuitWindowJitterMs=200;
constexpr uint32_t CoreLostMs=30000;        // silence before searching again
constexpr uint32_t ProbeAfterMs=10000;      // silence before asking the core to answer
constexpr uint32_t SendTimeoutMs=300;       // a send report that never arrives
constexpr uint32_t SettleMs=5;              // after returning to the home channel
constexpr uint32_t AckLingerMs=20;          // stay for a core's record acknowledgements
constexpr uint32_t ReportBudgetMs=400;      // longest record burst before scanning on
constexpr uint32_t BleScanMs=500;
constexpr uint32_t BleGuardMs=60;
constexpr uint32_t ScanGraceMs=1500;        // scan result that never arrives
constexpr uint32_t ScanRetryMs=100;
constexpr uint32_t OtherCoreMs=15000;       // how long a "wrong core" hint stays
constexpr uint16_t DwellDefaultMs=80,DwellMinMs=40,DwellMaxMs=300;
constexpr uint8_t SendTries=3;              // attempts per record on a unicast link
constexpr uint8_t UnicastGiveUp=5;          // unanswered unicasts before broadcasting
constexpr uint8_t AckProbes=3;              // reports that wait to learn whether a core acknowledges
constexpr int RssiStep=8;                   // report a network again when this much stronger
constexpr uint8_t RefreshUnits=15;          // and again after about a minute in view (4.096 s units)
constexpr uint32_t IdentifyRetryMs=1500;    // a Biscuit that gave no role is asked again at the next cycle end (about every 2 s)
constexpr uint8_t IdentifyTries=5;          // Identify attempts per registration
// Text of every request and heartbeat, in both families: the JCMK core's own
// opt-out (WiFiOps.h NODE_CAPABILITY_EXTERNAL_6G, MIT). That core then leaves
// this 2.4 GHz-only node out of its channel split and sends it no assignment,
// so joining, or only searching nearby, takes no channels from the rig's
// dual-band nodes. It compares the length exactly. A Biscuit ignores this
// text and a Piglet core reads only its own marker there.
constexpr char ExternalCapability[]="C5P1;ROLE=EXTERNAL;BANDS=6";
constexpr size_t ExternalCapabilityLength=sizeof(ExternalCapability)-1;
static_assert(ExternalCapabilityLength==26,"the core checks for exactly 26 bytes");

enum class Family : uint8_t {Biscuit,Jcmk};
enum class Core : uint8_t {None,Jcmk,Piglet,Biscuit,BiscuitSecure};
enum class Phase : uint8_t {Off,Search,Join,Scan,Ble,Report,Listen};
enum class Kind : uint8_t {Wifi,Ble};
enum class Auth : uint8_t {Open,Wep,Wpa,Wpa2,WpaWpa2,Wpa2Enterprise,Wpa3,Wpa2Wpa3,Wapi,Other};
enum class Peer : uint8_t {Remove,Plain,Encrypted};

// What the link needs from the hardware. Every call returns immediately.
struct Port {
  virtual bool tune(uint8_t channel)=0;
  virtual bool peer(const uint8_t* mac,Peer mode)=0;
  // The outcome arrives later through Link::sent(). false means it never left.
  virtual bool transmit(const uint8_t* destination,const uint8_t* frame,size_t length)=0;
  // Active scan of one channel. Results arrive through Link::wifi() and then Link::scanned().
  virtual bool scan(uint8_t channel,uint16_t dwellMs)=0;
  virtual void scanStop()=0;
  virtual void ble(bool on,uint32_t durationMs)=0;
protected:
  ~Port()=default;
};

inline uint16_t get16(const uint8_t* p){return uint16_t(p[0]|p[1]<<8);}
inline uint32_t get32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline bool sameMac(const uint8_t* a,const uint8_t* b){return !memcmp(a,b,6);}

// Text messages are always sent full size: a Biscuit drops anything shorter.
inline size_t buildFrame(uint8_t* out,uint8_t type,uint32_t counter,const void* text,size_t length){
  memset(out,0,FrameSize);memcpy(out,"ENOW",4);out[4]=type;
  out[5]=uint8_t(counter);out[6]=uint8_t(counter>>8);out[7]=uint8_t(counter>>16);out[8]=uint8_t(counter>>24);
  if(length>TextMax)length=TextMax;
  out[9]=uint8_t(length);out[10]=uint8_t(length>>8);
  if(length&&text)memcpy(out+11,text,length);
  return FrameSize;
}

struct Frame {
  uint8_t type=0;uint32_t counter=0;const uint8_t* text=nullptr;size_t textLength=0,length=0;
  bool full() const {return length>=FrameSize;}
};
// Accepts any length from the 5-byte header up and never reads past the buffer.
inline bool parseFrame(const uint8_t* data,size_t length,Frame& frame){
  if(!data||length<5||memcmp(data,"ENOW",4))return false;
  frame=Frame{};frame.type=data[4];frame.length=length;
  if(length>=9)frame.counter=get32(data+5);
  if(length>=11){
    size_t declared=get16(data+9);const size_t available=length-11;
    if(declared>available)declared=available;
    if(declared>TextMax)declared=TextMax;
    frame.text=data+11;frame.textLength=declared;
  }
  return true;
}
inline bool findText(const uint8_t* text,size_t length,const char* needle,size_t& at){
  const size_t n=strlen(needle);
  for(size_t i=0;text&&i+n<=length;++i)if(!memcmp(text+i,needle,n)){at=i;return true;}
  return false;
}
// A Biscuit announces its mode in the reply text. A Piglet core signs its reply.
inline Core classifyReply(const Frame& reply){
  size_t at=0;
  if(findText(reply.text,reply.textLength,"encrypt=",at))
    return at+8<reply.textLength&&reply.text[at+8]=='0'?Core::Biscuit:Core::BiscuitSecure;
  if(reply.textLength>=7&&!memcmp(reply.text,"PIGLET1",7))return Core::Piglet;
  return Core::Jcmk;
}
inline const char* coreName(Core core){
  switch(core){
    case Core::Jcmk:return "JCMK";case Core::Piglet:return "PIGLET";case Core::Biscuit:return "BISCUIT";
    case Core::BiscuitSecure:return "BISCUIT SECURE";default:return "NONE";}
}
inline const char* phaseName(Phase phase){
  switch(phase){
    case Phase::Search:return "search";case Phase::Join:return "join";case Phase::Scan:return "scan";
    case Phase::Ble:return "ble";case Phase::Report:return "report";case Phase::Listen:return "listen";
    default:return "off";}
}
// Each family writes security the way its own nodes do.
inline const char* authText(Auth auth,bool biscuit){
  switch(auth){
    case Auth::Open:return "[OPEN]";case Auth::Wep:return "[WEP]";case Auth::Wpa:return "[WPA_PSK]";
    case Auth::Wpa2:return "[WPA2_PSK]";case Auth::WpaWpa2:return "[WPA_WPA2_PSK]";
    case Auth::Wpa2Enterprise:return biscuit?"[WPA2_ENTERPRISE]":"[WPA2]";
    case Auth::Wpa3:return "[WPA3_PSK]";case Auth::Wpa2Wpa3:return "[WPA2_WPA3_PSK]";
    case Auth::Wapi:return biscuit?"[UNDEFINED]":"[WAPI_PSK]";
    default:return "[UNDEFINED]";}
}
inline bool knownChannel(unsigned channel){
  if(channel==104||channel==108)return true;
  for(uint8_t known:ChannelTable)if(known==channel)return true;
  return false;
}
// BSSID,SSID,SECURITY,CHANNEL,RSSI,W  The core adds time and position itself.
inline size_t wifiLine(char* out,size_t size,const uint8_t* bssid,const uint8_t* ssid,size_t ssidLength,
    Auth auth,unsigned channel,int rssi,bool biscuit){
  char name[33];size_t n=0;
  if(ssidLength>32)ssidLength=32;
  for(size_t i=0;ssid&&i<ssidLength&&ssid[i];++i){
    const uint8_t c=ssid[i];name[n++]=(c==','||c<0x20||c==0x7f)?'_':char(c); // one record, one line
  }
  name[n]=0;
  const int written=snprintf(out,size,"%02X:%02X:%02X:%02X:%02X:%02X,%s,%s,%u,%d,W",
    bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5],name,authText(auth,biscuit),channel,rssi);
  return written<0?0:size_t(written)<size?size_t(written):size-1;
}
// address,,[BLE],0,RSSI,B
inline size_t bleLine(char* out,size_t size,const uint8_t* address,int rssi){
  const int written=snprintf(out,size,"%02x:%02x:%02x:%02x:%02x:%02x,,[BLE],0,%d,B",
    address[0],address[1],address[2],address[3],address[4],address[5],rssi);
  return written<0?0:size_t(written)<size?size_t(written):size-1;
}

// One remembered network or device. The table is supplied by the caller.
// flags: SeenUsed, SeenBle, SeenReported, then five bits of report time.
struct Seen {uint8_t mac[6];int8_t rssi;uint8_t flags;};
constexpr uint8_t SeenUsed=1,SeenBle=2,SeenReported=4,SeenStampShift=3,SeenStampMask=31;

struct Config {
  Family family=Family::Jcmk;
  const char* identity="";          // shown by a Biscuit Manager, "Name:version"
  uint8_t maxChannel=11;            // highest channel this radio is allowed to scan
  bool ble=true;                    // JCMK family: also report Bluetooth devices (to a JCMK core only)
  bool encrypted=false;             // JCMK family: an ESP-NOW key is installed (a Piglet core is then not joined)
  bool bound=false;                 // only this core may be joined
  uint8_t boundCore[6]={};
  uint32_t seed=1;
  Seen* seen=nullptr;               // zeroed table, power-of-two entries
  size_t seenCount=0;
};

struct Status {
  Phase phase=Phase::Off;
  Core core=Core::None;             // family of the linked core
  Core other=Core::None;            // a core that answered but cannot be joined in this mode
  bool linked=false,assigned=false,ownPlan=false,broadcast=false,acknowledged=false;
  bool registered=false;            // JCMK, Piglet: while linked. Biscuit: it gave a role, and answers Identify with one
  uint8_t coreMac[6]={};
  int8_t coreRssi=0;
  uint8_t channel=HomeChannel,role=0,nodeIndex=0,nodeCount=0;
  uint8_t plan[14]={},planCount=0;
  uint16_t dwellMs=DwellDefaultMs;
  uint32_t wifi=0,ble=0;            // different networks and devices found
  uint32_t sent=0,failed=0,dropped=0,acked=0;
  uint32_t cycles=0,scans=0,scanErrors=0,requests=0,links=0,losses=0,heartbeats=0;
  uint32_t coreAgeMs=0;
};

class Link {
public:
  static constexpr size_t QueueSize=64,ControlSize=4,PlanMax=14,ProbeDepth=16;

  bool begin(Port& port,const Config& config,uint32_t now){
    // Any power of two that a 16-bit slot number can reach.
    if(!config.seen||!config.seenCount||config.seenCount>65536||(config.seenCount&(config.seenCount-1)))return false;
    port_=&port;config_=config;status_=Status{};now_=now;
    snprintf(identity_,sizeof(identity_),"%s",config.identity?config.identity:"");config_.identity=identity_;
    if(config_.maxChannel<1)config_.maxChannel=1;
    if(config_.maxChannel>14)config_.maxChannel=14;
    if(config_.family!=Family::Jcmk)config_.encrypted=false;
    memset(config_.seen,0,config_.seenCount*sizeof(Seen));
    random_=config.seed?config.seed:0x9e3779b9u;
    textCounter_=heartbeatCounter_=0;evictions_=0;identifyTries_=0;roleWanted_=false;
    queueHead_=queueCount_=controlCount_=0;flight_=Flight::None;scanning_=scanFailed_=rekey_=false;
    resetAssignment();
    if(config_.bound)port_->peer(config_.boundCore,Peer::Plain);
    search(now,random()%RequestFirstMs);     // nodes powered together do not shout together
    return true;
  }
  void end(){
    if(status_.phase==Phase::Off)return;
    if(scanning_)port_->scanStop();
    if(status_.phase==Phase::Ble)port_->ble(false,0);
    if(status_.linked)port_->peer(core_,Peer::Remove);
    else if(config_.bound)port_->peer(config_.boundCore,Peer::Remove);
    scanning_=false;flight_=Flight::None;status_.linked=status_.registered=false;status_.core=Core::None;
    status_.phase=Phase::Off;status_.channel=HomeChannel;
  }
  bool active() const {return status_.phase!=Phase::Off;}
  bool bleWindow() const {return status_.phase==Phase::Ble;}
  const Status& status() const {return status_;}

  void loop(uint32_t now){
    if(status_.phase==Phase::Off)return;
    now_=now;
    if(status_.other!=Core::None&&now-otherAt_>=OtherCoreMs)status_.other=Core::None;
    status_.coreAgeMs=status_.linked?now-lastCoreAt_:0;
    if(status_.linked&&status_.coreAgeMs>=CoreLostMs){++status_.losses;unlink(now);return;}
    switch(status_.phase){
      case Phase::Search:
        if(pump(now,false))break;
        if(now-requestAt_>=requestWait_){
          request(now);requestAt_=now;requestWait_=requestInterval_;
          requestInterval_=requestInterval_*2>RequestMaxMs?RequestMaxMs:requestInterval_*2;
        }
        break;
      case Phase::Join:
      case Phase::Listen:
        if(pump(now,false))break;
        if(now-phaseAt_>=window_){
          if(!secure(now))break;
          if(status_.phase==Phase::Listen)++status_.cycles;
          planIndex_=0;enter(Phase::Scan,now);
        }
        break;
      case Phase::Scan:scanStep(now);break;
      case Phase::Ble:
        if(now-phaseAt_<BleScanMs+BleGuardMs)break;
        port_->ble(false,0);report(now,true);
        break;
      case Phase::Report:reportStep(now);break;
      default:break;
    }
  }

  // A frame heard on the home channel. Call from the main loop, in arrival order.
  void received(const uint8_t* source,const uint8_t* data,size_t length,int rssi,uint32_t now){
    Frame frame;
    if(status_.phase==Phase::Off||!source||!parseFrame(data,length,frame))return;
    if(!status_.linked){
      if(frame.type!=CoreReply)return;
      if(config_.bound&&!sameMac(source,config_.boundCore))return;
      const Core kind=classifyReply(frame);
      if(joinable(kind))link(source,kind,rssi,now);
      else{status_.other=kind;otherAt_=now;}
      return;
    }
    if(!sameMac(source,core_))return;         // only the linked core may steer this node
    if(frame.type==CoreReply&&!joinable(classifyReply(frame))){
      status_.other=classifyReply(frame);otherAt_=now;++status_.losses;unlink(now);return; // its mode changed
    }
    lastCoreAt_=now;status_.coreRssi=clampRssi(rssi);  // only the core's own frames prove it is there
    if(frame.type==CoreReply&&!secure(now))return;
    switch(frame.type){
      // A Biscuit takes every request, this node's probe included, as a node
      // that is new to it. It may also have restarted. Either way it has to be told who this is again.
      case CoreReply:if(biscuit())identify(now,true);break;
      case RecordAck:++status_.acked;status_.acknowledged=true;break;
      case Admin:
        if(frame.full()){                    // Biscuit style role assignment
          status_.role=frame.textLength?frame.text[0]:uint8_t(frame.counter);status_.registered=true;roleWanted_=false;
          control(RoleAck,status_.role,nullptr,0);
        }else if(length>=AdminSize)applyAdmin(data);
        break;
      case ConfigUpdate:
        if(frame.full()){applyConfig(frame.text,frame.textLength);control(ConfigAck,0,nullptr,0);}
        break;
      default:break;                         // heartbeats only prove the core is alive
    }
  }
  // Outcome of the last transmit(). Broadcasts always report delivered.
  void sent(bool delivered,uint32_t now){
    if(status_.phase==Phase::Off||flight_==Flight::None)return;
    settle(delivered,true,now);
  }
  void wifi(const uint8_t* bssid,const uint8_t* ssid,size_t ssidLength,Auth auth,uint8_t channel,int rssi){
    if(!status_.linked||!bssid)return;
    Sighting s{};memcpy(s.mac,bssid,6);
    s.ssidLength=uint8_t(ssidLength>32?32:ssidLength);
    if(ssid&&s.ssidLength)memcpy(s.ssid,ssid,s.ssidLength);else s.ssidLength=0;
    s.auth=auth;s.channel=channel;s.rssi=clampRssi(rssi);s.kind=Kind::Wifi;
    observe(s);
  }
  void ble(const uint8_t* address,int rssi){
    if(!status_.linked||!address||!bleWanted()||status_.phase!=Phase::Ble)return;
    Sighting s{};memcpy(s.mac,address,6);s.rssi=clampRssi(rssi);s.kind=Kind::Ble;
    observe(s);
  }
  // The channel scan started by Port::scan() has ended and its results were passed in.
  void scanned(bool ok,uint32_t now){
    (void)now;
    if(!scanning_)return;
    scanning_=false;++planIndex_;
    if(ok)++status_.scans;else ++status_.scanErrors;
  }

private:
  enum class Flight : uint8_t {None,Request,Heartbeat,Control,Record,Stale};
  struct Sighting {
    uint8_t mac[6];uint8_t ssid[32];uint8_t ssidLength;Auth auth;uint8_t channel;int8_t rssi;
    Kind kind;uint8_t tries;uint16_t slot;
  };
  struct Control {uint8_t type;uint8_t length;uint8_t tries;uint32_t counter;char text[48];};

  static int8_t clampRssi(int rssi){return int8_t(rssi<-127?-127:rssi>0?0:rssi);}
  uint32_t random(){random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return random_;}
  bool biscuit() const {return status_.core==Core::Biscuit;}
  // A Piglet core logs every record as a Wi-Fi network, so Bluetooth goes to a JCMK core only.
  bool bleWanted() const {return config_.ble&&config_.family==Family::Jcmk&&status_.core==Core::Jcmk;}
  // A Piglet core has no ESP-NOW encryption: a keyed node would talk to it unheard.
  bool joinable(Core kind) const {
    if(config_.family==Family::Biscuit)return kind==Core::Biscuit;
    return kind==Core::Jcmk||(kind==Core::Piglet&&!config_.encrypted);
  }
  // The radio acknowledges an encrypted frame before the core has tried to
  // read it, and a Biscuit that restarted or forgot this node still
  // acknowledges. Only an open JCMK-style link takes the acknowledgement as an answer.
  bool ackProvesLife() const {return !config_.encrypted&&!biscuit();}
  void enter(Phase phase,uint32_t now){
    status_.phase=phase;phaseAt_=now;
    if(phase!=Phase::Scan)status_.channel=HomeChannel;
  }
  void search(uint32_t now,uint32_t stagger){
    port_->tune(HomeChannel);
    requestInterval_=RequestFirstMs;requestAt_=now;requestWait_=stagger;
    enter(Phase::Search,now);
  }
  void resetAssignment(){
    status_.assigned=status_.ownPlan=false;status_.role=status_.nodeIndex=status_.nodeCount=0;
    adminVersion_=adminFirst_=adminLast_=0;adminApplied_=false;
    status_.dwellMs=DwellDefaultMs;planIndex_=0;
    planClear();planFinish();
  }
  void planClear(){status_.planCount=0;}
  void planAdd(unsigned channel){
    if(channel<1||channel>config_.maxChannel||status_.planCount>=PlanMax)return;
    for(uint8_t i=0;i<status_.planCount;++i)if(status_.plan[i]==channel)return;
    status_.plan[status_.planCount++]=uint8_t(channel);
  }
  // A plan without a channel this radio can reach falls back to the whole band:
  // the core removes duplicates, so extra coverage costs nothing.
  void planFinish(){
    status_.ownPlan=status_.assigned&&!status_.planCount;
    if(!status_.planCount)for(unsigned c=1;c<=config_.maxChannel;++c)planAdd(c);
  }
  // A range of the JCMK table. A JCMK core that reads the opt-out sends none,
  // so a keyed link never has to read a sealed frame right behind the clear
  // reply, before the main loop has put the key in. An older core may still assign one.
  void applyAdmin(const uint8_t* data){
    const uint8_t version=data[5],first=data[8],last=data[9];
    status_.nodeIndex=data[6];status_.nodeCount=data[7];
    if(adminApplied_&&version==adminVersion_&&first==adminFirst_&&last==adminLast_)return;
    adminApplied_=true;adminVersion_=version;adminFirst_=first;adminLast_=last;
    planClear();status_.assigned=true;
    if(first<=last&&last<ChannelCount)for(unsigned i=first;i<=last;++i)planAdd(ChannelTable[i]);
    else for(uint8_t channel:ChannelTable)planAdd(channel);  // a nonsense range means everything
    planFinish();
  }
  // "channels=1,6,11;dwell=80", read as the Biscuit's reference node reads it:
  // white space and a plus sign before a number and a trailing comma are fine,
  // a channel 1-255 that is not in the table is skipped, any other malformed
  // entry (empty, not a number, 0, over 255, or followed by anything but a
  // comma) discards the list, and nothing past 42 listed channels is read.
  // The text ends at a terminator, should the length count one.
  void applyConfig(const uint8_t* text,size_t length){
    adminApplied_=false;planClear();
    for(size_t n=0;n<length;++n)if(!text[n]){length=n;break;}
    bool valid=length>=9&&!memcmp(text,"channels=",9);unsigned listed=0;size_t i=9;
    while(valid&&i<length&&text[i]!=';'&&listed<ChannelCount+2){
      while(i<length&&(text[i]==' '||(text[i]>='\t'&&text[i]<='\r')))++i;
      if(i<length&&text[i]=='+')++i;
      unsigned value=0,digits=0;
      while(i<length&&text[i]>='0'&&text[i]<='9'){if(value<1000)value=value*10+unsigned(text[i]-'0');++i;++digits;}
      if(!digits||value<1||value>255){valid=false;break;}
      if(knownChannel(value)){++listed;planAdd(value);}
      if(i<length&&text[i]==',')++i;         // also a trailing one: the list then simply ends
      else if(i<length&&text[i]!=';')valid=false;
    }
    if(!valid){planClear();listed=0;}
    status_.assigned=listed>0;
    size_t at=0;
    if(length&&findText(text,length,"dwell=",at)){
      unsigned value=0,digits=0;
      for(size_t j=at+6;j<length&&text[j]>='0'&&text[j]<='9'&&digits<5;++j,++digits)value=value*10+unsigned(text[j]-'0');
      if(digits&&value)status_.dwellMs=uint16_t(value<DwellMinMs?DwellMinMs:value>DwellMaxMs?DwellMaxMs:value);
    }
    planFinish();
  }
  void link(const uint8_t* mac,Core kind,int rssi,uint32_t now){
    if(!port_->peer(mac,config_.encrypted?Peer::Encrypted:Peer::Plain))return;
    memcpy(core_,mac,6);memcpy(status_.coreMac,mac,6);
    status_.linked=true;status_.core=kind;status_.other=Core::None;status_.coreRssi=clampRssi(rssi);
    status_.broadcast=status_.acknowledged=false;++status_.links;
    lastCoreAt_=now;unicastProven_=false;unicastFails_=0;controlCount_=0;ackProbes_=AckProbes;rekey_=false;
    resetAssignment();
    status_.registered=kind!=Core::Biscuit;    // a JCMK-style core takes on whoever asked
    if(kind==Core::Biscuit){
      identify(now,true);
      window_=BiscuitWindowMs+random()%(BiscuitWindowJitterMs+1);
    }else window_=JcmkWindowMs;
    enter(Phase::Join,now);
  }
  // Tells a Biscuit who this is. It answers with a role, then a channel list.
  // Records keep flowing meanwhile, as on the reference node. A Biscuit that
  // already gave a role stays registered while it is asked again, so the
  // screen does not flicker on every probe; it is dropped only when every try
  // goes unanswered.
  void identify(uint32_t now,bool fresh){
    if(fresh)identifyTries_=0;
    ++identifyTries_;identifyAt_=now;roleWanted_=true;
    control(Identify,0,identity_,strlen(identity_));
  }
  void unlink(uint32_t now){
    if(scanning_){port_->scanStop();scanning_=false;}
    if(status_.phase==Phase::Ble)port_->ble(false,0);
    port_->peer(core_,Peer::Remove);
    if(config_.bound)port_->peer(config_.boundCore,Peer::Plain);
    status_.linked=status_.registered=false;status_.core=Core::None;status_.broadcast=status_.acknowledged=false;rekey_=false;roleWanted_=false;
    queueHead_=queueCount_=controlCount_=0;
    if(flight_!=Flight::None)flight_=Flight::Stale;  // still on the air: wait for its report before sending more
    // Whatever was told to the old core is told again to the next one.
    for(size_t i=0;i<config_.seenCount;++i)config_.seen[i].flags&=uint8_t(~SeenReported);
    resetAssignment();search(now,0);
  }

  static uint32_t hash(const uint8_t* mac,bool ble){
    uint32_t h=2166136261u;
    for(int i=0;i<6;++i){h^=mac[i];h*=16777619u;}
    if(ble)h^=0x5bd1e995u;
    return h^h>>15;
  }
  // Open addressing over a short window. The counts stay exact while every
  // window has room. Once one is full an entry in it is replaced, and that
  // network or device counts as new if it is heard again. A Bluetooth address
  // is replaced before any network: phones change theirs anyway.
  Seen* remember(const Sighting& s,bool& fresh,uint16_t& slot){
    const bool ble=s.kind==Kind::Ble;const size_t mask=config_.seenCount-1,home=hash(s.mac,ble)&mask;
    const size_t depth=config_.seenCount<ProbeDepth?config_.seenCount:ProbeDepth;
    size_t emptyAt=SIZE_MAX,devices[ProbeDepth],deviceCount=0;
    for(size_t i=0;i<depth;++i){
      const size_t at=(home+i)&mask;Seen& e=config_.seen[at];
      if(!(e.flags&SeenUsed)){if(emptyAt==SIZE_MAX)emptyAt=at;continue;}
      if(sameMac(e.mac,s.mac)&&bool(e.flags&SeenBle)==ble){fresh=false;slot=uint16_t(at);return &e;}
      if(e.flags&SeenBle)devices[deviceCount++]=at;
    }
    if(emptyAt==SIZE_MAX)emptyAt=deviceCount?devices[evictions_++%deviceCount]:(home+evictions_++%depth)&mask; // full: replace one
    Seen* entry=&config_.seen[emptyAt];
    memcpy(entry->mac,s.mac,6);entry->rssi=s.rssi;entry->flags=uint8_t(SeenUsed|(ble?SeenBle:0));
    fresh=true;slot=uint16_t(emptyAt);return entry;
  }
  void observe(Sighting& s){
    static const uint8_t zero[6]={};
    if(sameMac(s.mac,zero))return;
    bool fresh=false;Seen* entry=remember(s,fresh,s.slot);
    if(fresh){if(s.kind==Kind::Ble)++status_.ble;else ++status_.wifi;}
    // Told once, then again when the signal is clearly stronger or about a
    // minute has passed. A core that logs every record stamps its own position
    // on each, so there the later, closer report is kept. A JCMK core logs an
    // address only while it is not among the last 200 it logged, so there the
    // first report normally stands and the repeats are only acknowledged.
    const uint8_t stamp=uint8_t(now_>>12&SeenStampMask);
    if(entry->flags&SeenReported){
      const bool stronger=s.kind==Kind::Wifi&&s.rssi>=entry->rssi+RssiStep;
      const bool stale=uint8_t((stamp-(entry->flags>>SeenStampShift))&SeenStampMask)>=RefreshUnits;
      if(!stronger&&!stale)return;
    }
    if(!push(s)){++status_.dropped;return;}  // stays as it was, so the next sighting retries
    entry->flags=uint8_t((entry->flags&(SeenUsed|SeenBle))|SeenReported|stamp<<SeenStampShift);entry->rssi=s.rssi;
  }
  void forget(const Sighting& s){
    if(s.slot>=config_.seenCount)return;
    Seen& e=config_.seen[s.slot];
    if((e.flags&SeenUsed)&&sameMac(e.mac,s.mac)&&bool(e.flags&SeenBle)==(s.kind==Kind::Ble))e.flags&=uint8_t(~SeenReported);
  }
  bool push(const Sighting& s){
    if(queueCount_>=QueueSize)return false;
    queue_[(queueHead_+queueCount_++)%QueueSize]=s;return true;
  }
  void control(uint8_t type,uint32_t counter,const char* text,size_t length){
    if(controlCount_>=ControlSize){memmove(controls_,controls_+1,sizeof(Control)*(ControlSize-1));--controlCount_;}
    Control& c=controls_[controlCount_++];c.type=type;c.counter=counter;c.tries=0;
    c.length=uint8_t(length>sizeof(c.text)?sizeof(c.text):length);
    if(c.length)memcpy(c.text,text,c.length);
  }
  const uint8_t* reportTarget() const {return status_.broadcast?BroadcastMac:core_;}
  void launch(Flight kind,const uint8_t* destination,uint32_t now){
    flight_=kind;flightAt_=now;flightUnicast_=!sameMac(destination,BroadcastMac);
    if(!port_->transmit(destination,frame_,FrameSize))settle(false,false,now);
  }
  void settle(bool delivered,bool onAir,uint32_t now){
    const Flight kind=flight_;flight_=Flight::None;sendDoneAt_=now;
    if(flightUnicast_&&onAir&&status_.linked){
      if(delivered){unicastProven_=true;unicastFails_=0;if(ackProvesLife())lastCoreAt_=now;}
      // A core that never acknowledges a unicast still hears broadcasts.
      else if(unicastFails_<255&&++unicastFails_>=UnicastGiveUp&&!unicastProven_&&!config_.encrypted)status_.broadcast=true;
    }
    if(kind==Flight::Control){
      // A handshake step that did not get through is offered again, ahead of
      // newer ones. A core known not to acknowledge unicasts may have heard it.
      if(!delivered&&status_.linked&&(!onAir||!status_.broadcast)&&++control_.tries<SendTries&&controlCount_<ControlSize){
        memmove(controls_+1,controls_,sizeof(Control)*controlCount_);controls_[0]=control_;++controlCount_;
      }
      return;
    }
    if(kind!=Flight::Record)return;
    if(delivered){++status_.sent;reported_=true;return;}
    if(++record_.tries<SendTries&&status_.linked&&push(record_))return;
    ++status_.failed;forget(record_);
  }
  void request(uint32_t now){
    buildFrame(frame_,CoreRequest,0,ExternalCapability,ExternalCapabilityLength);++status_.requests;
    // A core answers a request in the clear and then installs the key again,
    // so a keyed link drops its own key until that answer arrives.
    if(status_.linked&&config_.encrypted&&!rekey_&&port_->peer(core_,Peer::Plain))rekey_=true;
    // Searching asks everyone (or the bound core). A linked node asks its own
    // core only, the way it reports, so no other core in range takes it on.
    launch(Flight::Request,status_.linked?reportTarget():config_.bound?config_.boundCore:BroadcastMac,now);
  }
  // Puts the key back on a link that asked in the clear. false when the link was dropped.
  bool secure(uint32_t now){
    if(!rekey_)return true;
    rekey_=false;
    if(port_->peer(core_,Peer::Encrypted))return true;
    ++status_.losses;unlink(now);return false;
  }
  // Sends one waiting message. true while something is on its way out.
  bool pump(uint32_t now,bool records){
    if(flight_!=Flight::None){
      if(now-flightAt_<SendTimeoutMs)return true;
      settle(false,true,now);
    }
    if(rekey_&&(controlCount_||(records&&queueCount_))&&!secure(now))return false;
    if(controlCount_&&status_.linked){
      control_=controls_[0];
      memmove(controls_,controls_+1,sizeof(Control)*(--controlCount_));
      buildFrame(frame_,control_.type,control_.counter,control_.text,control_.length);
      launch(Flight::Control,core_,now);return true;
    }
    if(records&&queueCount_&&status_.linked){
      record_=queue_[queueHead_];queueHead_=(queueHead_+1)%QueueSize;--queueCount_;
      char line[TextMax+1];
      const size_t n=record_.kind==Kind::Ble?bleLine(line,sizeof(line),record_.mac,record_.rssi):
        wifiLine(line,sizeof(line),record_.mac,record_.ssid,record_.ssidLength,record_.auth,record_.channel,record_.rssi,biscuit());
      if(++textCounter_==0)textCounter_=1;
      buildFrame(frame_,Text,textCounter_,line,n);
      launch(Flight::Record,reportTarget(),now);return true;
    }
    return false;
  }
  void report(uint32_t now,bool cycleEnd){
    if(scanning_){port_->scanStop();scanning_=false;}
    port_->tune(HomeChannel);cycleEnd_=cycleEnd;reportStage_=0;reported_=false;
    enter(Phase::Report,now);
  }
  void reportStep(uint32_t now){
    if(now-phaseAt_<SettleMs)return;
    if(pump(now,now-phaseAt_<ReportBudgetMs))return;
    if(cycleEnd_&&reportStage_==0){
      if(!secure(now))return;
      reportStage_=1;buildFrame(frame_,Heartbeat,heartbeatCounter_++,ExternalCapability,ExternalCapabilityLength);++status_.heartbeats;
      launch(Flight::Heartbeat,reportTarget(),now);return;
    }
    if(cycleEnd_&&reportStage_==1){
      reportStage_=2;
      // Nothing heard for a while: a core answers a request even when it has
      // nothing new to say, which tells a live core from a lost one. A
      // Biscuit's own nodes give up after 30 s without a frame from it, so a
      // live Biscuit is expected to send on its own and then is rarely asked.
      if(now-lastCoreAt_>=ProbeAfterMs&&now-probeAt_>=ProbeAfterMs/2){probeAt_=now;request(now);return;}
    }
    // Newer cores acknowledge each record. Wait for those on the first reports
    // of a link, and keep waiting only for a core that actually sends them.
    if(reported_&&(status_.acknowledged||ackProbes_)){
      if(now-sendDoneAt_<AckLingerMs)return;
      if(ackProbes_)--ackProbes_;
    }
    if(cycleEnd_){
      // Still no role: identify again, then wait home for the answer. After
      // the last try, a Biscuit that gave a role before no longer counts as registered.
      if(biscuit()&&roleWanted_&&now-identifyAt_>=IdentifyRetryMs){
        if(identifyTries_<IdentifyTries)identify(now,false);
        else{roleWanted_=false;status_.registered=false;}
      }
      window_=biscuit()?BiscuitWindowMs+random()%(BiscuitWindowJitterMs+1):JcmkWindowMs;
      enter(Phase::Listen,now);
    }else enter(Phase::Scan,now);
  }
  void scanStep(uint32_t now){
    if(scanning_){
      if(now-scanAt_<uint32_t(status_.dwellMs)+ScanGraceMs)return;
      port_->scanStop();scanning_=false;++planIndex_;++status_.scanErrors;scanFailed_=true;scanAt_=now;
    }
    if(queueCount_||controlCount_){report(now,false);return;}
    if(planIndex_>=status_.planCount){
      if(bleWanted()){port_->tune(HomeChannel);port_->ble(true,BleScanMs);enter(Phase::Ble,now);}
      else report(now,true);
      return;
    }
    if(scanFailed_&&now-scanAt_<ScanRetryMs)return;
    scanFailed_=false;status_.channel=status_.plan[planIndex_];
    if(port_->scan(status_.channel,status_.dwellMs)){scanning_=true;scanAt_=now;}
    else{++planIndex_;++status_.scanErrors;scanFailed_=true;scanAt_=now;}
  }

  Port* port_=nullptr;Config config_;Status status_;
  char identity_[48]={};
  uint8_t core_[6]={},frame_[FrameSize]={};
  Sighting queue_[QueueSize]={},record_={};
  Control controls_[ControlSize]={},control_={};
  size_t queueHead_=0,queueCount_=0,controlCount_=0;
  Flight flight_=Flight::None;
  bool flightUnicast_=false,unicastProven_=false,scanning_=false,scanFailed_=false,cycleEnd_=false,reported_=false,adminApplied_=false,rekey_=false,roleWanted_=false;
  uint8_t unicastFails_=0,reportStage_=0,planIndex_=0,adminVersion_=0,adminFirst_=0,adminLast_=0,ackProbes_=0,identifyTries_=0;
  uint32_t random_=1,textCounter_=0,heartbeatCounter_=0,evictions_=0;
  uint32_t phaseAt_=0,window_=0,requestAt_=0,requestWait_=0,requestInterval_=RequestFirstMs;
  uint32_t lastCoreAt_=0,otherAt_=0,flightAt_=0,sendDoneAt_=0,scanAt_=0,probeAt_=0,identifyAt_=0,now_=0;
};

} // namespace dragon::node
