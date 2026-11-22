// Native test of the node session (firmware/src/node_mode.cpp): the USB commands,
// the command gate's allow list, JCMK encryption and its keys, the remembered
// choice and the restart guard. tools/test_core.py builds it with empty stand-ins
// for the Arduino and ESP-IDF headers; everything they would declare is here.
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define RTC_NOINIT_ATTR
#define DRAGON_VERSION "test"

struct String {
  std::string s;
  String(){}
  String(const char* c):s(c?c:""){}
  String(const std::string& x):s(x){}
  const char* c_str() const {return s.c_str();}
  unsigned length() const {return unsigned(s.size());}
  char operator[](unsigned i) const {return i<s.size()?s[i]:0;}
  bool startsWith(const char* p) const {return s.rfind(p,0)==0;}
  String substring(unsigned from) const {return from>=s.size()?String():String(s.substr(from));}
  String substring(unsigned from,unsigned to) const {return from>=to||from>=s.size()?String():String(s.substr(from,to-from));}
  bool operator==(const char* o) const {return s==o;}
  bool operator!=(const char* o) const {return s!=o;}
  friend String operator+(const String& a,const char* b){return String(a.s+b);}
};

namespace sim {
std::string log,transcript;      // this command's USB output, and everything printed
uint32_t now=1000;
std::string ui="radar";
}
struct SerialStub {
  void printf(const char* format,...){char b[2048];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);sim::log+=b;sim::transcript+=b;}
  void println(const char* text){printf("%s\n",text);}
} Serial;
struct EspStub {unsigned getFreeHeap(){return 200000;}unsigned getMinFreeHeap(){return 150000;}} ESP;
uint32_t millis(){return sim::now;}

// Settings storage that outlives a restart.
namespace sim {
struct Value {char type=0;bool b=false;uint8_t u=0;std::string text;};
std::map<std::string,Value> nvs;
}
struct Preferences {
  bool begin(const char*,bool){return true;}
  bool isKey(const char* k){return sim::nvs.count(k)>0;}
  size_t getBytesLength(const char* k){auto it=sim::nvs.find(k);return it!=sim::nvs.end()&&it->second.type=='y'?it->second.text.size():0;}
  size_t getBytes(const char* k,void* out,size_t n){auto it=sim::nvs.find(k);if(it==sim::nvs.end()||it->second.type!='y'||n<it->second.text.size())return 0;memcpy(out,it->second.text.data(),it->second.text.size());return it->second.text.size();}
  bool getBool(const char* k,bool d){auto it=sim::nvs.find(k);return it!=sim::nvs.end()&&it->second.type=='b'?it->second.b:d;}
  uint8_t getUChar(const char* k,uint8_t d){auto it=sim::nvs.find(k);return it!=sim::nvs.end()&&it->second.type=='u'?it->second.u:d;}
  String getString(const char* k,const char* d){auto it=sim::nvs.find(k);return it!=sim::nvs.end()&&it->second.type=='s'?String(it->second.text):String(d);}
  void putBool(const char* k,bool v){auto& x=sim::nvs[k];x={};x.type='b';x.b=v;}
  void putUChar(const char* k,uint8_t v){auto& x=sim::nvs[k];x={};x.type='u';x.u=v;}
  size_t putString(const char* k,const char* v){auto& x=sim::nvs[k];x={};x.type='s';x.text=v;return strlen(v);}
  void putBytes(const char* k,const void* v,size_t n){auto& x=sim::nvs[k];x={};x.type='y';x.text.assign(static_cast<const char*>(v),n);}
  bool remove(const char* k){return sim::nvs.erase(k)>0;}
};

typedef enum {ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,
  ESP_RST_BROWNOUT,ESP_RST_SDIO,ESP_RST_USB,ESP_RST_JTAG,ESP_RST_EFUSE,ESP_RST_PWR_GLITCH,ESP_RST_CPU_LOCKUP} esp_reset_reason_t;   // ESP-IDF 5.1 values
namespace sim {esp_reset_reason_t reason=ESP_RST_POWERON;}
esp_reset_reason_t esp_reset_reason(){return sim::reason;}
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
size_t heap_caps_get_largest_free_block(int){return 100000;}

// SHA-256 (FIPS 180-4), standing in for mbedtls_sha256.
namespace sha {
const uint32_t K[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
  0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,
  0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
  0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
inline uint32_t rotr(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
void digest(const uint8_t* data,size_t n,uint8_t out[32]){
  uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  std::vector<uint8_t> m(data,data+n);m.push_back(0x80);
  while(m.size()%64!=56)m.push_back(0);
  for(int i=7;i>=0;--i)m.push_back(uint8_t((uint64_t(n)*8)>>(i*8)));
  for(size_t at=0;at<m.size();at+=64){
    uint32_t w[64];
    for(int i=0;i<16;++i)w[i]=uint32_t(m[at+i*4])<<24|uint32_t(m[at+i*4+1])<<16|uint32_t(m[at+i*4+2])<<8|m[at+i*4+3];
    for(int i=16;i<64;++i)w[i]=w[i-16]+(rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10));
    uint32_t v[8];memcpy(v,h,sizeof(v));
    for(int i=0;i<64;++i){
      const uint32_t t1=v[7]+(rotr(v[4],6)^rotr(v[4],11)^rotr(v[4],25))+((v[4]&v[5])^(~v[4]&v[6]))+K[i]+w[i];
      const uint32_t t2=(rotr(v[0],2)^rotr(v[0],13)^rotr(v[0],22))+((v[0]&v[1])^(v[0]&v[2])^(v[1]&v[2]));
      memmove(v+1,v,7*sizeof(uint32_t));v[4]+=t1;v[0]=t1+t2;
    }
    for(int i=0;i<8;++i)h[i]+=v[i];
  }
  for(int i=0;i<32;++i)out[i]=uint8_t(h[i/4]>>(24-(i%4)*8));
}
}
int mbedtls_sha256(const unsigned char* input,size_t n,unsigned char* output,int is224){assert(!is224);sha::digest(input,n,output);return 0;}

// What the session stops, refuses for, or shows.
namespace sim {
bool sdRecording=false,sdClosing=false,ramRecording=false,labOwned=false,labClosing=false,beeper=false,wardrive=false,upload=false,fox=false;
int reconStops=0,labStops=0,recipeStops=0;
}
struct StopwatchStub {bool running=false;} stopwatch;
void macText(const uint8_t* mac,char* out){snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);}
bool setCapture(bool){return false;}
void uiShow(const String& page){sim::ui=page.s;}
void audioStop(){}
bool foxTransmit(){return sim::fox;}
void setFoxTransmit(bool on){sim::fox=on;}
bool reconStorageBusy(){return sim::sdRecording||sim::sdClosing;}   // like SdRecords::busy(): recording or finalising
void reconStop(){++sim::reconStops;if(sim::sdRecording){sim::sdRecording=false;sim::sdClosing=true;}sim::ramRecording=false;}
void recipeStop(){++sim::recipeStops;}
void missionStop(){}
bool bleLabOwnsRadio(){return sim::labOwned;}
void bleLabStop(){++sim::labStops;sim::beeper=false;if(sim::labOwned)sim::labClosing=true;}
bool wardriveBusy(){return sim::wardrive;}
bool wardriveUploadBusy(){return sim::upload;}

#include "../firmware/src/node_mode.cpp"

// BadUSB (badusb.cpp): keyboard mode is decided at boot; a script types only in it.
namespace sim {bool keyboardMode=false,scriptRunning=false;int badusbReleases=0;}
void badusbRelease(){++sim::badusbReleases;}
bool badusbKeyboardMode(){return sim::keyboardMode;}
BadUsbStatus badusbStatus(){BadUsbStatus s{};s.keyboardMode=sim::keyboardMode;s.running=sim::scriptRunning;s.connected=sim::keyboardMode;s.name=s.message=s.medium="";return s;}

// The radio side (radio.cpp).
namespace sim {
struct Radio {bool owned=false,keys=false;int fail=0,begins=0;dragon::node::Config config;uint8_t pmk[16]{},lmk[16]{};dragon::node::Status status;} radio;
const dragon::node::Status idle{};
}
bool radioNodeBegin(const dragon::node::Config& config,const uint8_t* pmk,const uint8_t* lmk,char* why,size_t whyLength){
  ++sim::radio.begins;
  if(sim::radio.owned){snprintf(why,whyLength,"Node already running");return false;}
  if(sim::radio.fail){--sim::radio.fail;snprintf(why,whyLength,"Wi-Fi could not switch to node mode");return false;}
  sim::radio.owned=true;sim::radio.config=config;sim::radio.keys=pmk&&lmk;
  memset(sim::radio.pmk,0,16);memset(sim::radio.lmk,0,16);if(pmk)memcpy(sim::radio.pmk,pmk,16);if(lmk)memcpy(sim::radio.lmk,lmk,16);
  return true;
}
void radioNodeEnd(){sim::radio.owned=false;}
bool radioNodeOwned(){return sim::radio.owned;}
const dragon::node::Status& radioNodeStatus(){return sim::radio.owned?sim::radio.status:sim::idle;}
void radioNodeMac(uint8_t* mac){const uint8_t own[6]={0x24,0x6f,0x28,1,2,3};memcpy(mac,own,6);}
uint32_t radioNodeReceiveDrops(){return 0;}
namespace sim{bool trace=false,traceMemory=true;}
void radioNodeTrace(bool on){sim::trace=on&&sim::traceMemory;}   // no memory for a line: stays off
bool radioNodeTracing(){return sim::trace;}

namespace {
std::string run(const char* command){sim::log.clear();assert(nodeCommand(command));return sim::log;}
bool has(const std::string& text,const char* part){return text.find(part)!=std::string::npos;}
// The value of " name=" in a status line.
std::string field(const std::string& line,const char* name){
  const std::string tag=std::string(" ")+name+"=";const size_t at=line.find(tag);assert(at!=std::string::npos);
  const size_t from=at+tag.size();return line.substr(from,line.find_first_of(" \n",from)-from);
}
std::string hex(const uint8_t* b,size_t n){std::string out;char x[3];for(size_t i=0;i<n;++i){snprintf(x,sizeof(x),"%02x",b[i]);out+=x;}return out;}
std::string sha16(const std::string& text){uint8_t h[32];sha::digest(reinterpret_cast<const uint8_t*>(text.data()),text.size(),h);return hex(h,16);}
// A restart: RAM starts over, settings storage survives, and RTC memory survives
// every reset except power-on, when it holds whatever it powered up with.
void reboot(esp_reset_reason_t reason){
  prefs=Preferences{};prefsReady=false;running=NodeKind::None;startedAt=0;error[0]=0;identity[0]=0;key[0]=0;memset(core,0,sizeof(core));
  bound=false;bleReports=true;encryption=false;linkBound=linkKeyed=linkBle=false;resumeWatch=false;resumedAt=0;
  if(reason==ESP_RST_POWERON){resumeGuard[0]=0xa5a5a5a5;resumeGuard[1]=0x5a5a5a5a;}
  sim::radio=sim::Radio{};sim::ui="radar";sim::reason=reason;sim::now=1000;sim::log.clear();
  nodeBegin();
}
uint8_t savedMode(){auto it=sim::nvs.find("mode");return it==sim::nvs.end()?0:it->second.u;}
void wipe(){sim::nvs.clear();reboot(ESP_RST_POWERON);}
}

static void testSha(){
  assert(sha16("abc")=="ba7816bf8f01cfea414140de5dae2223");
  // JCMK core with "Use Encryption" on and no ENOW Key typed: SHA-256 of "_pmk" and "_lmk".
  assert(sha16("_pmk")=="f37350bcda2297cfd8692f574c73cf58");
  assert(sha16("_lmk")=="258662ceedc8827f19b457069dbd6504");
}

static void testAllowList(){
  for(const char* c:{"node","node status","node stop","node jcmk","status","badge info","display status","clock status","clock hardware","orientation status",
      "time 1700000000","clock offset 60","radar status","tone settings","theme info","tone get 3","theme read 0 512","screen node","system restart"})
    assert(nodeAllowsCommand(c));
  // Anything that changes settings, starts something or touches the radio waits for the node.
  for(const char* c:{"nodes","radar set 1 1 1","tone config 1 50 1 -1","tone set 1 x:d=4,o=5,b=100:c","tone play 1","tone reset 1","tone level loud",
      "theme upload 100 0","theme apply","theme remove","display set 1 2 3 4","badge upload 10 0","wardrive end","wardrive start","wardrive status",
      "wardrive upload start","gps status","gps external on","lab sound repeat","lab stop","recon capture wifi","capture on","screen radar","get /x","resume"})
    assert(!nodeAllowsCommand(c));
}

static void testStatusAndStop(){
  wipe();
  std::string out=run("node");assert(has(out,"NODE_STATUS mode=none active=0 "));
  // node stop with no node running forgets the choice and leaves the screen alone.
  sim::ui="ble-lab";out=run("node stop");assert(out=="NODE_OK stopped\n");assert(sim::ui=="ble-lab");
  // node trace: a switch for the USB log only, answered either way, with or without a node.
  assert(run("node trace on")=="NODE_OK trace=1\n"&&sim::trace);assert(run("node trace off")=="NODE_OK trace=0\n"&&!sim::trace);
  assert(has(run("node trace"),"NODE_ERROR commands:")&&has(run("node trace maybe"),"trace on|off"));
  assert(run("node biscuit")=="NODE_STARTED biscuit\nNODE_OK biscuit\n");assert(sim::ui=="node");assert(savedMode()==1);
  // While a node runs it is allowed, changes nothing else and leaves the node running.
  assert(nodeAllowsCommand("node trace on"));
  assert(run("node trace on")=="NODE_OK trace=1\n"&&nodeBusy()&&sim::ui=="node");assert(run("node trace off")=="NODE_OK trace=0\n"&&nodeBusy());
  sim::traceMemory=false;assert(run("node trace on")=="NODE_OK trace=0\n"&&!sim::trace);sim::traceMemory=true;
  sim::radio.status.registered=true;sim::radio.status.linked=true;
  out=run("node status");assert(field(out,"registered")=="1");assert(field(out,"linked")=="1");
  sim::radio.status.registered=false;out=run("node status");assert(field(out,"registered")=="0");
  out=run("node stop");assert(out=="NODE_STOPPED radar restored\nNODE_OK stopped\n");assert(sim::ui=="modes");assert(savedMode()==0);
}

static void testSettingsInUse(){
  wipe();
  run("node key s3cr3t-Key");run("node bind 24:6F:28:10:20:30");
  // Saved: keyed, Bluetooth reports and the binding. Nothing runs, so none is in use.
  std::string out=run("node status");assert(field(out,"keyed")=="0"&&field(out,"ble_reports")=="0"&&field(out,"bound")=="0");
  out=run("node config");assert(has(out,"NODE_CONFIG remembered=none bound=1 core=24:6F:28:10:20:30 keyed=1 ble_reports=1 "));
  // BISCUIT NODE never encrypts and never reports Bluetooth, whatever is saved.
  run("node biscuit");assert(!sim::radio.keys&&!sim::radio.config.encrypted&&sim::radio.config.bound);
  out=run("node status");assert(field(out,"keyed")=="0"&&field(out,"ble_reports")=="0"&&field(out,"bound")=="1");
  assert(!nodeKeyed()&&!nodeBleReports());
  run("node jcmk");assert(sim::radio.keys&&sim::radio.config.encrypted);
  out=run("node status");assert(field(out,"mode")=="jcmk"&&field(out,"keyed")=="1"&&field(out,"ble_reports")=="1"&&field(out,"bound")=="1");
  assert(nodeKeyed()&&nodeBleReports());
  run("node ble off");out=run("node status");assert(field(out,"ble_reports")=="0");assert(!nodeBleReports());
  out=run("node config");assert(field(out,"keyed")=="1"&&field(out,"ble_reports")=="0");
  run("node stop");assert(!nodeKeyed()&&!nodeBleReports());
}

static void testEncryption(){
  wipe();
  // Encryption on with no key: the keys of a core whose ENOW Key was never typed.
  assert(run("node encrypt on")=="NODE_OK keyed=1\n");assert(sim::nvs.at("enc").b&&!sim::nvs.count("key"));
  run("node jcmk");assert(sim::radio.keys&&sim::radio.config.encrypted);
  assert(hex(sim::radio.pmk,16)=="f37350bcda2297cfd8692f574c73cf58");
  assert(hex(sim::radio.lmk,16)=="258662ceedc8827f19b457069dbd6504");
  assert(field(run("node status"),"keyed")=="1");
  // Off restarts the running JCMK Node in the clear.
  int begins=sim::radio.begins;assert(has(run("node encrypt off"),"NODE_OK keyed=0\n"));
  assert(sim::radio.begins==begins+1&&!sim::radio.keys&&!sim::radio.config.encrypted&&nodeBusy());
  // A text key turns encryption on.
  assert(run("node key abc")=="NODE_OK key=1\n");assert(sim::radio.keys);
  assert(hex(sim::radio.pmk,16)=="88fc4e98e5092dabb7e33cc7fa51d3ab"&&hex(sim::radio.lmk,16)=="e9c12113b1cf8567c3f19fd38c1fb5e6");
  // Off keeps the key; on again uses it.
  run("node encrypt off");assert(!sim::radio.keys);run("node encrypt on");assert(hex(sim::radio.pmk,16)==sha16("abc_pmk"));
  // Quoted: an empty key, and trailing spaces (the USB line is trimmed, but not inside quotes).
  run("node key \"\"");assert(sim::nvs.at("key").text.empty()&&sim::nvs.at("enc").b);assert(hex(sim::radio.lmk,16)=="258662ceedc8827f19b457069dbd6504");
  run("node key \"abc  \"");assert(sim::nvs.at("key").text=="abc  ");
  assert(hex(sim::radio.pmk,16)=="8a1e2ca9084eec43234c72430e7e9c19"&&hex(sim::radio.lmk,16)=="221ac51d5bb84876ec95a58c0068e49a");
  run("node key  abc");assert(sim::nvs.at("key").text==" abc");assert(hex(sim::radio.pmk,16)=="11d2f7ad9188ed11c64915232807e615");
  run("node key \"clear\"");assert(sim::nvs.at("key").text=="clear"&&sim::nvs.at("enc").b);
  run("node key \"a\"b\"");assert(sim::nvs.at("key").text=="a\"b");
  run("node key \"");assert(sim::nvs.at("key").text=="\"");   // one quote is the key itself
  // Refused keys leave the saved one alone.
  const std::string tooLong(65,'k');
  for(const std::string& bad:{std::string("node key \"")+tooLong+"\"",std::string("node key ")+tooLong,std::string("node key a\tb"),std::string("node key \"a\x7f\"")}){
    assert(has(run(bad.c_str()),"NODE_ERROR key must be"));assert(sim::nvs.at("key").text=="\"");
  }
  assert(has(run((std::string("node key \"")+std::string(64,'k')+"\"").c_str()),"NODE_OK key=1"));
  // Clear removes the key and turns encryption off.
  assert(has(run("node key clear"),"NODE_OK key=0"));assert(!sim::nvs.count("key")&&!sim::nvs.count("enc"));
  assert(!sim::radio.keys&&!sim::radio.config.encrypted&&nodeBusy()&&!nodeKeyed());
  assert(field(run("node config"),"keyed")=="0");
  run("node stop");
  // A key saved before "enc" existed still means encryption on; no key means off.
  sim::nvs.clear();sim::nvs["key"].type='s';sim::nvs["key"].text="old";reboot(ESP_RST_POWERON);
  assert(field(run("node config"),"keyed")=="1");run("node jcmk");assert(hex(sim::radio.pmk,16)==sha16("old_pmk"));run("node stop");
  sim::nvs.clear();reboot(ESP_RST_POWERON);assert(field(run("node config"),"keyed")=="0");
  // An explicit off wins over a stored key.
  run("node key old");run("node encrypt off");reboot(ESP_RST_POWERON);run("node jcmk");assert(!sim::radio.keys);run("node stop");
  assert(has(run("node encrypt"),"NODE_ERROR commands:")&&has(run("node encrypt maybe"),"encrypt on|off"));
}

static void testFailedSwitch(){
  wipe();
  run("node jcmk");assert(savedMode()==2);
  sim::radio.fail=1;
  std::string out=run("node biscuit");
  assert(out=="NODE_STOPPED radar restored\nNODE_ERROR Wi-Fi could not switch to node mode\n");
  assert(!nodeBusy()&&savedMode()==0&&sim::ui=="modes");
  assert(field(run("node status"),"remembered")=="none");
  reboot(ESP_RST_POWERON);assert(!nodeBusy()&&sim::ui=="radar");
  // A failed start with no node running keeps what was remembered.
  sim::nvs["mode"].type='u';sim::nvs["mode"].u=1;sim::radio.fail=1;sim::ui="clock";
  out=run("node jcmk");assert(out=="NODE_ERROR Wi-Fi could not switch to node mode\n");assert(savedMode()==1&&sim::ui=="clock");
  // A settings restart that fails also stops the node and forgets it.
  run("node jcmk");sim::radio.fail=1;out=run("node ble off");
  assert(out=="NODE_OK ble_reports=0\nNODE_STOPPED radar restored\nNODE_ERROR Wi-Fi could not switch to node mode\n");
  assert(!nodeBusy()&&savedMode()==0&&sim::ui=="modes");
}

static void testStartRules(){
  wipe();sim::labStops=sim::reconStops=0;
  // Refused sessions are left as they were.
  stopwatch.running=true;sim::beeper=true;
  assert(has(run("node jcmk"),"NODE_ERROR Stop the stopwatch first"));assert(sim::labStops==0&&sim::reconStops==0&&sim::beeper);
  stopwatch.running=false;sim::wardrive=true;assert(has(run("node jcmk"),"NODE_ERROR Finish the GPS wardrive first"));
  sim::wardrive=false;sim::upload=true;assert(has(run("node jcmk"),"NODE_ERROR Wait for the upload to finish"));
  sim::upload=false;assert(sim::labStops==0&&sim::reconStops==0);
  // A repeating beeper between attempts does not own the radio; it is stopped anyway.
  assert(has(run("node jcmk"),"NODE_OK jcmk"));assert(!sim::beeper&&sim::labStops==1);run("node stop");
  // BLE Lab holding the radio is asked to stop, and the start waits for it.
  sim::labOwned=true;assert(has(run("node biscuit"),"NODE_ERROR BLE Lab is closing; try again"));assert(sim::labClosing&&!nodeBusy());
  sim::labOwned=false;assert(has(run("node biscuit"),"NODE_OK biscuit"));run("node stop");
  // A capture recording to SD is stopped, then waited for while it is finalised.
  sim::sdRecording=true;std::string out=run("node jcmk");
  assert(out=="NODE_ERROR Saving the capture; try again in a moment\n");assert(!sim::sdRecording&&sim::sdClosing&&!nodeBusy()&&savedMode()==0);
  assert(has(run("node jcmk"),"Saving the capture"));sim::sdClosing=false;assert(has(run("node jcmk"),"NODE_OK jcmk"));run("node stop");
  // A capture in RAM just stops.
  sim::ramRecording=true;assert(has(run("node jcmk"),"NODE_OK jcmk"));assert(!sim::ramRecording);run("node stop");
  sim::fox=true;run("node biscuit");assert(!sim::fox);
  // Switching kinds ends the old node without the refusal checks.
  stopwatch.running=true;const int stops=sim::reconStops;assert(has(run("node jcmk"),"NODE_OK jcmk"));assert(sim::reconStops==stops&&nodeKind()==NodeKind::Jcmk);
  stopwatch.running=false;run("node stop");
}

static void testResume(){
  wipe();
  // Power loss and restarts resume the node.
  run("node jcmk");reboot(ESP_RST_POWERON);assert(sim::log=="NODE_STARTED jcmk\nNODE_RESUMED jcmk\n"&&sim::ui=="node"&&nodeKind()==NodeKind::Jcmk);
  // A crash or watchdog reset does not, and the choice is forgotten.
  for(esp_reset_reason_t crash:{ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_CPU_LOCKUP}){
    run("node jcmk");reboot(crash);assert(!nodeBusy()&&savedMode()==0&&sim::ui=="radar");
    assert(has(sim::log,"NODE_RESUME_SKIPPED jcmk reset=")&&has(sim::log,"crashed or a watchdog fired; choose it again from Modes"));
  }
  // One brownout while an engine starts resumes, and so does a second one soon after.
  run("node biscuit");sim::now+=3600000;nodeLoop();
  reboot(ESP_RST_BROWNOUT);assert(has(sim::log,"NODE_RESUMED biscuit"));sim::now+=20000;nodeLoop();
  reboot(ESP_RST_BROWNOUT);assert(has(sim::log,"NODE_RESUMED biscuit"));
  // A third restart in a row, each within a minute of resuming, any reason: skipped.
  sim::now+=59000;nodeLoop();reboot(ESP_RST_SW);
  assert(!nodeBusy()&&savedMode()==0&&sim::ui=="radar");
  assert(sim::log=="NODE_RESUME_SKIPPED biscuit reset=3: it restarted twice within a minute of resuming; check the battery or USB supply, then choose it again from Modes\n");
  reboot(ESP_RST_BROWNOUT);assert(!nodeBusy()&&sim::log.empty());
  // A resumed node that runs a minute starts the count again.
  run("node jcmk");reboot(ESP_RST_BROWNOUT);reboot(ESP_RST_BROWNOUT);assert(nodeBusy());
  sim::now+=60000;nodeLoop();reboot(ESP_RST_BROWNOUT);assert(nodeBusy());reboot(ESP_RST_PWR_GLITCH);assert(nodeBusy());
  reboot(ESP_RST_BROWNOUT);assert(!nodeBusy()&&has(sim::log,"NODE_RESUME_SKIPPED jcmk reset=9: it restarted twice"));
  // Power-on clears RTC memory; whatever it holds then is not a count.
  run("node jcmk");reboot(ESP_RST_BROWNOUT);reboot(ESP_RST_BROWNOUT);reboot(ESP_RST_POWERON);assert(nodeBusy());
  reboot(ESP_RST_BROWNOUT);assert(nodeBusy());
  // RTC memory with the mark but a count no restart could leave is not a count either.
  run("node stop");run("node jcmk");resumeGuard[1]=1000;reboot(ESP_RST_SW);assert(nodeBusy());reboot(ESP_RST_SW);assert(nodeBusy());
  // A node chosen by hand, the other kind or the same one, is not a resume.
  run("node stop");run("node jcmk");reboot(ESP_RST_BROWNOUT);reboot(ESP_RST_BROWNOUT);
  run("node biscuit");reboot(ESP_RST_BROWNOUT);assert(nodeKind()==NodeKind::Biscuit);reboot(ESP_RST_BROWNOUT);assert(nodeKind()==NodeKind::Biscuit);
  run("node biscuit");reboot(ESP_RST_BROWNOUT);assert(nodeKind()==NodeKind::Biscuit);
  // Nor is one stopped and later remembered again.
  reboot(ESP_RST_BROWNOUT);run("node stop");sim::nvs["mode"].u=1;reboot(ESP_RST_SW);assert(nodeBusy());
  // An ended resumed node no longer counts, even before a minute.
  run("node stop");run("node jcmk");reboot(ESP_RST_SW);reboot(ESP_RST_SW);assert(nodeBusy());
  sim::radio.owned=false;nodeLoop();sim::nvs["mode"].u=2;reboot(ESP_RST_SW);assert(nodeBusy());
  // A resume that fails keeps the choice for the next boot and does not count.
  run("node stop");run("node jcmk");
  for(int i=0;i<3;++i){prefs=Preferences{};prefsReady=false;running=NodeKind::None;resumeWatch=false;sim::radio=sim::Radio{};sim::radio.fail=1;sim::log.clear();sim::reason=ESP_RST_SW;nodeBegin();
    assert(sim::log=="NODE_ERROR resume jcmk: Wi-Fi could not switch to node mode\n"&&savedMode()==2&&!nodeBusy());}
  reboot(ESP_RST_SW);assert(has(sim::log,"NODE_RESUMED jcmk"));
  run("node stop");reboot(ESP_RST_SW);assert(sim::log.empty()&&!nodeBusy());
}

static void testBadUsb(){
  wipe();sim::labStops=sim::reconStops=0;
  // A BadUSB script typing on the host: no node starts and nothing else is stopped.
  sim::scriptRunning=true;assert(run("node biscuit")=="NODE_ERROR Stop the BadUSB script first\n");
  assert(!nodeBusy()&&savedMode()==0&&sim::labStops==0&&sim::reconStops==0&&sim::radio.begins==0);sim::scriptRunning=false;
  // USB keyboard mode (BadUSB's one-shot restart): refused too, for either kind.
  sim::keyboardMode=true;
  for(const char* start:{"node biscuit","node jcmk"}){assert(run(start)=="NODE_ERROR Leave USB keyboard mode first\n");assert(!nodeBusy()&&savedMode()==0&&sim::radio.begins==0);}
  assert(!nodeStart(NodeKind::Jcmk)&&!strcmp(nodeError(),"Leave USB keyboard mode first"));
  // A remembered node (a failed resume keeps it) is not resumed by the restart into
  // keyboard mode, and is not forgotten either: the next normal boot resumes it.
  sim::keyboardMode=false;run("node jcmk");assert(nodeBusy());
  sim::keyboardMode=true;reboot(ESP_RST_SW);
  assert(!nodeBusy()&&sim::radio.begins==0&&sim::ui=="radar"&&savedMode()==2&&sim::log=="NODE_RESUME_SKIPPED jcmk: USB keyboard mode\n");
  // EXIT restarts as USB serial; a power cycle or the reset button does the same.
  sim::keyboardMode=false;reboot(ESP_RST_SW);assert(nodeKind()==NodeKind::Jcmk&&sim::ui=="node"&&has(sim::log,"NODE_RESUMED jcmk"));
  sim::keyboardMode=true;reboot(ESP_RST_POWERON);assert(!nodeBusy());sim::keyboardMode=false;reboot(ESP_RST_POWERON);assert(nodeKind()==NodeKind::Jcmk);
  // Skipped keyboard boots do not count as quick restarts of a resumed node.
  for(int i=0;i<4;++i){sim::keyboardMode=true;reboot(ESP_RST_SW);assert(!nodeBusy());sim::keyboardMode=false;reboot(ESP_RST_SW);assert(nodeBusy());sim::now+=60000;nodeLoop();}
  run("node stop");sim::keyboardMode=true;reboot(ESP_RST_SW);assert(sim::log.empty()&&!nodeBusy());sim::keyboardMode=false;
  // A node start frees BadUSB's idle script memory; a refused one leaves it alone.
  const int releases=sim::badusbReleases;sim::scriptRunning=true;run("node jcmk");assert(sim::badusbReleases==releases);sim::scriptRunning=false;
  run("node jcmk");assert(nodeBusy()&&sim::badusbReleases==releases+1);run("node stop");
}

int main(){
  testSha();
  testAllowList();
  testStatusAndStop();
  testSettingsInUse();
  testEncryption();
  testFailedSwitch();
  testStartRules();
  testResume();
  testBadUsb();
  // The key is never printed.
  for(const char* secret:{"s3cr3t-Key","abc  ","old_"})assert(!has(sim::transcript,secret));
  assert(!has(sim::transcript,"Tools > Modes"));
  std::puts("Node session tests passed");
  return 0;
}
