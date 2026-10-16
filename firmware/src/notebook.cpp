#include "notebook.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "background_radio.h"
using namespace dragon;
analysis::Counters notebookCounters;
analysis::Probe* notebookProbes=nullptr;
notebook::Advert* notebookAdverts=nullptr;
uint32_t notebookRejected=0;
void notebookBegin(){notebookProbes=(analysis::Probe*)ps_calloc(notebook::Capacity,sizeof(analysis::Probe));notebookAdverts=(notebook::Advert*)ps_calloc(notebook::Capacity,sizeof(notebook::Advert));}
void notebookWifi(const uint8_t* p,size_t n,const WifiFrame& f,int rssi,uint8_t channel,uint32_t at){notebookCounters.observe(p,n,f);analysis::observeProbe(notebookProbes,notebook::Capacity,f,rssi,channel,at);}
void notebookBle(const uint8_t* mac,uint8_t type,int rssi,const uint8_t* p,size_t n,size_t boundary,uint32_t at){if(!notebook::observeAdvert(notebookAdverts,notebook::Capacity,mac,type,rssi,p,n,boundary,at))++notebookRejected;}
static bool line(const String& s){size_t pos=0;uint32_t last=millis();while(pos<s.length()){size_t room=Serial.availableForWrite(),n=room?Serial.write((const uint8_t*)s.c_str()+pos,std::min<size_t>(128,std::min(room,s.length()-pos))):0;if(n){pos+=n;last=millis();}else if(millis()-last>5000)return false;radioTransferTick();delay(1);}return true;}
static String hex(const uint8_t* p,size_t n){String s;s.reserve(n*2);for(size_t i=0;i<n;++i){char b[3];snprintf(b,3,"%02x",p[i]);s+=b;}return s;}
bool notebookCommand(const String& c){
 if(c=="notebook status"){unsigned probes=0,adverts=0;for(unsigned i=0;i<notebook::Capacity;++i){probes+=notebookProbes&&notebookProbes[i].used;adverts+=notebookAdverts&&notebookAdverts[i].used;}auto& t=notebookCounters;Serial.printf("NOTEBOOK_STATUS ready=%u probes=%u adverts=%u rejected=%lu frames=%lu beacons=%lu requests=%lu disconnect=%lu eapol=%lu m1=%lu m2=%lu m3=%lu m4=%lu pmkid=%lu sae=%lu retries=%lu\n",notebookProbes&&notebookAdverts,probes,adverts,notebookRejected,t.frames,t.beacons,t.probes,t.disconnect,t.eapol,t.keyMessages[1],t.keyMessages[2],t.keyMessages[3],t.keyMessages[4],t.pmkid,t.sae,t.retries);return true;}
 if(c=="notebook clear"){if(notebookProbes)memset(notebookProbes,0,notebook::Capacity*sizeof(*notebookProbes));if(notebookAdverts)memset(notebookAdverts,0,notebook::Capacity*sizeof(*notebookAdverts));notebookCounters={};notebookRejected=0;Serial.println("NOTEBOOK_OK cleared volatile observations");return true;}
 if(c!="notebook probes"&&c!="notebook adverts")return false;
 bool probes=c.endsWith("probes");size_t bytes=notebook::Capacity*(probes?sizeof(analysis::Probe):sizeof(notebook::Advert));void* source=probes?(void*)notebookProbes:(void*)notebookAdverts;void* copy=source?ps_malloc(bytes):nullptr;
 if(!copy){Serial.println("NOTEBOOK_ERROR memory");return true;}memcpy(copy,source,bytes);RadioTransfer transfer;
 bool ok=line("NOTEBOOK_BEGIN\n");
 if(probes){ok=ok&&line("mac,channel,rssi,seen_ms,count,ssid_hex\n");for(unsigned i=0;i<notebook::Capacity&&ok;++i){auto& a=((analysis::Probe*)copy)[i];if(!a.used)continue;char mac[18];macText(a.mac,mac);ok=line(String(mac)+","+a.channel+","+a.rssi+","+a.seen+","+a.count+","+hex(a.ssid,a.length)+"\n");}}
 else {ok=ok&&line("mac,address_type,rssi,seen_ms,count,changes,adv_length,company,appearance,tx_power,service,protocol,payload_hex\n");for(unsigned i=0;i<notebook::Capacity&&ok;++i){auto& a=((notebook::Advert*)copy)[i];if(!a.used)continue;auto insight=lab::inspectAdvertisement(a.data,a.length,a.boundary);char mac[18];macText(a.mac,mac);ok=line(String(mac)+","+a.type+","+a.rssi+","+a.seen+","+a.count+","+a.changes+","+a.boundary+","+insight.company+","+insight.appearance+","+(insight.hasPower?String(insight.power):String(""))+","+(insight.service?insight.service:"")+","+insight.protocol+","+hex(a.data,a.length)+"\n");}}
 free(copy);Serial.println(ok?"NOTEBOOK_END":"NOTEBOOK_ERROR timeout");return true;
}
#endif
