// MIT: synthesized RTTTL alerts; no legacy MP3 decoder or microphone capture.
#include "app.h"
#include "recon.h"
#include "tones.h"
#include <Preferences.h>
static Preferences prefs;
static AppAudioStatus status;
static String custom[unsigned(Family::Count)];
static dragon::Melody melody;
static dragon::ToneQueue queue;
static unsigned cursor=0;static uint32_t noteAt=0,nextPulse=0;
static bool ready=false,playing=false,preview=false,hadSignal=false;
static int lastTarget=-1;
static dragon::DeviceKey lastKey;
bool huntSound=true;
int audioFocus=-1;
bool toneOutputBegin();
void toneOutput(uint16_t hz,uint16_t ms,uint8_t volume);
static_assert(sizeof(dragon::defaultTones)/sizeof(*dragon::defaultTones)==unsigned(Family::Count),"Every detector needs a tone");
static String toneText(unsigned f){char key[12];snprintf(key,sizeof(key),"tone%u",f);return prefs.isKey(key)?custom[f]:String(dragon::defaultTones[f]);}
static bool start(const char* text,bool isPreview=false){
 dragon::Melody next;if(!ready||!dragon::parseTone(text,next))return false;
 toneOutput(0,0,0);melody=next;cursor=0;noteAt=millis();playing=next.count>0;preview=isPreview;
 status.mode=playing?AudioMode::Playing:AudioMode::Idle;return true;
}
void audioBegin(){
 prefs.begin("dd-tones",false);status.volume=constrain(prefs.getUChar("volume",100),0,100);huntSound=prefs.getBool("hunt",true);audioFocus=prefs.getInt("focus",-1);if(audioFocus<-1||audioFocus>=int(Family::Count))audioFocus=-1;
 for(unsigned i=0;i<unsigned(Family::Count);++i){char key[12];snprintf(key,sizeof(key),"tone%u",i);String value=prefs.getString(key,"");dragon::Melody test;if(value.length()<=dragon::TONE_TEXT_MAX&&dragon::parseTone(value.c_str(),test))custom[i]=value;else prefs.remove(key);}
 #if defined(BOARD_XIAO_ROUND) || defined(BOARD_CROWPANEL_21)
 ready=false;strlcpy(status.message,"No onboard speaker / visual alerts",sizeof(status.message));
 #else
 ready=toneOutputBegin();snprintf(status.message,sizeof(status.message),"%s",ready?"RTTTL speaker ready":"Speaker initialization failed");if(!ready)status.mode=AudioMode::Error;
 #endif
 Serial.printf("AUDIO_READY %u\n",ready);
}
void audioSave(){prefs.putUChar("volume",status.volume);prefs.putBool("hunt",huntSound);prefs.putInt("focus",audioFocus);detectorSave();}
void audioVolume(int v){status.volume=constrain(v,0,100);audioSave();}
void audioStop(){playing=preview=false;queue.clear();toneOutput(0,0,0);
#if defined(BOARD_XIAO_ROUND) || defined(BOARD_CROWPANEL_21)
 status.mode=AudioMode::Idle;
#else
 status.mode=ready?AudioMode::Idle:AudioMode::Error;
#endif
}
void audioPlay(const String&){snprintf(status.message,sizeof(status.message),"Use RTTTL alerts in Settings / Audio");}
void audioRecord(){snprintf(status.message,sizeof(status.message),"Microphone recording is not enabled");}
AppAudioStatus audioStatus(){return status;}
bool audioReady(){return ready;}
void audioBeep(){start("Test:d=16,o=6,b=180:c,e,g",true);}
void audioAlert(Family family){if(ready&&alertSound&&!detector.quiet&&status.volume){
  // Axon can interrupt a lower-priority melody immediately; retain cooldown.
  if(family==Family::Axon&&!(queue.played&(1u<<unsigned(family))&&uint32_t(millis()-queue.last[unsigned(family)])<4000)){
    queue.pending&=~(1u<<unsigned(family));queue.played|=1u<<unsigned(family);queue.last[unsigned(family)]=millis();start(toneText(unsigned(family)).c_str());
  }else queue.request(unsigned(family),millis());
}}
static int effectiveFocus(){auto m=reconMode();return m==dragon::ReconMode::Axon?int(Family::Axon):m==dragon::ReconMode::Alpr?int(Family::Flock):m==dragon::ReconMode::Sky?int(Family::Drone):audioFocus;}
static bool focusMatches(Family f){return int(f)==effectiveFocus()||(effectiveFocus()==int(Family::Flock)&&(f==Family::Alpr||f==Family::Raven));}
static bool focusEligible(const Contact& c,uint32_t now){
 if(!c.used||now-c.seen>12000||(deviceRules(contactKey(c))&(RuleIgnore|RuleSnooze)))return false;
 if(effectiveFocus()>=int(Family::Deauth)&&effectiveFocus()<=int(Family::BleSpam))return int(c.trafficFamily)==effectiveFocus()&&detector.enabled(c.trafficFamily)&&c.wifiAlerts&&now-c.lastWifiAlert<=12000;
 if(!focusMatches(c.match.family)||!detector.enabled(c.match.family))return false;
 if(effectiveFocus()<=int(Family::Ble))return c.packets>=2&&now-c.firstSeen>=400;
 return now-c.matchedAt<=12000&&((dragon::signatureAlertReady(c.match,c.matchHits,c.matchSince,c.matchedAt,axonPriority)&&dragon::alertConfidenceAllowed(c.match,detector.minimum,axonPriority))||(deviceRules(contactKey(c))&RuleWatch));
}
static int target(uint32_t now){
 if(trackedContact>=0)return trackedContact;
 if(effectiveFocus()<0)return -1;
 static uint32_t checkedAt=0;static int cached=-1,cachedFocus=-2;
 if(cachedFocus==effectiveFocus() && uint32_t(now-checkedAt)<150){
   return cached>=0&&focusEligible(contacts[cached],now)?cached:-1;
 }
 checkedAt=now;cachedFocus=effectiveFocus();
 int best=-1;
 for(unsigned i=0;i<MAX_CONTACTS;++i){auto& c=contacts[i];if(!focusEligible(c,now))continue;if(best<0||c.signal.value()>contacts[best].signal.value())best=i;}
 if(best>=0&&lastTarget>=0&&lastTarget<int(MAX_CONTACTS)){auto& c=contacts[lastTarget];if(contactKey(c)==lastKey&&focusEligible(c,now)&&c.signal.value()+6>=contacts[best].signal.value())best=lastTarget;}
 cached=best;return best;
}
void audioLoop(){
 uint32_t now=millis();bool enabled=ready&&alertSound&&!detector.quiet&&radioEnabled&&status.volume;
 if(!enabled){queue.clear();hadSignal=false;lastTarget=-1;if(!preview)audioStop();}
 if(playing&&int32_t(now-noteAt)>=0){
  if(cursor>=melody.count){playing=preview=false;status.mode=AudioMode::Idle;toneOutput(0,0,0);}
  else{auto n=melody.notes[cursor++];toneOutput(n.hz,n.ms*9/10,status.volume);noteAt=now+n.ms;}
 }
 if(!enabled||playing)return;
 int family=queue.take(now);if(family>=0){snprintf(status.message,sizeof(status.message),"%s alert",dragon::familyName(Family(family)));start(toneText(family).c_str());return;}
 if(!huntSound){hadSignal=false;lastTarget=-1;return;}
 int i=target(now);bool fresh=i>=0&&i<int(MAX_CONTACTS)&&contacts[i].used&&contacts[i].signal.count&&now-contacts[i].seen<=12000&&!(deviceRules(contactKey(contacts[i]))&(RuleIgnore|RuleSnooze));
 if(!fresh){if(hadSignal){start("Lost:d=16,o=5,b=150:g,c");snprintf(status.message,sizeof(status.message),"Hunt signal lost");}hadSignal=false;lastTarget=-1;return;}
 auto& c=contacts[i];bool changed=lastTarget!=i||!(lastKey==contactKey(c));lastTarget=i;lastKey=contactKey(c);
 if(changed)nextPulse=now;hadSignal=true;
 if(int32_t(now-nextPulse)<0)return;
 unsigned hz=dragon::huntPitch(c.signal.value());nextPulse=now+dragon::huntInterval(c.signal.value());
 melody.count=3;melody.duration=180;melody.notes[0]={uint16_t(hz),60};melody.notes[1]={0,50};
 // Axon has a distinct double heartbeat; other targets rise/fall with trend.
 melody.notes[2]={uint16_t(c.match.family==Family::Axon?hz:c.signal.trend>0?hz*5/4:c.signal.trend<0?hz*4/5:hz),70};
 cursor=0;noteAt=now;playing=true;status.mode=AudioMode::Playing;snprintf(status.message,sizeof(status.message),"Hunt %s / %d dBm",dragon::familyName(c.match.family),c.signal.value());
}
static void settings(){Serial.printf("TONE_SETTINGS ready=%u enabled=%u volume=%u hunt=%u focus=%d quiet=%u\n",ready,alertSound,status.volume,huntSound,audioFocus,detector.quiet);}
bool audioCommand(const String& c){
 if(c=="tone settings"){settings();return true;}
 if(c=="tone stop"){audioStop();Serial.println("TONE_OK");return true;}
 if(c.startsWith("tone config ")){unsigned enabled,v,hunt;int focus;char extra;if(sscanf(c.c_str(),"tone config %u %u %u %d %c",&enabled,&v,&hunt,&focus,&extra)==4&&enabled<=1&&v<=100&&hunt<=1&&focus>=-1&&focus<int(Family::Count)){alertSound=enabled;status.volume=v;huntSound=hunt;audioFocus=focus;audioStop();audioSave();Serial.println("TONE_OK");}else Serial.println("TONE_ERROR settings");return true;}
 if(!c.startsWith("tone "))return false;
 unsigned f;char action[12];int offset=0;
 if(sscanf(c.c_str(),"tone %11s %u%n",action,&f,&offset)!=2||f>=unsigned(Family::Count)){Serial.println("TONE_ERROR family");return true;}
 bool valueAction=!strcmp(action,"set")||!strcmp(action,"preview");
 if((valueAction&&c[offset]!=' ')||(!valueAction&&unsigned(offset)!=c.length())){Serial.println("TONE_ERROR syntax");return true;}
 char key[12];snprintf(key,sizeof(key),"tone%u",f);
 if(!strcmp(action,"get")){Serial.printf("TONE_VALUE %u %s\n",f,toneText(f).c_str());return true;}
 if(!strcmp(action,"reset")){if(prefs.isKey(key)&&!prefs.remove(key)){Serial.println("TONE_ERROR storage");return true;}custom[f]="";Serial.println("TONE_OK");return true;}
 if(!strcmp(action,"play")){Serial.println(start(toneText(f).c_str(),true)?"TONE_OK":"TONE_ERROR output");return true;}
 if(valueAction){String value=c.substring(offset+1);if(value=="-")value="";dragon::Melody test;if(!dragon::parseTone(value.c_str(),test)){Serial.println("TONE_ERROR invalid RTTTL (512 chars / 96 notes / 15 seconds)");return true;}
  if(!strcmp(action,"preview")){Serial.println(start(value.c_str(),true)?"TONE_OK":"TONE_ERROR output");return true;}
  if(prefs.putString(key,value)!=value.length()||!prefs.isKey(key)){Serial.println("TONE_ERROR storage");return true;}custom[f]=value;Serial.println("TONE_OK");return true;
 }
 Serial.println("TONE_ERROR command");return true;
}
#if !defined(BOARD_STOPWATCH) && !defined(BOARD_CROWPANEL_21)
#if !defined(BOARD_AMOLED_175C) && !defined(BOARD_AMOLED_175) && !defined(BOARD_XIAO_ROUND)
void boardBegin(){}
void boardLoop(){}
#endif
bool alertSound=true,alertVibration=false;
#endif
