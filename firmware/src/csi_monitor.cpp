#include "csi_monitor.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "ble_lab.h"
#include "recon_internal.h"
#include "csi_motion.h"
#include <esp_wifi.h>
#include <Preferences.h>
#include <atomic>
#include <new>
namespace {
using dragon::csi::MotionModel;using dragon::csi::State;using dragon::csi::Supply;using dragon::csi::Evidence;using dragon::csi::Candidate;
std::atomic<bool> active{false};std::atomic<uint32_t> dropped{0},callbacks{0},sourcePackets{0};
std::atomic<unsigned> lastLength{0},lastMode{0};std::atomic<int> layout{-1};
// Frames from the selected AP by PHY class (0 DSSS/CCK, 1 legacy OFDM, 2 HT):
// heard from the promiscuous path, csiClass from the CSI callback.
std::atomic<uint32_t> heard[3]{},csiClass[3]{};
bool resume=false;uint8_t oldLock=0,channel=0,source[6]{};
struct Sample {int8_t iq[128],rssi;uint32_t at;};
QueueHandle_t queue=nullptr;StaticQueue_t control;uint8_t* memory=nullptr;
MotionModel* model=nullptr; // about 3 KB, kept in PSRAM like the queue
uint32_t started=0,window=0,total=0,seenMoved=0;
Supply supply=Supply::Ok;
struct Snap {uint32_t frames[3],usable,at;};Snap snaps[6]{};unsigned snapHead=0,snapCount=0;
const char* state="STOPPED";char message[96]="Tap a network to start";char label[33]="";
// What each tried network gave, the last network used (NVS) and whether this
// screen turned the radar's passive listening on just to list networks.
dragon::csi::EvidenceTable<16> evidence;Preferences store;bool storeOpen=false,hasLast=false,surveyOwned=false;uint8_t lastMac[6]{},lastChannel=0;
Candidate* pool=nullptr;uint16_t order[dragon::toolkit::ApCapacity];
struct Point {uint32_t at;uint16_t rate;uint8_t score,quality;const char* state;int8_t margin;float variation;};
Point history[120]{};unsigned count=0,head=0;
const char* const Standing="Senses movement near this detector while it sits still. Not people, position or count.";
void received(void*,wifi_csi_info_t* info){
 if(!active.load()||!info||!info->buf)return;++callbacks;
 if(info->rx_ctrl.channel!=channel||memcmp(info->mac,source,6))return;
 ++sourcePackets;lastLength=info->len;lastMode=info->rx_ctrl.sig_mode;
 unsigned phy=MotionModel::phyClass(info->rx_ctrl.sig_mode,info->rx_ctrl.rate);++csiClass[phy];
 if(!phy||info->len!=128||info->rx_ctrl.sig_mode>1)return;
 int format=info->rx_ctrl.sig_mode+2*info->rx_ctrl.cwb,unset=-1;
 layout.compare_exchange_strong(unset,format);if(layout.load()!=format)return;
 Sample s{};memcpy(s.iq,info->buf,128);s.rssi=info->rx_ctrl.rssi;s.at=millis();
 if(xQueueSend(queue,&s,0)!=pdTRUE)++dropped;
}
void loadLast(){if(storeOpen)return;storeOpen=store.begin("dd-csi",false);if(storeOpen&&store.getBytes("last",lastMac,6)==6){lastChannel=store.getUChar("ch",0);hasLast=true;}}
void saveLast(){loadLast();if(!storeOpen||(hasLast&&!memcmp(lastMac,source,6)&&lastChannel==channel))return;memcpy(lastMac,source,6);lastChannel=channel;hasLast=true;store.putBytes("last",lastMac,6);store.putUChar("ch",channel);}
// Ranks the surveyed AP table; pool[order[i]].slot indexes aps.
unsigned gather(uint32_t fresh,unsigned max=dragon::toolkit::ApCapacity){using namespace dragon::toolkit;
 if(!aps)return 0;if(!pool)pool=(Candidate*)ps_calloc(ApCapacity,sizeof(Candidate));if(!pool)pool=(Candidate*)calloc(ApCapacity,sizeof(Candidate));if(!pool)return 0;loadLast();
 unsigned n=0;const uint32_t now=millis();
 for(unsigned i=0;i<ApCapacity;++i){if(!aps[i].used)continue;Candidate& c=pool[n++];c={};memcpy(c.mac,aps[i].mac,6);c.channel=aps[i].channel;c.rssi=aps[i].rssi;c.age=now-aps[i].seen;c.evidence=evidence.get(c.mac);c.last=hasLast&&!memcmp(c.mac,lastMac,6);c.slot=i;}
 return dragon::csi::rank(pool,n,order,std::min(max,ApCapacity),fresh);
}
bool begin(const uint8_t* bssid,uint8_t ch,const char* name);
int margin(){return model&&model->noiseWindows()>=5?model->margin():0;}
void recalibrate(){model->reset(millis());seenMoved=model->movedCount();window=millis();snapCount=0;supply=Supply::Ok;count=head=0;state="CALIBRATING";}
void note(char* out,size_t capacity){
 if(!active||!model){strlcpy(out,message,capacity);return;}
 const uint32_t now=millis();const State s=model->state();
 if((s==State::LowData||s==State::Calibrating)&&supply!=Supply::Ok)strlcpy(out,dragon::csi::supplyMessage(supply),capacity);
 else if(!model->calibrated()&&model->movedCount()>seenMoved)strlcpy(out,"Signal changed (detector moved or something close); recalibrating",capacity);
 else if(s==State::LowData)strlcpy(out,model->calibrated()?"Too little CSI from this network":"Calibration needs 15 windows of 5+ samples/s",capacity);
 else if(s==State::Calibrating)snprintf(out,capacity,"Calibrating %u/30 s. Keep detector and AP still.",model->calibrationPercent(now)*30/100);
 else if(s==State::Hold)snprintf(out,capacity,"Motion %lus ago / hold %lus",(unsigned long)((now-model->lastMotion())/1000),(unsigned long)(MotionModel::HoldMs/1000));
 else if(model->lastMotion())snprintf(out,capacity,"Last motion %lus ago",(unsigned long)((now-model->lastMotion())/1000));
 else strlcpy(out,"No motion since calibration",capacity);
}
}
void csiFrame(const wifi_promiscuous_pkt_t* p){if(active.load()&&p->rx_ctrl.channel==channel&&!memcmp(p->payload+10,source,6))++heard[MotionModel::phyClass(p->rx_ctrl.sig_mode,p->rx_ctrl.rate)];}
bool csiActive(){return active.load();}
void csiStop(){if(!active.exchange(false))return;esp_wifi_set_csi(false);esp_wifi_set_csi_rx_cb(nullptr,nullptr);esp_wifi_set_promiscuous(false);esp_wifi_set_mode(WIFI_MODE_NULL);channelLock=oldLock;setRadio(resume);state="STOPPED";strlcpy(message,"Room motion stopped / radar restored",sizeof(message));}
bool csiStart(){using namespace dragon::toolkit;
 if(!target.hasAp){strlcpy(message,"No network selected; tap one on the Room motion screen",sizeof(message));return false;}
 bool fresh=false;for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used&&!memcmp(aps[i].mac,target.bssid,6)&&millis()-aps[i].seen<30000&&aps[i].channel==target.channel)fresh=true;
 if(!fresh){strlcpy(message,"Selected AP is stale; survey and select it again",sizeof(message));return false;}
 return begin(target.bssid,target.channel,target.ssid);
}
namespace {
bool begin(const uint8_t* bssid,uint8_t ch,const char* name){using namespace dragon::toolkit;
 if(active.load())csiStop();
 if(radioStandby()||reconOwnsRadio()||reconStorageBusy()||bleLabOwnsRadio()||status.recording){strlcpy(message,"Leave clock/timer and stop other radio work first",sizeof(message));return false;}
 if(!queue){if(!memory)memory=(uint8_t*)ps_malloc(32*sizeof(Sample));if(!memory)return false;queue=xQueueCreateStatic(32,sizeof(Sample),memory,&control);if(!queue)return false;}
 if(!model){void* m=ps_malloc(sizeof(MotionModel));if(!m)return false;model=new(m) MotionModel();}
 Sample stale;while(xQueueReceive(queue,&stale,0)==pdTRUE){}
 // Listening the screen started for the list is not the radar state to restore.
 resume=surveyOwned?false:radioEnabled;surveyOwned=false;
 oldLock=channelLock;memcpy(source,bssid,6);channel=ch;strlcpy(label,name&&*name?name:"",sizeof(label));setRadio(false);active=true;dropped=0;callbacks=0;sourcePackets=0;lastLength=lastMode=0;layout=-1;total=0;started=millis();
 for(unsigned k=0;k<3;++k)heard[k]=csiClass[k]=0;model->clearLog();recalibrate();
 wifi_csi_config_t cfg{};cfg.lltf_en=true;cfg.channel_filter_en=true;
 esp_err_t err=esp_wifi_set_mode(WIFI_MODE_STA);
 if(err==ESP_OK)err=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
 if(err==ESP_OK)err=esp_wifi_set_csi_config(&cfg);
 if(err==ESP_OK)err=esp_wifi_set_csi_rx_cb(received,nullptr);
 if(err==ESP_OK)err=esp_wifi_set_csi(true);
 if(err==ESP_OK)err=esp_wifi_set_promiscuous(true);
 if(err!=ESP_OK){csiStop();snprintf(message,sizeof(message),"CSI setup: %s",esp_err_to_name(err));return false;}
 saveLast();strlcpy(message,"Keep AP and detector still during calibration",sizeof(message));return true;
}
}
// By BSSID from the surveyed table, on the channel it was last heard on.
bool csiStartNetwork(const uint8_t* bssid){using namespace dragon::toolkit;const uint32_t now=millis();
 for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used&&!memcmp(aps[i].mac,bssid,6)&&now-aps[i].seen<dragon::csi::NextFreshMs&&aps[i].channel>=1&&aps[i].channel<=13)return begin(aps[i].mac,aps[i].channel,aps[i].name);
 strlcpy(message,"That network is no longer heard; pick another",sizeof(message));return false;
}
bool csiStartAuto(){unsigned n=gather(dragon::csi::FreshMs);int k=dragon::csi::pick(pool,order,n);
 if(k<0){strlcpy(message,n?"Every network heard gave no motion data; move closer to an AP":"No network heard yet; wait a few seconds",sizeof(message));return false;}
 return csiStartNetwork(dragon::toolkit::aps[pool[k].slot].mac);
}
bool csiStartNext(){uint8_t current[6];memcpy(current,source,6);unsigned n=gather(dragon::csi::NextFreshMs);int k=dragon::csi::pick(pool,order,n,current);
 if(k<0){strlcpy(message,"No other network heard; Change network to listen again",sizeof(message));return false;}
 return csiStartNetwork(dragon::toolkit::aps[pool[k].slot].mac);
}
bool csiNextName(char* out,size_t capacity){unsigned n=gather(dragon::csi::NextFreshMs);int k=dragon::csi::pick(pool,order,n,source);if(k<0)return false;
 const char* name=dragon::toolkit::aps[pool[k].slot].name;if(*name)strlcpy(out,name,capacity);else snprintf(out,capacity,"hidden CH %u",pool[k].channel);return true;}
unsigned csiNetworks(CsiNetwork* out,unsigned max,uint32_t fresh){unsigned n=gather(fresh,max);
 for(unsigned i=0;i<n;++i){auto& c=pool[order[i]];auto& o=out[i];memcpy(o.mac,c.mac,6);o.channel=c.channel;o.rssi=c.rssi;o.age=c.age;o.evidence=c.evidence;o.last=c.last;strlcpy(o.name,dragon::toolkit::aps[c.slot].name,sizeof(o.name));}
 return n;
}
bool csiNoData(){return active.load()&&(supply==Supply::NotHeard||supply==Supply::NoCsi);}
const char* csiMessage(){return message;}
void csiSurvey(bool on){
 if(on){if(!active.load()&&!surveyOwned&&!radioEnabled&&!radioStandby()&&!reconOwnsRadio()&&!bleLabOwnsRadio()){setRadio(true);surveyOwned=radioEnabled;}return;}
 if(surveyOwned&&!active.load())setRadio(false);surveyOwned=false;
}
void csiLoop(){if(!active)return;uint32_t now=millis();if(now-started>=900000){csiStop();return;}
 Sample s;for(unsigned i=0;i<32&&xQueueReceive(queue,&s,0)==pdTRUE;++i){model->addSample(s.iq,sizeof(s.iq),s.rssi,s.at,true,dragon::csi::Encoding::Signed8ImagReal);++total;}
 model->tick(now=millis(),total,dropped.load());state=dragon::csi::stateName(model->state());
 if(now-window<1000)return;
 // Five-second view of what the selected AP sends, for the LOW DATA reason.
 Snap& c=snaps[snapHead];for(unsigned k=0;k<3;++k)c.frames[k]=std::max(heard[k].load(),csiClass[k].load());c.usable=total;c.at=now;
 snapHead=(snapHead+1)%6;if(snapCount<6)++snapCount;
 const Snap& o=snaps[(snapHead+6-snapCount)%6];
 dragon::csi::Traffic t;t.dsss=c.frames[0]-o.frames[0];t.ofdm=c.frames[1]-o.frames[1];t.ht=c.frames[2]-o.frames[2];t.heard=t.dsss+t.ofdm+t.ht;t.usable=c.usable-o.usable;
 supply=snapCount>=4?dragon::csi::diagnose(t,c.at-o.at):Supply::Ok;
 if(supply==Supply::NotHeard||supply==Supply::NoCsi)evidence.note(source,Evidence::NoData,now);
 else if(c.frames[1]+c.frames[2])evidence.note(source,Evidence::Ofdm,now);
 history[head]={now,model->samplesPerSecond(),model->motionScore(),model->reliability(),state,int8_t(margin()),model->fused()};head=(head+1)%120;count=std::min(count+1,120u);
 window=now;
}
void csiStatus(char* out,size_t capacity){
 char text[96],edge[8]="--";note(text,sizeof(text));if(model&&model->noiseWindows()>=5)snprintf(edge,sizeof(edge),"%+d",model->margin());
 char name[40];if(active.load())snprintf(name,sizeof(name)," / %s",*label?displayName(label).c_str():"hidden");else *name=0;
 snprintf(out,capacity,"%s%s / CH %u\nScore %u / limit %u / margin %s\nQuality %u%% / %u samples/s\n%s\n\n%s",state,name,channel,model?model->motionScore():0,MotionModel::MotionThreshold,edge,model?model->reliability():0,model?model->samplesPerSecond():0,text,Standing);
}
bool csiCommand(const String& c){if(!c.startsWith("csi "))return false;
 if(c=="csi start")Serial.println(csiStart()?"RECON_OK CSI started":"RECON_ERROR fresh AP / busy / CSI setup");
 // Indexes follow the latest ranking, as printed by `csi networks`.
 else if(c=="csi networks"){unsigned n=gather(dragon::csi::FreshMs,32);for(unsigned i=0;i<n;++i){auto& x=pool[order[i]];auto& a=dragon::toolkit::aps[x.slot];char mac[18];macText(x.mac,mac);
  Serial.printf("RECON_CSI_NETWORK %u %s ch=%u rssi=%d age=%lu evidence=%s last=%u name=%s\n",i,mac,x.channel,x.rssi,(unsigned long)(x.age/1000),dragon::csi::evidenceName(x.evidence),x.last,a.name);}
  Serial.println("RECON_OK");}
 else if(c=="csi start auto")Serial.println(csiStartAuto()?"RECON_OK CSI started":"RECON_ERROR no usable network / busy / CSI setup");
 else if(c=="csi start next")Serial.println(csiStartNext()?"RECON_OK CSI started":"RECON_ERROR no other network / busy / CSI setup");
 else if(c.startsWith("csi start ")){char extra;unsigned k;bool ok=false;
  if(sscanf(c.c_str(),"csi start %u %c",&k,&extra)==1&&k<32){unsigned n=gather(dragon::csi::FreshMs,32);ok=k<n&&csiStartNetwork(pool[order[k]].mac);}
  Serial.println(ok?"RECON_OK CSI started":"RECON_ERROR index / busy / CSI setup");}
 else if(c=="csi stop"){csiStop();Serial.println("RECON_OK");}
 else if(c=="csi calibrate"&&active){recalibrate();Serial.println("RECON_OK calibrating");}
 else if(c=="csi status"){
  const bool m=model;Serial.printf("RECON_CSI active=%u state=%s channel=%u score=%u quality=%u rate=%u samples=%lu drops=%lu callbacks=%lu source_packets=%lu last_length=%u last_mode=%u layout=%d baseline=%u variation=%.6f threshold=%.6f message=%s\n",active.load(),state,channel,m?model->motionScore():0,m?model->reliability():0,m?model->samplesPerSecond():0,total,dropped.load(),callbacks.load(),sourcePackets.load(),lastLength.load(),lastMode.load(),layout.load(),m?unsigned(model->calibrationWindows()):0,m?model->fused():0,m?model->thresholdZ():0,message);
  char text[96];note(text,sizeof(text));const uint32_t now=millis();
  Serial.printf("RECON_CSI_ROOM window_score=%u limit=%u margin=%d noise_windows=%u still_peak=%u last_motion_age_ms=%ld hold_ms=%lu intervals=%u moved=%lu rejected=%lu carriers=%u amplitude=%u correlation=%u phase=%u supply=%s heard_dsss=%lu heard_ofdm=%lu heard_ht=%lu csi_dsss=%lu csi_ofdm=%lu csi_ht=%lu note=%s\nRECON_OK\n",m?model->windowScore():0,MotionModel::MotionThreshold,margin(),m?unsigned(model->noiseWindows()):0,m?model->stillPeak():0,m&&model->lastMotion()?long(now-model->lastMotion()):-1L,(unsigned long)MotionModel::HoldMs,m?unsigned(model->intervalCount()):0,m?model->movedCount():0,m?model->rejected():0,m?unsigned(model->carriers()):0,m?model->amplitudeScore():0,m?model->correlationScore():0,m?model->phaseScore():0,dragon::csi::supplyName(supply),heard[0].load(),heard[1].load(),heard[2].load(),csiClass[0].load(),csiClass[1].load(),csiClass[2].load(),text);
 }
 else if(c=="csi history"){Serial.println("RECON_CSI_CSV uptime_ms,samples_per_second,change_score,signal_quality,variation,state,margin");for(unsigned i=0;i<count;++i){auto& p=history[(head+120-count+i)%120];Serial.printf("RECON_CSI_POINT %lu,%u,%u,%u,%.6f,%s,%d\n",p.at,p.rate,p.score,p.quality,p.variation,p.state,p.margin);}
  Serial.println("RECON_CSI_INTERVALS start_ms,end_ms,peak_score,status");for(size_t i=0;model&&i<model->intervalCount();++i){auto& v=model->interval(i);Serial.printf("RECON_CSI_INTERVAL %lu,%lu,%u,%s\n",v.start,v.end,v.peak,v.open?"open":v.moved?"detector_moved":"ended");}
  Serial.println("RECON_OK");}
 else Serial.println("RECON_ERROR CSI command or inactive");return true;
}
#endif
