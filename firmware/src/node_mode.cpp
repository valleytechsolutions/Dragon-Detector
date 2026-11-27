// BISCUIT NODE and JCMK Node session: start/stop rules, the remembered choice
// and its restart guard, the optional JCMK encryption and core binding, and the
// USB commands. The radio side is node_radio.inc; the protocol is node_link.h.
#include "node_mode.h"
#include "app.h"
#include "recon.h"
#include "recipes.h"
#include "missions.h"
#include "ble_lab.h"
#include "wardrive.h"
#include "wardrive_upload.h"
#include "badusb.h"
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
char key[65]="";            // JCMK "ESP-NOW key", may be empty; never printed
uint8_t core[6]{};
bool bound=false,bleReports=true,encryption=false;     // saved; encryption is the core's "Use Encryption"
bool linkBound=false,linkKeyed=false,linkBle=false;    // what the running link uses

// Restart guard. RTC memory keeps its value through every reset except power-on,
// so it counts the restarts in a row that came soon after a resume.
RTC_NOINIT_ATTR volatile uint32_t resumeGuard[2];
constexpr uint32_t ResumeGuardMagic=0x4e4f4445;       // "NODE"
constexpr uint32_t ResumeQuickLimit=2;               // quick restarts in a row before the next resume is skipped
constexpr uint32_t ResumeSettledMs=60000;            // a resumed node that ran this long is not a quick restart
bool resumeWatch=false;
uint32_t resumedAt=0;
uint32_t quickRestarts(){return resumeGuard[0]==ResumeGuardMagic&&resumeGuard[1]<=ResumeQuickLimit?resumeGuard[1]:0;}
void setQuickRestarts(uint32_t n){resumeGuard[0]=ResumeGuardMagic;resumeGuard[1]=n;}
void settle(){resumeWatch=false;setQuickRestarts(0);}

void say(const char* text){snprintf(error,sizeof(error),"%s",text);}
void load(){
  if(prefsReady)return;
  prefsReady=prefs.begin("dd-node",false);
  if(!prefsReady)return;
  bound=prefs.getBytesLength("core")==6&&prefs.getBytes("core",core,6)==6;
  bleReports=prefs.getBool("ble",true);
  const bool hasKey=prefs.isKey("key");
  String saved=hasKey?prefs.getString("key",""):String();snprintf(key,sizeof(key),"%s",saved.c_str());
  encryption=prefs.getBool("enc",hasKey);   // settings saved before "enc" existed: a stored key meant encryption on
  saved=prefs.getString("id","");snprintf(identity,sizeof(identity),"%s",saved.c_str());
}
NodeKind remembered(){
  if(!prefsReady)return NodeKind::None;
  const uint8_t mode=prefs.getUChar("mode",0);
  return mode==1?NodeKind::Biscuit:mode==2?NodeKind::Jcmk:NodeKind::None;
}
void remember(NodeKind kind){if(prefsReady)prefs.putUChar("mode",uint8_t(kind));}
// JustCallMeKoko derives both ESP-NOW keys from the text key the same way,
// the empty text included (its "Use Encryption" with no ENOW Key typed).
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
bool printable(const String& text,size_t limit,bool empty=false){
  if((!empty&&!text.length())||text.length()>limit)return false;
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
  const bool keyed=kind==NodeKind::Jcmk&&encryption;
  if(keyed){derive("_pmk",pmk);derive("_lmk",lmk);}
  config.encrypted=keyed;
  char why[64]="";
  const bool ok=radioNodeBegin(config,keyed?pmk:nullptr,keyed?lmk:nullptr,why,sizeof(why));
  memset(pmk,0,sizeof(pmk));memset(lmk,0,sizeof(lmk));
  if(!ok){say(why);return false;}
  running=kind;startedAt=millis();
  linkBound=bound;linkKeyed=keyed;linkBle=kind==NodeKind::Jcmk&&bleReports;
  return true;
}

bool startSession(NodeKind kind){
  load();*error=0;
  if(kind==NodeKind::None){nodeStop();return true;}
#if BOARD_RADIO_AP_SURVEY_ONLY && !defined(BOARD_WAVESHARE_P4_KNOB)
  say("Node mode requires ESP-NOW. This radio supports Wi-Fi surveys.");
  return false;
#endif
  if(radioNodeOwned()&&running==kind)return true;
  // BadUSB keyboard mode is a one-shot session without USB serial, and a script types
  // only while nothing else runs. Neither shares the device with a node.
  if(badusbKeyboardMode()){say("Leave USB keyboard mode first");return false;}
  if(badusbStatus().running){say("Stop the BadUSB script first");return false;}
  const bool switching=radioNodeOwned();
  if(!switching){
    // Sessions that must finish on their own terms are refused, never cut off.
    if(stopwatch.running){say("Stop the stopwatch first");return false;}
    if(wardriveBusy()){say("Finish the GPS wardrive first");return false;}
    if(wardriveUploadBusy()){say("Wait for the upload to finish");return false;}
    // Everything else a node replaces ends here: BLE Lab (a repeating beeper between
    // its attempts too), recipes, missions, radio tools and a capture still recording.
    bleLabStop();recipeStop();
#ifndef BOARD_RADAR_LITE
    missionStop();
#endif
    reconStop();setCapture(false);audioStop();
    badusbRelease();   // BadUSB's idle 8 KB script buffer; it is read again when BadUSB opens
    if(foxTransmit())setFoxTransmit(false);
    // Only a capture file still being finalised, or a Lab connection still closing, is waited for.
    if(reconStorageBusy()){say("Saving the capture; try again in a moment");return false;}
    if(bleLabOwnsRadio()){say("BLE Lab is closing; try again");return false;}
  }else radioNodeEnd();     // switching between the two node kinds
  if(!startRadio(kind)){
    running=NodeKind::None;
    // A failed switch has already ended the old node and given the radar its radios back.
    if(switching){remember(NodeKind::None);Serial.println("NODE_STOPPED radar restored");}
    return false;
  }
  remember(kind);
  Serial.printf("NODE_STARTED %s\n",kindText(kind));
  return true;
}
}

bool nodeBusy(){return radioNodeOwned();}
NodeKind nodeKind(){return radioNodeOwned()?running:NodeKind::None;}
const char* nodeError(){return error;}
const dragon::node::Status& nodeStatus(){return radioNodeStatus();}
uint32_t nodeElapsedMs(){return radioNodeOwned()?millis()-startedAt:0;}
bool nodeKeyed(){return radioNodeOwned()&&linkKeyed;}
bool nodeBleReports(){return radioNodeOwned()&&linkBle;}

bool nodeStart(NodeKind kind){settle();return startSession(kind);}   // a node chosen by hand is not a resume

void nodeStop(){
  load();settle();
  const bool was=radioNodeOwned();
  radioNodeEnd();running=NodeKind::None;
  remember(NodeKind::None);
  if(was)Serial.println("NODE_STOPPED radar restored");
}

#ifdef BOARD_WAVESHARE_P4_KNOB
bool radioDriversUp();
static NodeKind pendingResume=NodeKind::None;
#endif
void nodeBegin(){
  load();
  const NodeKind kind=remembered();
  const uint32_t quick=quickRestarts();setQuickRestarts(0);
  if(kind==NodeKind::None)return;
#if BOARD_RADIO_AP_SURVEY_ONLY && !defined(BOARD_WAVESHARE_P4_KNOB)
  Serial.println("NODE_RESUME_SKIPPED ESP-NOW unavailable on this radio");
  return;
#endif
  // The restart into BadUSB keyboard mode never resumes a node. The choice is kept, so
  // the node resumes after EXIT, the reset button or a power cycle.
  if(badusbKeyboardMode()){Serial.printf("NODE_RESUME_SKIPPED %s: USB keyboard mode\n",kindText(kind));return;}
  // A node must not trap the device in a restart loop: not after a crash or a watchdog
  // reset, and not when it restarts again soon after each resume, whatever the reason
  // (often a supply that sags when the node transmits). One quick restart, such as an
  // engine start in a car, still resumes.
  const esp_reset_reason_t reason=esp_reset_reason();
  const bool crashed=reason==ESP_RST_PANIC||reason==ESP_RST_INT_WDT||reason==ESP_RST_TASK_WDT||reason==ESP_RST_WDT||reason==ESP_RST_CPU_LOCKUP;
  if(crashed||quick>=ResumeQuickLimit){
    remember(NodeKind::None);
    Serial.printf("NODE_RESUME_SKIPPED %s reset=%d: %s\n",kindText(kind),int(reason),crashed?"the device crashed or a watchdog fired; choose it again from Modes"
      :"it restarted twice within a minute of resuming; check the battery or USB supply, then choose it again from Modes");
    return;
  }
  setQuickRestarts(quick+1);   // counted before the radio starts, which is when a weak supply gives way
#ifdef BOARD_WAVESHARE_P4_KNOB
  // The knob's radio chip starts on its own task: resume once it is up.
  if(!radioDriversUp()){pendingResume=kind;Serial.printf("NODE_RESUME_WAITING %s: the radio chip is starting\n",kindText(kind));return;}
#endif
  if(startSession(kind)){resumeWatch=true;resumedAt=millis();uiShow("node");Serial.printf("NODE_RESUMED %s\n",kindText(kind));}
  else{setQuickRestarts(0);Serial.printf("NODE_ERROR resume %s: %s\n",kindText(kind),error);}
}

void nodeLoop(){
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(pendingResume!=NodeKind::None&&!p4CoprocBusy()){
    // The radio chip has finished starting, one way or the other. Try once:
    // startSession fails cleanly if the radio did not come up, so this never
    // waits forever (e.g. Wi-Fi up but BLE down leaves no scanner).
    const NodeKind kind=pendingResume;pendingResume=NodeKind::None;
    if(startSession(kind)){resumeWatch=true;resumedAt=millis();uiShow("node");Serial.printf("NODE_RESUMED %s\n",kindText(kind));}
    else{setQuickRestarts(0);Serial.printf("NODE_ERROR resume %s: %s (radio %s)\n",kindText(kind),error,p4CoprocStateName());}
  }
#endif
  // A resumed node that has run a minute, or has ended, no longer counts as a quick restart.
  if(resumeWatch&&(!radioNodeOwned()||millis()-resumedAt>=ResumeSettledMs))settle();
}

bool nodeAllowsCommand(const String& c){
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(c=="p4 status"||c=="radio chip status"||c=="p4 radio status")return true;
#endif
  // Node commands, the clock, a restart and read-only queries, USB Studio's connect queries among them.
  static const char* const exact[]={"node","status","memory status","loop diagnostics","scan status","ui status","touch status",
    "amoled status","143c status","185b status","rotary status","watcher status","dial status","display status",
    "badge info","screenshot","screen node","clock status","clock hardware","orientation status",
    "radar status","tone settings","theme info","system restart"};
  for(const char* e:exact)if(c==e)return true;
  return c.startsWith("node ")||c.startsWith("time ")||c.startsWith("clock offset ")||c.startsWith("tone get ")||c.startsWith("theme read ");
}

bool nodeCommand(const String& c){
  if(c!="node"&&!c.startsWith("node "))return false;
  load();
  if(c=="node status"||c=="node"){
    const auto& s=nodeStatus();char coreMac[18]="";
    if(s.linked)macText(s.coreMac,coreMac);
    char plan[48]="";size_t at=0;
    for(uint8_t i=0;i<s.planCount&&at+4<sizeof(plan);++i)at+=size_t(snprintf(plan+at,sizeof(plan)-at,i?",%u":"%u",s.plan[i]));
    // bound, keyed and ble_reports: what the running link uses. node config shows the saved settings.
    Serial.printf("NODE_STATUS mode=%s active=%u phase=%s core=%s linked=%u core_mac=%s core_rssi=%d channel=%u registered=%u assigned=%u own_plan=%u plan=%s dwell_ms=%u role=%u node=%u/%u "
      "wifi=%lu ble=%lu sent=%lu failed=%lu dropped=%lu acked=%lu submitted=%lu cycles=%lu scans=%lu scan_errors=%lu requests=%lu links=%lu losses=%lu heartbeats=%lu core_age_ms=%lu "
      "other=%s broadcast=%u bound=%u keyed=%u ble_reports=%u rx_drops=%lu heard=%lu heard_core=%lu waiting=%u elapsed_ms=%lu remembered=%s heap=%u min_heap=%u largest=%u\n",
      kindText(nodeKind()),nodeBusy(),dragon::node::phaseName(s.phase),dragon::node::coreName(s.core),s.linked,s.linked?coreMac:"-",s.coreRssi,s.channel,
      s.registered,s.assigned,s.ownPlan,at?plan:"-",s.dwellMs,s.role,s.nodeIndex,s.nodeCount,
      (unsigned long)s.wifi,(unsigned long)s.ble,(unsigned long)s.sent,(unsigned long)s.failed,(unsigned long)s.dropped,(unsigned long)s.acked,
      (unsigned long)s.submitted,(unsigned long)s.cycles,(unsigned long)s.scans,(unsigned long)s.scanErrors,(unsigned long)s.requests,(unsigned long)s.links,(unsigned long)s.losses,
      (unsigned long)s.heartbeats,(unsigned long)s.coreAgeMs,dragon::node::coreName(s.other),s.broadcast,nodeBusy()&&linkBound,nodeKeyed(),nodeBleReports(),
      (unsigned long)radioNodeReceiveDrops(),(unsigned long)s.heard,(unsigned long)s.heardCore,unsigned(s.waiting),(unsigned long)nodeElapsedMs(),kindText(remembered()),
      unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
    return true;
  }
  if(c=="node biscuit"||c=="node jcmk"){
    const NodeKind kind=c=="node biscuit"?NodeKind::Biscuit:NodeKind::Jcmk;
    const bool was=nodeBusy();
    if(nodeStart(kind)){uiShow("node");Serial.printf("NODE_OK %s\n",kindText(kind));}
    else{Serial.printf("NODE_ERROR %s\n",error);if(was)uiShow("modes");}   // a failed switch has ended the old node
    return true;
  }
  // With no node running this only forgets the choice; the screen stays where it is.
  if(c=="node stop"){const bool was=nodeBusy();nodeStop();if(was)uiShow("modes");Serial.println("NODE_OK stopped");return true;}
  // A diagnostic for field logs: every node frame heard and sent, until `node trace off` or a restart.
  if(c=="node trace on"||c=="node trace off"){radioNodeTrace(c=="node trace on");Serial.printf("NODE_OK trace=%u\n",unsigned(radioNodeTracing()));return true;}
  // Settings take effect at the next start; a running JCMK Node restarts at once.
  auto apply=[&](bool jcmkOnly){
    if(!radioNodeOwned()||(jcmkOnly&&running!=NodeKind::Jcmk))return;
    const NodeKind kind=running;radioNodeEnd();running=NodeKind::None;
    if(!startRadio(kind)){remember(NodeKind::None);Serial.println("NODE_STOPPED radar restored");Serial.printf("NODE_ERROR %s\n",error);uiShow("modes");}
  };
  // Encryption follows the core's "Use Encryption", apart from the key text: on with
  // an empty key matches a core where no ENOW Key was ever typed.
  if(c=="node encrypt on"||c=="node encrypt off"){
    encryption=c=="node encrypt on";if(prefsReady)prefs.putBool("enc",encryption);
    Serial.printf("NODE_OK keyed=%u\n",encryption);apply(true);return true;
  }
  if(c=="node key clear"){key[0]=0;encryption=false;if(prefsReady){prefs.remove("key");prefs.remove("enc");}Serial.println("NODE_OK key=0");apply(true);return true;}
  if(c.startsWith("node key ")){
    // In double quotes the key may be empty or end in spaces, which the USB line otherwise loses.
    String text=c.substring(9);
    const bool quoted=text.length()>=2&&text[0]=='"'&&text[text.length()-1]=='"';
    if(quoted)text=text.substring(1,text.length()-1);
    if(!printable(text,64,quoted)){Serial.println("NODE_ERROR key must be 1 to 64 printable characters, or 0 to 64 in double quotes");return true;}
    snprintf(key,sizeof(key),"%s",text.c_str());encryption=true;
    if(prefsReady){prefs.putString("key",key);prefs.putBool("enc",true);}
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
      kindText(remembered()),bound,text,encryption,bleReports,*identity?identity:defaultIdentity(),own);
    return true;
  }
  Serial.println("NODE_ERROR commands: node status|config|biscuit|jcmk|stop|trace on|off|key <text>|key \"<text>\"|key clear|encrypt on|off|bind <mac>|bind off|identity <text>|identity default|ble on|off");
  return true;
}
