#include "app.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_timer.h>
#include <time.h>

dragon::DetectorSettings detector;
HistoryRecord* historyRecords=nullptr;
size_t historyCount=0;
bool historyReady=false;
static Preferences prefs;
struct Rule {dragon::DeviceKey key;uint8_t flags=0;};
static Rule rules[RULE_LIMIT];
static uint32_t bootId=0,sequence=0,epochBase=0;
static uint64_t epochAt=0;
static uint32_t persistedAt[HISTORY_LIMIT]{};
static bool dirty[HISTORY_LIMIT]{};
static uint8_t journal=0;
static size_t journalBytes=0;
static uint32_t lastWrite=0;
static size_t writeCursor=0;
static const char* journals[]={"/detections-0.dat","/detections-1.dat"};
static constexpr uint32_t HISTORY_MAGIC=0x34484444;
static uint32_t checksum(const HistoryRecord& r){return dragon::crc32((const uint8_t*)&r,offsetof(HistoryRecord,checksum))^0xffffffffu;}
uint32_t wallTime(){return epochBase?epochBase+uint32_t((esp_timer_get_time()-epochAt)/1000000):0;}
void setWallTime(uint32_t epoch){epochBase=epoch;epochAt=esp_timer_get_time();}
dragon::DeviceKey contactKey(const Contact& c){return keyOf(c);}
uint8_t deviceRules(const dragon::DeviceKey& key){for(auto& r:rules)if(r.flags&&r.key==key)return r.flags;return 0;}
bool ruleAt(size_t i,dragon::DeviceKey& key,uint8_t& flags){if(i>=RULE_LIMIT||!rules[i].flags)return false;key=rules[i].key;flags=rules[i].flags;return true;}
bool toggleRule(const dragon::DeviceKey& key,DeviceRule flag){
  Rule* slot=nullptr;for(auto& r:rules){if(r.flags&&r.key==key){slot=&r;break;}if(!r.flags&&!slot)slot=&r;}
  if(!slot)return false;
  Rule before=*slot;slot->key=key;slot->flags^=flag;
  Rule saved[RULE_LIMIT];memcpy(saved,rules,sizeof(saved));for(auto& r:saved)r.flags&=RuleIgnore|RuleWatch;
  if(prefs.putBytes("rules",saved,sizeof(saved))!=sizeof(saved)){*slot=before;return false;}
  return true;
}
void detectorSave(){
  prefs.putUInt("schema",1);prefs.putBytes("settings",&detector,sizeof(detector));
  prefs.putBool("sound",alertSound);prefs.putBool("vibrate",alertVibration);
  prefs.putBool("active-ble",activeBle);
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
  bootId=esp_random();prefs.begin("dd-detect",false);
  if(prefs.getUInt("schema",0)==1 && prefs.getBytesLength("settings")==sizeof(detector))prefs.getBytes("settings",&detector,sizeof(detector));
  // Upgrade the previous untouched default mask, preserving customized masks.
  if(!prefs.getBool("coverage-v05",false)){
    const uint32_t oldDefault=((1u<<unsigned(Family::Count))-1u)&~(1u<<unsigned(Family::IBeacon));
    if(detector.families==oldDefault)detector.families|=1u<<unsigned(Family::IBeacon);
    prefs.putBytes("settings",&detector,sizeof(detector));prefs.putUInt("schema",1);prefs.putBool("coverage-v05",true);
  }
  activeBle=prefs.getBool("active-ble",false);
  if(unsigned(detector.minimum)<1||unsigned(detector.minimum)>3)detector.minimum=Confidence::Medium;
  if(detector.autoQuiet!=0&&detector.autoQuiet!=5&&detector.autoQuiet!=10)detector.autoQuiet=5;
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
}
void historyObserve(const Contact& c,Family family,Confidence confidence,const char* evidence){
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
    if(LittleFS.totalBytes()-LittleFS.usedBytes()<65536){++storageDrops;return;}
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
  Serial.println("HISTORY_BEGIN");Serial.println("radio,address_type,mac,name,family,confidence,first_epoch,last_epoch,last_boot_uptime_ms,encounters,rssi,peak,channel,evidence");
  for(size_t i=0;i<historyCount;++i){auto& r=historyRecords[i];char mac[18];macText(r.key.mac,mac);
    String row=String(r.key.radio?"BLE":"WiFi")+","+r.key.addressType+","+mac+","+csv(r.name)+","+csv(dragon::familyName(Family(r.family)))+","+String(r.confidence)+","+r.firstEpoch+","+r.lastEpoch+","+r.lastUptime+","+r.encounters+","+r.rssi+","+r.peak+","+r.channel+","+csv(r.evidence);
    if(Serial.println(row)==0)return false;delay(1);
  }
  Serial.println("HISTORY_END");return true;
}
bool emitDetection(Contact& c,Family family,Confidence confidence,const char* evidence,bool force){
  if(!detector.enabled(family))return false;
  // Bound the 512-record history search under high packet rates; first family
  // changes are immediate, continuing RSSI/encounter updates at most once/sec.
  if(c.historyFamily!=family || uint32_t(millis()-c.historyAt)>=1000){historyObserve(c,family,confidence,evidence);c.historyAt=millis();c.historyFamily=family;}
  uint8_t flags=deviceRules(contactKey(c));bool watch=flags&RuleWatch;
  if(flags&(RuleIgnore|RuleSnooze))return false;
  if(!watch && confidence<detector.minimum)return false;
  if(!force && !c.gate.allow(millis(),c.rssi,detector.autoQuiet,watch))return false;
  auto key=contactKey(c);char detail[128];snprintf(detail,sizeof(detail),"%d dBm / %s; %s",c.rssi,dragon::confidenceName(confidence),evidence);
  addAlert(watch?"WATCH LIST HIT":dragon::familyName(family),detail,c.mac);
  audioAlert(family);
  if(!detector.quiet||watch)uiNotify(watch?"LOCKED ON":dragon::familyName(family),detail,&key,watch);
  return true;
}
bool detectorCommand(const String& cmd){
  if(cmd=="traffic status"){auto& t=trafficStatus;Serial.printf("TRAFFIC deauth_frames=%lu deauth_alerts=%lu recent=%u channel=%u reason=%u protected=%u ble_alerts=%lu group=%u addresses=%u variants=%u\n",t.disconnectFrames,t.disconnectAlerts,recentDisconnects(),t.channel,t.reason,t.protectedFrame,t.bleAlerts,t.bleGroup,t.bleAddresses,t.bleVariants);return true;}
  if(cmd=="radio active"||cmd=="radio passive"){setActiveBle(cmd=="radio active");Serial.printf("RADIO_MODE active_ble=%u\n",activeBle);return true;}
  if(cmd=="history export"){bool scanning=radioEnabled;setRadio(false);if(!historyExport())Serial.println("HISTORY_ERROR");setRadio(scanning);return true;}
  if(cmd=="history clear"){historyClear();Serial.printf("HISTORY_COUNT %u\n",unsigned(historyCount));return true;}
  if(cmd.startsWith("time ")){unsigned long epoch=0;char extra;if(sscanf(cmd.c_str(),"time %lu %c",&epoch,&extra)==1&&epoch>=1700000000ul&&epoch<=4102444800ul){setWallTime(epoch);Serial.println("TIME_OK");}else Serial.println("TIME_ERROR");return true;}
  if(cmd=="detector status"){Serial.printf("DETECTOR history=%u storage=%s minimum=%u mask=%08lx tracked=%d fox=%u\n",unsigned(historyCount),historyReady?"OK":"MISSING",unsigned(detector.minimum),(unsigned long)detector.families,trackedContact,foxTransmit());return true;}
  if(cmd=="hunt stop"){trackContact(-1);uiShow("radar");Serial.println("HUNT_STOPPED");return true;}
  if(cmd.startsWith("hunt ")){unsigned type=0,channel=0,bytes[6]{};char kind[8],extra;int n=sscanf(cmd.c_str(),"hunt %7s %u %x:%x:%x:%x:%x:%x %u %c",kind,&type,&bytes[0],&bytes[1],&bytes[2],&bytes[3],&bytes[4],&bytes[5],&channel,&extra);
    bool ok=n==9&&type<=3&&channel<=11&&(!strcmp(kind,"BLE")||!strcmp(kind,"WiFi"));for(auto v:bytes)ok&=v<=255;
    if(ok){dragon::DeviceKey key;key.radio=!strcmp(kind,"BLE");key.addressType=key.radio?type:0;for(int i=0;i<6;++i)key.mac[i]=bytes[i];ok=huntDevice(key,channel);if(ok)uiShow("hunt");}Serial.println(ok?"HUNT_OK":"HUNT_ERROR");return true;
  }
  return false;
}
