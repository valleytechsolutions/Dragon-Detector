#include "csi_monitor.h"
#ifdef BOARD_AMOLED_175C
#include "app.h"
#include "ble_lab.h"
#include "recon_internal.h"
#include "csi_analysis.h"
#include <esp_wifi.h>
#include <atomic>
namespace {
std::atomic<bool> active{false};std::atomic<uint32_t> dropped{0},callbacks{0},sourcePackets{0};
std::atomic<unsigned> lastLength{0},lastMode{0};std::atomic<int> layout{-1};
bool resume=false;uint8_t oldLock=0,channel=0,source[6]{};
struct Sample {int8_t iq[128],rssi;uint32_t at;};
QueueHandle_t queue=nullptr;StaticQueue_t control;uint8_t* memory=nullptr;
dragon::csi::Shape shape;dragon::csi::Baseline baseline;
uint32_t started=0,calibration=0,window=0,lastSample=0,total=0,previousDrops=0;
unsigned windowCount=0,cadence=0,score=0,quality=0;
float sum=0,rssiSum=0,rssiSq=0,variation=0;
const char* state="STOPPED";char message[96]="Select a fresh AP, then Start";
struct Point {uint32_t at;uint16_t rate;uint8_t score,quality;float variation;};
Point history[120]{};unsigned count=0,head=0;
void received(void*,wifi_csi_info_t* info){
 if(!active.load()||!info||!info->buf)return;++callbacks;
 if(info->rx_ctrl.channel!=channel||memcmp(info->mac,source,6))return;
 ++sourcePackets;lastLength=info->len;lastMode=info->rx_ctrl.sig_mode;
 if(info->len!=128||info->rx_ctrl.sig_mode>1)return;
 int format=info->rx_ctrl.sig_mode+2*info->rx_ctrl.cwb,unset=-1;
 layout.compare_exchange_strong(unset,format);if(layout.load()!=format)return;
 Sample s{};memcpy(s.iq,info->buf,128);s.rssi=info->rx_ctrl.rssi;s.at=millis();
 if(xQueueSend(queue,&s,0)!=pdTRUE)++dropped;
}
void recalibrate(){shape={};baseline={};calibration=window=millis();lastSample=0;windowCount=0;sum=rssiSum=rssiSq=variation=0;score=quality=cadence=0;previousDrops=dropped;count=head=0;state="CALIBRATING";}
}
bool csiActive(){return active.load();}
void csiStop(){if(!active.exchange(false))return;esp_wifi_set_csi(false);esp_wifi_set_csi_rx_cb(nullptr,nullptr);esp_wifi_set_promiscuous(false);esp_wifi_set_mode(WIFI_MODE_NULL);channelLock=oldLock;setRadio(resume);state="STOPPED";strlcpy(message,"CSI stopped / radar restored",sizeof(message));}
bool csiStart(){using namespace dragon::toolkit;
 if(reconOwnsRadio()||bleLabOwnsRadio()||status.recording||!target.hasAp){strlcpy(message,"Stop other work and select a fresh AP first",sizeof(message));return false;}
 bool fresh=false;for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used&&!memcmp(aps[i].mac,target.bssid,6)&&millis()-aps[i].seen<30000&&aps[i].channel==target.channel)fresh=true;
 if(!fresh){strlcpy(message,"Selected AP is stale; survey and select it again",sizeof(message));return false;}
 if(!queue){if(!memory)memory=(uint8_t*)ps_malloc(32*sizeof(Sample));if(!memory)return false;queue=xQueueCreateStatic(32,sizeof(Sample),memory,&control);if(!queue)return false;}
 Sample stale;while(xQueueReceive(queue,&stale,0)==pdTRUE){}
 resume=radioEnabled;oldLock=channelLock;memcpy(source,target.bssid,6);channel=target.channel;setRadio(false);active=true;dropped=0;callbacks=0;sourcePackets=0;lastLength=lastMode=0;layout=-1;total=0;started=millis();recalibrate();
 wifi_csi_config_t cfg{};cfg.lltf_en=true;cfg.channel_filter_en=true;
 esp_err_t err=esp_wifi_set_mode(WIFI_MODE_STA);
 if(err==ESP_OK)err=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
 if(err==ESP_OK)err=esp_wifi_set_csi_config(&cfg);
 if(err==ESP_OK)err=esp_wifi_set_csi_rx_cb(received,nullptr);
 if(err==ESP_OK)err=esp_wifi_set_csi(true);
 if(err==ESP_OK)err=esp_wifi_set_promiscuous(true);
 if(err!=ESP_OK){csiStop();snprintf(message,sizeof(message),"CSI setup: %s",esp_err_to_name(err));return false;}
 strlcpy(message,"Keep AP and detector still during calibration",sizeof(message));return true;
}
void csiLoop(){if(!active)return;uint32_t now=millis();if(now-started>=900000){csiStop();return;}
 Sample s;for(unsigned i=0;i<32&&xQueueReceive(queue,&s,0)==pdTRUE;++i){float value=shape.observe(s.iq,sizeof(s.iq));if(value<0)continue;lastSample=s.at;++total;++windowCount;sum+=value;rssiSum+=s.rssi;rssiSq+=float(s.rssi)*s.rssi;}
 if(now-window<1000)return;
 cadence=windowCount*1000u/(now-window);uint32_t losses=dropped.load()-previousDrops;previousDrops=dropped;
 variation=windowCount?sum/windowCount:0;float mean=windowCount?rssiSum/windowCount:0;
 float deviation=std::sqrt(std::max(0.0f,windowCount?rssiSq/windowCount-mean*mean:0));
 bool usable=cadence>=5&&now-lastSample<1500&&losses<=2&&deviation<6;
 quality=usable?unsigned(std::max(0.0f,100-deviation*8-losses*10)):0;
 if(now-calibration<30000){state="CALIBRATING";if(usable)baseline.add(variation);score=0;}
 else if(!usable||baseline.count<20){state="LOW DATA";score=0;}
 else {score=baseline.score(variation);state=variation>baseline.threshold()?"CHANGE":"STABLE";}
 history[head]={now,uint16_t(std::min(cadence,65535u)),uint8_t(score),uint8_t(quality),variation};head=(head+1)%120;count=std::min(count+1,120u);
 window=now;windowCount=0;sum=rssiSum=rssiSq=0;
}
void csiStatus(char* out,size_t capacity){snprintf(out,capacity,"%s / CH %u\nChange score %u / 100\nSignal quality %u%% / %u samples/s\nSamples %lu / queue drops %lu\nCalibration %u / 30 seconds\n\n%s\n\nRadio-path changes, not a person or occupancy detector.",state,channel,score,quality,cadence,total,dropped.load(),calibration?std::min(30u,unsigned((millis()-calibration)/1000)):0,message);}
bool csiCommand(const String& c){if(!c.startsWith("csi "))return false;
 if(c=="csi start")Serial.println(csiStart()?"RECON_OK CSI started":"RECON_ERROR fresh AP / busy / CSI setup");
 else if(c=="csi stop"){csiStop();Serial.println("RECON_OK");}
 else if(c=="csi calibrate"&&active){recalibrate();Serial.println("RECON_OK calibrating");}
 else if(c=="csi status"){Serial.printf("RECON_CSI active=%u state=%s channel=%u score=%u quality=%u rate=%u samples=%lu drops=%lu callbacks=%lu source_packets=%lu last_length=%u last_mode=%u layout=%d baseline=%u variation=%.6f threshold=%.6f message=%s\nRECON_OK\n",active.load(),state,channel,score,quality,cadence,total,dropped.load(),callbacks.load(),sourcePackets.load(),lastLength.load(),lastMode.load(),layout.load(),baseline.count,variation,baseline.threshold(),message);}
 else if(c=="csi history"){Serial.println("RECON_CSI_CSV uptime_ms,samples_per_second,change_score,signal_quality,variation");for(unsigned i=0;i<count;++i){auto& p=history[(head+120-count+i)%120];Serial.printf("RECON_CSI_POINT %lu,%u,%u,%u,%.6f\n",p.at,p.rate,p.score,p.quality,p.variation);}Serial.println("RECON_OK");}
 else Serial.println("RECON_ERROR CSI command or inactive");return true;
}
#endif
