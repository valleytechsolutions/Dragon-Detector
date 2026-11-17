// BISCUIT NODE and JCMK Node session: start/stop rules, the remembered choice,
// the optional JCMK key and core binding, and the USB commands. The radio side
// is node_radio.inc; the protocol is node_link.h.
#include "node_mode.h"
#include "app.h"
#include "recon.h"
#include "recipes.h"
#include "missions.h"
#include "ble_lab.h"
#include "wardrive.h"
#include "wardrive_upload.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>

namespace {
Preferences prefs;
bool prefsReady=false;
NodeKind running=NodeKind::None;
uint32_t startedAt=0;
char error[64]="";
char identity[48]="";
char key[65]="";            // JCMK "ESP-NOW key"; never printed
uint8_t core[6]{};
bool bound=false,bleReports=true;

void say(const char* text){snprintf(error,sizeof(error),"%s",text);}
void load(){
  if(prefsReady)return;
  prefsReady=prefs.begin("dd-node",false);
  if(!prefsReady)return;
  bound=prefs.getBytesLength("core")==6&&prefs.getBytes("core",core,6)==6;
  bleReports=prefs.getBool("ble",true);
  String saved=prefs.getString("key","");snprintf(key,sizeof(key),"%s",saved.c_str());
  saved=prefs.getString("id","");snprintf(identity,sizeof(identity),"%s",saved.c_str());
}
NodeKind remembered(){
  if(!prefsReady)return NodeKind::None;
  const uint8_t mode=prefs.getUChar("mode",0);
  return mode==1?NodeKind::Biscuit:mode==2?NodeKind::Jcmk:NodeKind::None;
}
void remember(NodeKind kind){if(prefsReady)prefs.putUChar("mode",uint8_t(kind));}
// JustCallMeKoko derives both ESP-NOW keys from the text key the same way.
void derive(const char* suffix,uint8_t* out){
  uint8_t hash[32];String text=String(key)+suffix;
  mbedtls_sha256(reinterpret_cast<const unsigned char*>(text.c_str()),text.length(),hash,0);
  memcpy(out,hash,16);memset(hash,0,sizeof(hash));
}
const char* defaultIdentity(){return "DragonDetector:" DRAGON_VERSION;}
const char* kindText(NodeKind kind){return kind==NodeKind::Biscuit?"biscuit":kind==NodeKind::Jcmk?"jcmk":"none";}
bool parseMac(const String& text,uint8_t* mac){
  unsigned v[6];char tail;
  if(sscanf(text.c_str(),"%2x:%2x:%2x:%2x:%2x:%2x%c",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&tail)!=6)return false;
  for(unsigned i=0;i<6;++i)mac[i]=uint8_t(v[i]);
  return !(mac[0]&1);       // a core is one device, never a group address
}
bool printable(const String& text,size_t limit){
  if(!text.length()||text.length()>limit)return false;
  for(size_t i=0;i<text.length();++i){const char c=text[i];if(c<0x20||c==0x7f)return false;}
  return true;
}

bool startRadio(NodeKind kind){
  dragon::node::Config config;
  config.family=kind==NodeKind::Biscuit?dragon::node::Family::Biscuit:dragon::node::Family::Jcmk;
  config.identity=*identity?identity:defaultIdentity();
  config.maxChannel=11;     // the radio's regulatory setting: channels 1 to 11
  config.ble=bleReports;
  config.bound=bound;memcpy(config.boundCore,core,6);
  uint8_t pmk[16],lmk[16];
  const bool keyed=kind==NodeKind::Jcmk&&*key;
  if(keyed){derive("_pmk",pmk);derive("_lmk",lmk);}
  config.encrypted=keyed;
  char why[64]="";
  const bool ok=radioNodeBegin(config,keyed?pmk:nullptr,keyed?lmk:nullptr,why,sizeof(why));
  memset(pmk,0,sizeof(pmk));memset(lmk,0,sizeof(lmk));
  if(!ok){say(why);return false;}
  running=kind;startedAt=millis();return true;
}
}

bool nodeBusy(){return radioNodeOwned();}
NodeKind nodeKind(){return radioNodeOwned()?running:NodeKind::None;}
const char* nodeError(){return error;}
const dragon::node::Status& nodeStatus(){return radioNodeStatus();}
uint32_t nodeElapsedMs(){return radioNodeOwned()?millis()-startedAt:0;}
bool nodeKeyed(){return radioNodeOwned()&&running==NodeKind::Jcmk&&*key;}
bool nodeBleReports(){return running==NodeKind::Jcmk&&bleReports;}

bool nodeStart(NodeKind kind){
  load();*error=0;
  if(kind==NodeKind::None){nodeStop();return true;}
  if(radioNodeOwned()&&running==kind)return true;
  if(!radioNodeOwned()){
    // Sessions that must finish on their own terms are refused, never cut off.
    if(stopwatch.running){say("Stop the stopwatch first");return false;}
    if(wardriveBusy()){say("Finish the GPS wardrive first");return false;}
    if(wardriveUploadBusy()){say("Wait for the upload to finish");return false;}
    if(reconStorageBusy()){say("Wait for the save to finish");return false;}
    if(bleLabOwnsRadio()){bleLabStop();say("BLE Lab is closing; try again");return false;}
    recipeStop();
#ifndef BOARD_RADAR_LITE
    missionStop();
#endif
    reconStop();setCapture(false);audioStop();
    if(foxTransmit())setFoxTransmit(false);
  }else radioNodeEnd();     // switching between the two node kinds
  if(!startRadio(kind)){running=NodeKind::None;return false;}
  remember(kind);
  Serial.printf("NODE_STARTED %s\n",kindText(kind));
  return true;
}

void nodeStop(){
  load();
  const bool was=radioNodeOwned();
  radioNodeEnd();running=NodeKind::None;
  remember(NodeKind::None);
  if(was)Serial.println("NODE_STOPPED radar restored");
}

void nodeBegin(){
  load();
  const NodeKind kind=remembered();
  if(kind==NodeKind::None)return;
  // A node that crashed the device must not trap it in a restart loop.
  const esp_reset_reason_t reason=esp_reset_reason();
  if(reason==ESP_RST_PANIC||reason==ESP_RST_INT_WDT||reason==ESP_RST_TASK_WDT||reason==ESP_RST_WDT||reason==ESP_RST_CPU_LOCKUP){
    remember(NodeKind::None);
    Serial.printf("NODE_RESUME_SKIPPED %s reset=%d; choose it again from Tools > Modes\n",kindText(kind),int(reason));
    return;
  }
  if(nodeStart(kind)){uiShow("node");Serial.printf("NODE_RESUMED %s\n",kindText(kind));}
  else Serial.printf("NODE_ERROR resume %s: %s\n",kindText(kind),error);
}

bool nodeAllowsCommand(const String& c){
  static const char* const exact[]={"status","memory status","loop diagnostics","scan status","ui status","touch status",
    "amoled status","143c status","185b status","rotary status","watcher status","dial status","display status",
    "badge info","screenshot","screen node","clock status","clock hardware","orientation status","system restart"};
  for(const char* e:exact)if(c==e)return true;
  return c.startsWith("node ")||c.startsWith("time ")||c.startsWith("clock offset ");
}

bool nodeCommand(const String& c){
  if(c!="node"&&!c.startsWith("node "))return false;
  load();
  if(c=="node status"||c=="node"){
    const auto& s=nodeStatus();char coreMac[18]="";
    if(s.linked)macText(s.coreMac,coreMac);
    char plan[48]="";size_t at=0;
    for(uint8_t i=0;i<s.planCount&&at+4<sizeof(plan);++i)at+=size_t(snprintf(plan+at,sizeof(plan)-at,i?",%u":"%u",s.plan[i]));
    Serial.printf("NODE_STATUS mode=%s active=%u phase=%s core=%s linked=%u core_mac=%s core_rssi=%d channel=%u assigned=%u own_plan=%u plan=%s dwell_ms=%u role=%u node=%u/%u "
      "wifi=%lu ble=%lu sent=%lu failed=%lu dropped=%lu acked=%lu cycles=%lu scans=%lu scan_errors=%lu requests=%lu links=%lu losses=%lu heartbeats=%lu core_age_ms=%lu "
      "other=%s broadcast=%u bound=%u keyed=%u ble_reports=%u rx_drops=%lu elapsed_ms=%lu remembered=%s heap=%u min_heap=%u largest=%u\n",
      kindText(nodeKind()),nodeBusy(),dragon::node::phaseName(s.phase),dragon::node::coreName(s.core),s.linked,s.linked?coreMac:"-",s.coreRssi,s.channel,
      s.assigned,s.ownPlan,at?plan:"-",s.dwellMs,s.role,s.nodeIndex,s.nodeCount,
      (unsigned long)s.wifi,(unsigned long)s.ble,(unsigned long)s.sent,(unsigned long)s.failed,(unsigned long)s.dropped,(unsigned long)s.acked,
      (unsigned long)s.cycles,(unsigned long)s.scans,(unsigned long)s.scanErrors,(unsigned long)s.requests,(unsigned long)s.links,(unsigned long)s.losses,
      (unsigned long)s.heartbeats,(unsigned long)s.coreAgeMs,dragon::node::coreName(s.other),s.broadcast,bound,*key!=0,bleReports,
      (unsigned long)radioNodeReceiveDrops(),(unsigned long)nodeElapsedMs(),kindText(remembered()),
      unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
    return true;
  }
  if(c=="node biscuit"||c=="node jcmk"){
    const NodeKind kind=c=="node biscuit"?NodeKind::Biscuit:NodeKind::Jcmk;
    if(nodeStart(kind)){uiShow("node");Serial.printf("NODE_OK %s\n",kindText(kind));}
    else Serial.printf("NODE_ERROR %s\n",error);
    return true;
  }
  if(c=="node stop"){nodeStop();uiShow("modes");Serial.println("NODE_OK stopped");return true;}
  // Settings take effect at the next start; a running JCMK Node restarts at once.
  auto apply=[&](bool jcmkOnly){
    if(!radioNodeOwned()||(jcmkOnly&&running!=NodeKind::Jcmk))return;
    const NodeKind kind=running;radioNodeEnd();running=NodeKind::None;
    if(!startRadio(kind)){remember(NodeKind::None);Serial.printf("NODE_ERROR %s\n",error);uiShow("modes");}
  };
  if(c=="node key clear"){key[0]=0;if(prefsReady)prefs.remove("key");Serial.println("NODE_OK key=0");apply(true);return true;}
  if(c.startsWith("node key ")){
    const String text=c.substring(9);
    if(!printable(text,64)){Serial.println("NODE_ERROR key must be 1 to 64 printable characters");return true;}
    snprintf(key,sizeof(key),"%s",text.c_str());if(prefsReady)prefs.putString("key",key);
    Serial.println("NODE_OK key=1");apply(true);return true;
  }
  if(c=="node bind off"){bound=false;memset(core,0,6);if(prefsReady)prefs.remove("core");Serial.println("NODE_OK bound=0");apply(false);return true;}
  if(c.startsWith("node bind ")){
    uint8_t mac[6];
    if(!parseMac(c.substring(10),mac)){Serial.println("NODE_ERROR bind needs a core address like 24:6F:28:10:20:30");return true;}
    memcpy(core,mac,6);bound=true;if(prefsReady)prefs.putBytes("core",core,6);
    char text[18];macText(core,text);Serial.printf("NODE_OK bound=1 core=%s\n",text);apply(false);return true;
  }
  if(c=="node identity default"){identity[0]=0;if(prefsReady)prefs.remove("id");Serial.printf("NODE_OK identity=%s\n",defaultIdentity());apply(false);return true;}
  if(c.startsWith("node identity ")){
    const String text=c.substring(14);
    if(!printable(text,40)){Serial.println("NODE_ERROR identity must be 1 to 40 printable characters");return true;}
    snprintf(identity,sizeof(identity),"%s",text.c_str());if(prefsReady)prefs.putString("id",identity);
    Serial.printf("NODE_OK identity=%s\n",identity);apply(false);return true;
  }
  if(c=="node ble on"||c=="node ble off"){
    bleReports=c=="node ble on";if(prefsReady)prefs.putBool("ble",bleReports);
    Serial.printf("NODE_OK ble_reports=%u\n",bleReports);apply(true);return true;
  }
  if(c=="node config"){
    char text[18]="-";if(bound)macText(core,text);
    uint8_t mac[6];radioNodeMac(mac);char own[18];macText(mac,own);
    Serial.printf("NODE_CONFIG remembered=%s bound=%u core=%s keyed=%u ble_reports=%u identity=%s own_mac=%s\n",
      kindText(remembered()),bound,text,*key!=0,bleReports,*identity?identity:defaultIdentity(),own);
    return true;
  }
  Serial.println("NODE_ERROR commands: node status|config|biscuit|jcmk|stop|key <text>|key clear|bind <mac>|bind off|identity <text>|identity default|ble on|off");
  return true;
}
