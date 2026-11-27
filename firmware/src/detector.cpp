#include "app.h"
#include "background_radio.h"
#include "flash_budget.h"
#include "recon.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_timer.h>
#include <time.h>
#include <sys/time.h>

dragon::DetectorSettings detector;
bool axonPriority=true;
uint32_t axonAlertCount=0;
HistoryRecord* historyRecords=nullptr;
size_t historyCount=0;
bool historyReady=false;
static Preferences prefs;
// Alert ledgers: outside Contact so eviction cannot reset a visit.
static dragon::VisitLedger visits;
static dragon::IncidentLedger incidents;
#ifdef BOARD_RADAR_LITE
static constexpr unsigned VISIT_LIMIT=256;
#else
static constexpr unsigned VISIT_LIMIT=512,VISIT_FALLBACK=128;
#endif
static uint16_t alertEpoch=0;
uint16_t alertRevision(){return uint16_t(alertEpoch*31u+unsigned(reconMode()));}
void visitTouch(const dragon::DeviceKey& key,uint32_t now){visits.touch(key,now);}
void visitExpire(uint32_t now){visits.goneMs=dragon::visitGoneMs(detector.autoQuiet);visits.expire(now);incidents.expire(now);}
void visitListen(bool wifi,bool ble,uint32_t now){visits.listen(0,wifi,now);visits.listen(1,ble,now);incidents.listen(0,wifi,now);incidents.listen(1,ble,now);}
// Running visits survive deep sleep and software resets in RTC memory; the
// system clock keeps counting through both. A power-on reset fails the check.
static constexpr unsigned VISIT_KEEP=128;static constexpr uint32_t VISIT_KEEP_MAGIC=0x56495331;
struct VisitKeep {uint32_t magic,count,crc;uint64_t savedUs;dragon::VisitRecord records[VISIT_KEEP];};
RTC_NOINIT_ATTR static VisitKeep visitKeep;
static uint64_t systemUs(){timeval tv;gettimeofday(&tv,nullptr);return uint64_t(tv.tv_sec)*1000000ull+tv.tv_usec;}
static uint32_t visitKeepCrc(){return dragon::crc32((const uint8_t*)&visitKeep.savedUs,sizeof(visitKeep.savedUs)+visitKeep.count*sizeof(dragon::VisitRecord))^visitKeep.count;}
void visitsPersist(){visitKeep.magic=0;visitKeep.count=visits.save(visitKeep.records,VISIT_KEEP,millis());visitKeep.savedUs=systemUs();visitKeep.crc=visitKeepCrc();visitKeep.magic=VISIT_KEEP_MAGIC;}
static void visitsRestore(){
  visits.goneMs=dragon::visitGoneMs(detector.autoQuiet);const uint64_t now=systemUs();
  if(visitKeep.magic==VISIT_KEEP_MAGIC&&visitKeep.count<=VISIT_KEEP&&visitKeep.crc==visitKeepCrc()&&now>=visitKeep.savedUs&&now-visitKeep.savedUs<3600000000ull)
    visits.restore(visitKeep.records,visitKeep.count,uint32_t((now-visitKeep.savedUs)/1000),millis());
  visitKeep.magic=0;
}
static void visitsBegin(){
#ifdef BOARD_RADAR_LITE
  static dragon::Visit table[VISIT_LIMIT];visits.begin(table,VISIT_LIMIT);
#else
  // Internal RAM only if PSRAM fails, so the fallback costs nothing otherwise.
  auto* table=(dragon::Visit*)ps_calloc(VISIT_LIMIT,sizeof(dragon::Visit));unsigned n=VISIT_LIMIT;
  if(!table){table=(dragon::Visit*)heap_caps_calloc(VISIT_FALLBACK,sizeof(dragon::Visit),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);n=VISIT_FALLBACK;}
  visits.begin(table,table?n:0);
#endif
}
struct Rule {dragon::DeviceKey key;uint8_t flags=0;};
static Rule rules[RULE_LIMIT];
static uint32_t bootId=0,sequence=0;
static uint32_t persistedAt[HISTORY_LIMIT]{};
static bool dirty[HISTORY_LIMIT]{};
static uint8_t journal=0;
static size_t journalBytes=0;
static uint32_t lastWrite=0;
static size_t writeCursor=0;
static const char* journals[]={"/detections-0.dat","/detections-1.dat"};
static constexpr uint32_t HISTORY_MAGIC=0x34484444;
static uint32_t checksum(const HistoryRecord& r){return dragon::crc32((const uint8_t*)&r,offsetof(HistoryRecord,checksum))^0xffffffffu;}
// IDF's RTC + high-resolution system clock retains its epoch across powered
// software resets. A power-on reset clears it; never invent elapsed offline time.
// Stopwatch measurements remain on esp_timer_get_time(), independent of UTC.
uint32_t wallTime(){time_t now=time(nullptr);return dragon::validEpoch(now)?uint32_t(now):0;}
void setWallTime(uint32_t epoch){if(!dragon::validEpoch(epoch))return;timeval tv{time_t(epoch),0};settimeofday(&tv,nullptr);}
dragon::DeviceKey contactKey(const Contact& c){return keyOf(c);}
uint8_t deviceRules(const dragon::DeviceKey& key){for(auto& r:rules)if(r.flags&&r.key==key)return r.flags;return 0;}
bool ruleAt(size_t i,dragon::DeviceKey& key,uint8_t& flags){if(i>=RULE_LIMIT||!rules[i].flags)return false;key=rules[i].key;flags=rules[i].flags;return true;}
bool toggleRule(const dragon::DeviceKey& key,DeviceRule flag){
  Rule* slot=nullptr;for(auto& r:rules){if(r.flags&&r.key==key){slot=&r;break;}if(!r.flags&&!slot)slot=&r;}
  if(!slot)return false;
  Rule before=*slot;slot->key=key;slot->flags^=flag;
  Rule saved[RULE_LIMIT];memcpy(saved,rules,sizeof(saved));for(auto& r:saved)r.flags&=RuleIgnore|RuleWatch;
  if(prefs.putBytes("rules",saved,sizeof(saved))!=sizeof(saved)){*slot=before;return false;}
  ++alertEpoch;return true;
}
void detectorSave(){
  ++alertEpoch; // present devices get one fresh decision; the ledgers keep repeats silent
  prefs.putBool("adult-mode",detector.enabled(Family::Adult));
#ifndef BOARD_RADAR_LITE
  prefs.putUInt("schema",1);prefs.putBytes("settings",&detector,sizeof(detector));
  prefs.putBool("sound",alertSound);prefs.putBool("vibrate",alertVibration);
  prefs.putBool("active-ble",activeBle);
  prefs.putBool("axon-priority",axonPriority);
#endif
}
void setAdultMode(bool enabled){
  detector.families=dragon::adultOptInMask(detector.families,enabled);
  detectorSave();
  // No previous opt-in classification may survive disabling the setting.
  if(!enabled)for(size_t i=0;contacts.data&&i<MAX_CONTACTS;++i)if(contacts[i].used&&contacts[i].match.family==Family::Adult){
    if(trackedContact==int(i))trackContact(-1);
    contacts[i]={};
  }
}
void displayMac(const uint8_t* mac,char* out){macText(mac,out);if(detector.privacy)strcpy(out+9,"XX:XX:XX");}
String displayName(const char* name){if(!detector.privacy)return String(name);return strlen(name)>3?String(name).substring(0,3)+"...":String(name);}
static int historySlot(const dragon::DeviceKey& key,uint8_t family){
  for(size_t i=0;i<historyCount;++i)if(historyRecords[i].key==key&&historyRecords[i].family==family)return int(i);
  if(historyCount<HISTORY_LIMIT)return int(historyCount++);
  size_t oldest=0;for(size_t i=1;i<historyCount;++i)if(int32_t(historyRecords[i].sequence-historyRecords[oldest].sequence)<0)oldest=i;
  historyRecords[oldest]={};dirty[oldest]=false;persistedAt[oldest]=0;return int(oldest);
}
void detectorBegin(){
  prefs.begin("dd-detect",false);visitsBegin();
  nearby.on=prefs.getBool("near",true);{int v=prefs.getChar("near-dbm",-70);nearby.enter=v<-85||v>-55?-70:v;}
#ifdef BOARD_RADAR_LITE
  // Fixed passive radar. Never import old full-UI filters, ignore rules or
  // display timers. No persistent history allocation on Lite; only explicit settings are saved.
  detector=dragon::DetectorSettings{};detector.history=false;
  detector.families=dragon::adultOptInMask(detector.families,prefs.getBool("adult-mode",false));
  activeBle=false;axonPriority=true;
#ifndef BOARD_M5DIAL
  alertSound=alertVibration=false;
#endif
  Serial.println("LITE detectors=all axon_priority=1 passive=1 history=off");
  visitsRestore();return;
#else
  bootId=esp_random();
  if(prefs.getUInt("schema",0)==1 && prefs.getBytesLength("settings")==sizeof(detector))prefs.getBytes("settings",&detector,sizeof(detector));
  // Upgrade the previous untouched default mask, preserving customized masks.
  if(!prefs.getBool("coverage-v05",false)){
    const uint32_t oldDefault=dragon::DetectorSettings{}.families&~(1u<<unsigned(Family::IBeacon));
    if(detector.families==oldDefault)detector.families|=1u<<unsigned(Family::IBeacon);
    prefs.putBytes("settings",&detector,sizeof(detector));prefs.putUInt("schema",1);prefs.putBool("coverage-v05",true);
  }
  activeBle=prefs.getBool("active-ble",false);
  // A historical all-bits mask never opts a user into this new category.
  detector.families=dragon::adultOptInMask(detector.families,prefs.getBool("adult-mode",false));
  axonPriority=prefs.getBool("axon-priority",true);
  if(unsigned(detector.minimum)<1||unsigned(detector.minimum)>3)detector.minimum=Confidence::Medium;
  if(detector.autoQuiet!=0&&detector.autoQuiet!=5&&detector.autoQuiet!=10)detector.autoQuiet=5;
  // The byte used to be a re-alert budget (0 meant no limit, the noisiest).
  // It now means how long a device must be away before it alerts again; old
  // choices do not carry over, so every saved value starts at 15 minutes.
  if(!prefs.getBool("visit-v1",false)){detector.autoQuiet=5;prefs.putBytes("settings",&detector,sizeof(detector));prefs.putUInt("schema",1);prefs.putBool("visit-v1",true);}
  visitsRestore();
  if(detector.smoothingMs<600||detector.smoothingMs>4000)detector.smoothingMs=1800;
  if(detector.dimSeconds>600)detector.dimSeconds=0;
  if(detector.offSeconds>600)detector.offSeconds=0;
  if(detector.idleBrightness>25)detector.idleBrightness=8;
  if(prefs.getBytesLength("rules")==sizeof(rules))prefs.getBytes("rules",rules,sizeof(rules));
  for(auto& r:rules)r.flags&=RuleIgnore|RuleWatch;
  alertSound=prefs.getBool("sound",true);alertVibration=prefs.getBool("vibrate",true);
  historyRecords=(HistoryRecord*)ps_calloc(HISTORY_LIMIT,sizeof(HistoryRecord));
  historyReady=historyRecords && LittleFS.totalBytes()>0;
  if(!historyReady){Serial.println("HISTORY unavailable; no automatic format");return;}
  for(uint8_t j=0;j<2;++j){
    File f=LittleFS.open(journals[j],FILE_READ);HistoryRecord r;
    if(!f)continue;
    while(f.read((uint8_t*)&r,sizeof(r))==sizeof(r)) {
      if(r.magic!=HISTORY_MAGIC || r.checksum!=checksum(r) || r.family>=unsigned(Family::Count) || r.confidence>3 || r.name[63] || r.evidence[127])continue;
      // Old journal replay must not evict a newer record when the table is full.
      int existing=-1;for(size_t i=0;i<historyCount;++i)if(historyRecords[i].key==r.key&&historyRecords[i].family==r.family){existing=i;break;}
      if(existing<0 && historyCount==HISTORY_LIMIT){size_t oldest=0;for(size_t i=1;i<historyCount;++i)if(int32_t(historyRecords[i].sequence-historyRecords[oldest].sequence)<0)oldest=i;if(int32_t(r.sequence-historyRecords[oldest].sequence)<=0)continue;}
      int i=existing>=0?existing:historySlot(r.key,r.family);
      if(!historyRecords[i].magic || int32_t(r.sequence-historyRecords[i].sequence)>0)historyRecords[i]=r;
      if(!sequence||int32_t(r.sequence-sequence)>0){sequence=r.sequence;journal=j;}
    }
    f.close();
  }
  File current=LittleFS.open(journals[journal],FILE_READ);journalBytes=current?current.size():0;current.close();
  Serial.printf("HISTORY ready records=%u capacity=%u\n",unsigned(historyCount),unsigned(HISTORY_LIMIT));
#endif
}
void historyObserve(const Contact& c,Family family,Confidence confidence,const char* evidence){
  if(family==Family::Adult)return; // Optional candidates are live-only, not persistent history.
  if(!historyReady||!detector.history||!detector.enabled(family))return;
  uint32_t now=millis(),epoch=wallTime();int i=historySlot(contactKey(c),unsigned(family));auto& r=historyRecords[i];
  bool first=!r.magic;bool encounter=first||r.boot!=bootId||uint32_t(now-r.lastUptime)>300000;
  bool changed=first||r.family!=unsigned(family)||r.confidence!=unsigned(confidence)||strcmp(r.evidence,evidence);
  if(first){r={};r.magic=HISTORY_MAGIC;r.key=contactKey(c);r.firstEpoch=epoch;r.firstUptime=now;}
  if(encounter)++r.encounters;
  r.lastEpoch=epoch;r.lastUptime=now;r.boot=bootId;r.family=unsigned(family);r.confidence=unsigned(confidence);r.channel=c.channel;r.rssi=c.rssi;r.peak=std::max<int16_t>(r.peak,std::max<int16_t>(c.rssi,c.trace.peak));
  strlcpy(r.name,c.name,sizeof(r.name));strlcpy(r.evidence,evidence,sizeof(r.evidence));
  if(changed||encounter||!persistedAt[i]||uint32_t(now-persistedAt[i])>=60000){dirty[i]=true;r.sequence=++sequence;persistedAt[i]=now;}
}
void detectorLoop(){
  if(!historyReady||!detector.history||millis()-lastWrite<250)return;
  for(size_t step=0;step<historyCount;++step){size_t i=(writeCursor+step)%historyCount;if(dirty[i]){
    lastWrite=millis();
    bool rotate=journalBytes%sizeof(HistoryRecord) || journalBytes>=1024*sizeof(HistoryRecord);
    uint8_t target=rotate?1-journal:journal;
    if(!dragon::storageFits(LittleFS.totalBytes(),LittleFS.usedBytes(),65536)){++storageDrops;return;}
    File f=LittleFS.open(journals[target],rotate?FILE_WRITE:FILE_APPEND);
    if(!f){++storageDrops;return;}
    HistoryRecord r=historyRecords[i];r.checksum=checksum(r);
    size_t written=f.write((const uint8_t*)&r,sizeof(r));f.flush();journalBytes=f.size();f.close();journal=target;
    if(written!=sizeof(r)){++storageDrops;return;}dirty[i]=false;writeCursor=(i+1)%HISTORY_LIMIT;return;
  }}
}
void historyClear(){
  if(!historyReady)return;
  bool a=!LittleFS.exists(journals[0])||LittleFS.remove(journals[0]);bool b=!LittleFS.exists(journals[1])||LittleFS.remove(journals[1]);
  if(!a||!b){addAlert("History clear failed","Storage error; some records may remain");return;}
  memset(historyRecords,0,HISTORY_LIMIT*sizeof(HistoryRecord));memset(dirty,0,sizeof(dirty));memset(persistedAt,0,sizeof(persistedAt));historyCount=0;journalBytes=0;sequence=0;
}
static String csv(const char* s){String out="\"";const char* p=s;while(*p==' '||*p=='\t')++p;if(*p&&strchr("=+-@",*p))out+='\'';for(;*s;++s){if(*s=='"')out+='"';if(*s!='\r'&&*s!='\n')out+=*s;}return out+'"';}
bool historyExport(){
  if(!historyReady)return false;
  auto* snapshot=(HistoryRecord*)ps_malloc(historyCount*sizeof(HistoryRecord));
  if(historyCount&&!snapshot)return false;
  const size_t count=historyCount;if(count)memcpy(snapshot,historyRecords,count*sizeof(HistoryRecord));RadioTransfer transfer;
  Serial.println("HISTORY_BEGIN");Serial.println("radio,address_type,mac,name,family,confidence,first_epoch,last_epoch,last_boot_uptime_ms,encounters,rssi,peak,channel,evidence");
  for(size_t i=0;i<count;++i){auto& r=snapshot[i];char mac[18];macText(r.key.mac,mac);
    String row=String(r.key.radio?"BLE":"WiFi")+","+r.key.addressType+","+mac+","+csv(r.name)+","+csv(dragon::familyName(Family(r.family)))+","+String(r.confidence)+","+r.firstEpoch+","+r.lastEpoch+","+r.lastUptime+","+r.encounters+","+r.rssi+","+r.peak+","+r.channel+","+csv(r.evidence);
    if(Serial.println(row)==0){free(snapshot);return false;}radioTransferTick();delay(1);
  }
  free(snapshot);Serial.println("HISTORY_END");return true;
}
void historyRefresh(Contact& c,Family family,Confidence confidence,const char* evidence){
  // Bound the 512-record history search under high packet rates; first family
  // changes are immediate, continuing RSSI/encounter updates at most once/sec.
  if(c.historyFamily!=family || uint32_t(millis()-c.historyAt)>=1000){historyObserve(c,family,confidence,evidence);c.historyAt=millis();c.historyFamily=family;}
}
bool emitDetection(Contact& c,Family family,Confidence confidence,const char* evidence,const dragon::Incident* incident){
  if(family==Family::Adult)return false; // Quiet radar candidates: no popups, sound or event log.
  if(!detector.enabled(family))return false;
  historyRefresh(c,family,confidence,evidence);
  // An incident is not the device: it never takes the watch presentation
  // (LOCKED ON, sticky, past quiet mode) even when its participant is watched.
  uint8_t flags=deviceRules(contactKey(c));bool watch=!incident&&(flags&RuleWatch);
  if(!reconAccepts(family,c.ble)&&!watch)return false;
  if(flags&(RuleIgnore|RuleSnooze))return false;
  // Trackers remain visible and logged, but only an explicit watch rule alerts.
  if(dragon::trackerFamily(family)&&!watch)return false;
  if(!watch && !dragon::alertConfidenceAllowed({family,confidence,evidence},detector.minimum,axonPriority))return false;
  const char* title=watch?"WATCH LIST HIT":dragon::familyName(family);const uint32_t now=millis();
  visits.goneMs=dragon::visitGoneMs(detector.autoQuiet);
  // One notification per device per visit (watched devices too). An incident
  // notifies once per quiet period (never shorter than a visit) and once per
  // kind per participant visit. A repeat only counts on the Alerts row.
  dragon::Incident in{};uint32_t tag=0;if(incident){in=*incident;in.quietMs=dragon::incidentQuietMs(in.quietMs,detector.autoQuiet);tag=dragon::incidentTag(in);}
  auto verdict=incident?incidents.claim(in,now):visits.claim(contactKey(c),uint8_t(family),now);
  if(incident){const auto own=visits.claimIncident(contactKey(c),uint8_t(in.kind),now);if(own!=dragon::Verdict::Notify)verdict=own;}
  if(verdict==dragon::Verdict::Repeat){alertRepeat(title,incident?nullptr:c.mac,tag);return false;}
  auto key=contactKey(c);char detail[128];snprintf(detail,sizeof(detail),"%s%s%d dBm / %s",c.name,*c.name?" / ":"",c.rssi,evidence);
  addAlert(title,detail,c.mac,false,tag); // the ledgers decided: always its own row
  audioAlert(family);
  if(family==Family::Axon)++axonAlertCount;
  // Keep the bubble readable; complete signature evidence stays in history,
  // the event log and contact details.
  snprintf(detail,sizeof(detail),"%s%s%d dBm / %s",c.name,*c.name?" / ":"",c.rssi,c.ble?"Bluetooth":"Wi-Fi");
  if(!detector.quiet||watch)uiNotify(watch?"LOCKED ON":dragon::familyName(family),detail,&key,watch);
  return true;
}
// Read-only, aggregate diagnostics. No addresses are printed.
bool alertCommand(const String& c){
  const uint32_t now=millis();
  if(c=="alerts status"){const uint32_t rows=std::min<uint32_t>(alertTotal,MAX_ALERTS);
    Serial.printf("ALERTS total=%lu rows=%lu repeats=%lu notified_devices=%lu notified_incidents=%lu\n",(unsigned long)alertTotal,(unsigned long)rows,(unsigned long)alertRepeats,(unsigned long)visits.notices,(unsigned long)incidents.notices);
    for(uint32_t i=0;i<rows;++i){auto& a=alerts[(alertTotal-1-i)%MAX_ALERTS];
      Serial.printf("ALERT_ROW %lu age_s=%lu count=%u last_s=%lu title=%s\n",(unsigned long)i,(unsigned long)((now-a.at)/1000),unsigned(a.count),(unsigned long)((now-(a.last?a.last:a.at))/1000),a.title);}
    Serial.println("ALERTS_END");return true;}
  if(c=="visits status"){visits.goneMs=dragon::visitGoneMs(detector.autoQuiet);
    Serial.printf("VISITS capacity=%u used=%u present=%u gone_ms=%lu notified=%lu repeats=%lu live_evictions=%lu restored=%lu listening_wifi=%u listening_ble=%u\n",visits.size,visits.used(),visits.live(now),(unsigned long)visits.goneMs,(unsigned long)visits.notices,(unsigned long)visits.repeats,(unsigned long)visits.evictions,(unsigned long)visits.restored,visits.listening(0),visits.listening(1));
    Serial.printf("INCIDENTS capacity=%u used=%u notified=%lu repeats=%lu evictions=%lu\n",dragon::IncidentLedger::Size,incidents.used(),(unsigned long)incidents.notices,(unsigned long)incidents.repeats,(unsigned long)incidents.evictions);
    for(auto& e:incidents.entries)if(e.used)Serial.printf("INCIDENT kind=%s id=%lu started_s=%lu last_s=%lu quiet_s=%lu repeats=%u\n",dragon::incidentName(e.kind),(unsigned long)e.id,(unsigned long)((now-e.firstAt)/1000),(unsigned long)((now-e.lastTrigger)/1000),(unsigned long)(e.quietMs/1000),unsigned(e.repeats));
    Serial.println("VISITS_END");return true;}
  return false;
}
// The near count has its own keys: DetectorSettings is a size-checked blob.
void nearbySet(bool on,int level){
  level=std::max(-85,std::min(-55,level));nearby.on=on;nearby.enter=level;
  prefs.putBool("near",on);prefs.putChar("near-dbm",level);nearby.reset(millis());
}
// Aggregate only: no address is printed.
bool nearbyCommand(const String& c){
  int level;char tail;
  if(c=="radar near"){const uint32_t now=millis();
    Serial.printf("NEAR on=%u state=%s level_dbm=%d exit_dbm=%d near=%u steady=%u pending=%u capped=%u new_addresses=%lu gone=%lu handovers=%lu returns=%lu dropped=%lu listen_ms=%lu slots=24 radio=BLE unit=addresses\n",nearby.on,nearby.state(now),nearby.enter,nearby.exitDbm(),nearby.near,nearby.steady,nearby.pending,nearby.capped(),(unsigned long)nearby.fresh,(unsigned long)nearby.gone,(unsigned long)nearby.handovers,(unsigned long)nearby.returns,(unsigned long)nearby.dropped,(unsigned long)nearby.clock);
  }else if(c=="radar near on"||c=="radar near off"){nearbySet(c.endsWith("on"),nearby.enter);Serial.printf("NEAR_OK on=%u\n",nearby.on);}
  else if(sscanf(c.c_str(),"radar near level %d %c",&level,&tail)==1&&level>=-85&&level<=-55){nearbySet(nearby.on,level);Serial.printf("NEAR_OK level_dbm=%d\n",nearby.enter);}
  else if(c.startsWith("radar near level"))Serial.println("NEAR_ERROR level -85..-55");
  else return false;
  return true;
}
bool detectorCommand(const String& cmd){
  if(alertCommand(cmd))return true;
  if(cmd=="detector adult on"||cmd=="detector adult off"){setAdultMode(cmd.endsWith("on"));Serial.printf("ADULT_MODE %u\n",detector.enabled(Family::Adult));return true;}
  if(cmd=="detector quiet on"||cmd=="detector quiet off"){detector.quiet=cmd.endsWith("on");detectorSave();audioStop();Serial.printf("DETECTOR_QUIET %u\n",detector.quiet);return true;}
  if(cmd=="axon status"){axonStatus();return true;}
  if(cmd=="axon priority on"||cmd=="axon priority off"){axonPriority=cmd.endsWith("on");detectorSave();Serial.printf("AXON_PRIORITY %u\n",axonPriority);return true;}
  if(cmd=="traffic status"){
    auto& t=trafficStatus;
    Serial.printf("TRAFFIC deauth_frames=%lu deauth_alerts=%lu recent=%u channel=%u reason=%u protected=%u ble_alerts=%lu group=%u addresses=%u variants=%u ssid_conflicts=%lu",t.disconnectFrames,t.disconnectAlerts,recentDisconnects(),t.channel,t.reason,t.protectedFrame,t.bleAlerts,t.bleGroup,t.bleAddresses,t.bleVariants,t.ssidConflicts);
#ifdef BOARD_WAVESHARE_P4_KNOB
    Serial.printf(" ap_security_samples=%lu ble_payload_samples=%lu raw_wifi=0",t.apSecuritySamples,t.blePayloadSamples);
#endif
    Serial.println();return true;
  }
  if(cmd=="radio active"||cmd=="radio passive"){setActiveBle(cmd=="radio active");Serial.printf("RADIO_MODE active_ble=%u\n",activeBle);return true;}
  if(cmd=="history export"){if(!historyExport())Serial.println("HISTORY_ERROR");return true;}
  if(cmd=="history clear"){historyClear();Serial.printf("HISTORY_COUNT %u\n",unsigned(historyCount));return true;}
  if(cmd.startsWith("time ")){unsigned long epoch=0;char extra;if(sscanf(cmd.c_str(),"time %lu %c",&epoch,&extra)==1&&configureClock(epoch))Serial.println("TIME_OK");else Serial.println("TIME_ERROR");return true;}
  if(cmd=="detector status"){Serial.printf("DETECTOR history=%u storage=%s minimum=%u mask=%08lx tracked=%d fox=%u\n",unsigned(historyCount),historyReady?"OK":"MISSING",unsigned(detector.minimum),(unsigned long)detector.families,trackedContact,foxTransmit());return true;}
  if(cmd=="hunt stop"){trackContact(-1);uiShow("radar");Serial.println("HUNT_STOPPED");return true;}
  if(cmd.startsWith("hunt ")){unsigned type=0,channel=0,bytes[6]{};char kind[8],extra;int n=sscanf(cmd.c_str(),"hunt %7s %u %x:%x:%x:%x:%x:%x %u %c",kind,&type,&bytes[0],&bytes[1],&bytes[2],&bytes[3],&bytes[4],&bytes[5],&channel,&extra);
    bool ok=n==9&&type<=3&&channel<=11&&(!strcmp(kind,"BLE")||!strcmp(kind,"WiFi"));for(auto v:bytes)ok&=v<=255;
    if(ok){dragon::DeviceKey key;key.radio=!strcmp(kind,"BLE");key.addressType=key.radio?type:0;for(int i=0;i<6;++i)key.mac[i]=bytes[i];ok=huntDevice(key,channel);if(ok)uiShow("hunt");}Serial.println(ok?"HUNT_OK":"HUNT_ERROR");return true;
  }
  return false;
}
