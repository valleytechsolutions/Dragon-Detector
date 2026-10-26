#include "network_tools.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "recon_internal.h"
#include "ble_lab.h"
#include "ble_profile.h"
#include "ble_insights.h"
#include "network_packets.h"
#include <Preferences.h>
#include <esp_netif.h>
#include <esp_wifi_default.h>
#include <esp_wifi.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <esp_event.h>
#include <atomic>
#ifdef DRAGON_WIFI_CLOCK
#include <esp_netif_sntp.h>
#include "clock_sync.h"
#endif
namespace {
struct Profile {char name[33]{};char password[64]{};};
Profile profiles[8];Preferences prefs;
Profile joiningProfile;
bool active=false,connected=false,resume=false,scanning=false,waiting=false;
uint8_t oldLock=0;uint32_t started=0,lastCheck=0,sentAt=0,nextAt=0;
esp_netif_t* iface=nullptr;esp_netif_ip_info_t address{};int fd=-1;
esp_event_handler_instance_t wifiEvents=nullptr;std::atomic<unsigned> disconnectReason{0},disconnectCount{0};
unsigned attempts=0,handledDisconnect=0;uint32_t lastAttempt=0;
void wifiEvent(void*,esp_event_base_t,int32_t event,void* data){if(event==WIFI_EVENT_STA_DISCONNECTED&&data){disconnectReason=static_cast<wifi_event_sta_disconnected_t*>(data)->reason;++disconnectCount;}}
uint32_t nextHost=0,lastHost=0,currentHost=0,nonce=0;uint16_t identifier=0,sequence=0;
unsigned sent=0,received=0,missed=0,errors=0;bool dirty=false;
struct Result {uint32_t host;uint16_t elapsed;};Result results[254];unsigned count=0;
char message[112]="Choose a saved network or add one in the web tools";
#ifdef DRAGON_WIFI_CLOCK
int timeProfile=-1;
bool timeOwned=false,timeWaiting=false,timeService=false;
uint32_t timeStarted=0,timePrevious=0,timeLastSync=0;
std::atomic<int64_t> timeReceived{0};
dragon::ClockSyncSchedule timeSchedule;
const char* timeResult="disabled";
void stopTimeService(){if(timeService){esp_netif_sntp_deinit();timeService=false;}timeReceived=0;timeWaiting=false;}
bool startTimeService(){
 if(timeService||!connected)return false;
 esp_sntp_config_t cfg=ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,ESP_SNTP_SERVER_LIST("time.cloudflare.com","pool.ntp.org"));
 cfg.wait_for_sync=false;
 cfg.sync_cb=[](timeval* tv){if(tv)timeReceived.store(tv->tv_sec);};
 timeReceived=0;timeStarted=millis();timePrevious=wallTime();timeResult="syncing";
 timeService=esp_netif_sntp_init(&cfg)==ESP_OK;
 if(!timeService)timeResult="unavailable";
 return timeService;
}
bool requestTime(){
 if(timeWaiting||timeService)return false;
 if(active){if(!connected||scanning)return false;timeWaiting=true;if(startTimeService())return true;timeWaiting=false;return false;}
 if(timeProfile<0||!networkConnect(timeProfile,true))return false;
 timeOwned=timeWaiting=true;timeResult="joining";return true;
}
void finishTime(bool ok){
 stopTimeService();timeSchedule.finished(millis(),ok);
 if(timeOwned){networkStop();timeOwned=false;}
 timeResult=ok?"synced":"failed";
 if(ok)timeLastSync=wallTime();
}
#endif
void note(const char* s){strlcpy(message,s,sizeof(message));}
String ip(uint32_t host){char s[16];snprintf(s,sizeof(s),"%u.%u.%u.%u",unsigned(host>>24),unsigned((host>>16)&255),unsigned((host>>8)&255),unsigned(host&255));return s;}
bool valid(const Profile& p){size_t n=strnlen(p.name,33),k=strnlen(p.password,64);if(!n||n>32||(k&&k<8)||k>63)return false;for(unsigned i=0;i<n;++i)if(uint8_t(p.name[i])<32||uint8_t(p.name[i])>126)return false;for(unsigned i=0;i<k;++i)if(uint8_t(p.password[i])<32||uint8_t(p.password[i])>126)return false;return true;}
bool store(unsigned slot,const Profile& p){char key[4];snprintf(key,sizeof(key),"p%u",slot);uint8_t bytes[sizeof(Profile)+4];memcpy(bytes,&p,sizeof(p));uint32_t crc=dragon::lab::checksum(bytes,sizeof(p));memcpy(bytes+sizeof(p),&crc,4);if(prefs.putBytes(key,bytes,sizeof(bytes))!=sizeof(bytes))return false;profiles[slot]=p;return true;}
void scanStop(){if(fd>=0){close(fd);fd=-1;}scanning=waiting=false;}
bool scanStart(uint32_t first,uint32_t last){
 if(radioStandby()){note("Leave clock or timer to scan");return false;}
 uint32_t own=ntohl(address.ip.addr),mask=ntohl(address.netmask.addr);
 if(!active||!connected||scanning||!dragon::net::range(first,last,own,mask)){note("Join first; choose up to 254 local subnet hosts");return false;}
 if(dirty){note("Save or clear scan results before another scan");return false;}
 fd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);if(fd<0||fcntl(fd,F_SETFL,O_NONBLOCK)<0){scanStop();note("Ping socket unavailable");return false;}
 sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=address.ip.addr;
 if(bind(fd,(sockaddr*)&local,sizeof(local))){scanStop();note("Cannot bind joined interface");return false;}
 nextHost=first;lastHost=last;identifier=esp_random();nonce=esp_random();sequence=0;count=sent=received=missed=errors=0;scanning=true;waiting=false;nextAt=millis();note("Checking local hosts; silence does not mean offline");return true;
}
void scanLoop(uint32_t now){if(!scanning)return;
 if(waiting){uint8_t packet[256];sockaddr_in peer{};socklen_t len=sizeof(peer);for(unsigned i=0;i<4;++i){int n=recvfrom(fd,packet,sizeof(packet),MSG_DONTWAIT,(sockaddr*)&peer,&len);if(n<0)break;
 if(ntohl(peer.sin_addr.s_addr)==currentHost&&dragon::net::reply(packet,n,identifier,sequence,nonce)){++received;if(count<254)results[count++]={currentHost,uint16_t(std::min<uint32_t>(65535,now-sentAt))};dirty=true;waiting=false;break;}}
 if(waiting&&now-sentAt>=800){++missed;waiting=false;dirty=true;}}
 if(waiting||int32_t(now-nextAt)<0)return;
 if(nextHost>lastHost){scanStop();note("Scan complete; Save CSV or clear results");return;}
 if(nextHost==ntohl(address.ip.addr)){++nextHost;return;}
 currentHost=nextHost++;uint8_t packet[16];dragon::net::echo(packet,identifier,++sequence,nonce);sockaddr_in peer{};peer.sin_family=AF_INET;peer.sin_addr.s_addr=htonl(currentHost);
 int n=sendto(fd,packet,sizeof(packet),0,(sockaddr*)&peer,sizeof(peer));++sent;dirty=true;sentAt=now;nextAt=now+300;waiting=n==sizeof(packet);if(!waiting)++errors;
}
bool decodeProfile(const String& c,unsigned& slot,Profile& p){char name[65],pass[127],tail;size_t n=0,k=0;if(sscanf(c.c_str(),"net profile put %u %64s %126s %c",&slot,name,pass,&tail)!=3||slot>=8)return false;
 if(!dragon::lab::decodeHex(name,(uint8_t*)p.name,32,n)||!n||memchr(p.name,0,n))return false;p.name[n]=0;
 if(strcmp(pass,"-")){if(!dragon::lab::decodeHex(pass,(uint8_t*)p.password,63,k)||memchr(p.password,0,k))return false;}p.password[k]=0;return valid(p);
}
}
void networkBegin(){prefs.begin("dd-net",false);for(unsigned i=0;i<8;++i){char key[4];snprintf(key,sizeof(key),"p%u",i);uint8_t data[sizeof(Profile)+4]{};if(prefs.getBytesLength(key)!=sizeof(data)||prefs.getBytes(key,data,sizeof(data))!=sizeof(data))continue;uint32_t crc;memcpy(&crc,data+sizeof(Profile),4);Profile p;memcpy(&p,data,sizeof(p));if(dragon::lab::checksum(data,sizeof(Profile))==crc&&valid(p))profiles[i]=p;}
#ifdef DRAGON_WIFI_CLOCK
 int saved=prefs.getChar("time-profile",-1);if(saved>=0&&saved<8&&valid(profiles[saved]))timeProfile=saved;
 timeResult=timeProfile>=0?"scheduled":"disabled";
#endif
}
bool networkActive(){return active;}
bool networkConnected(){return active&&connected;}
unsigned networkProfiles(char names[][33],unsigned slots[],unsigned cap){unsigned n=0;for(unsigned i=0;i<8&&n<cap;++i)if(valid(profiles[i])){strlcpy(names[n],profiles[i].name,33);slots[n++]=i;}return n;}
void networkStop(){
#ifdef DRAGON_WIFI_CLOCK
 if(timeWaiting||timeService){stopTimeService();timeSchedule.finished(millis(),false);timeResult="cancelled";}
 timeOwned=false;
#endif
 if(!active&&!iface)return;scanStop();esp_wifi_disconnect();wifi_config_t blank{}; // Clear the driver's RAM copy as well as disconnecting.
 esp_wifi_set_config(WIFI_IF_STA,&blank);esp_wifi_set_mode(WIFI_MODE_NULL);
 if(wifiEvents){esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiEvents);wifiEvents=nullptr;}
 if(iface){esp_netif_destroy_default_wifi(iface);iface=nullptr;}active=connected=false;address={};memset(&joiningProfile,0,sizeof(joiningProfile));channelLock=oldLock;setRadio(resume);note("Disconnected / previous radar state restored");}
static bool connectProfile(const Profile& profile,bool clockOnly){using namespace dragon::toolkit;
 if(radioStandby()&&!clockOnly){note("Leave clock or timer to use network tools");return false;}
 constexpr unsigned minimumHeap=
#ifdef BOARD_GPS_LC76G
  24000; // This profile allocates TLS buffers from its 8 MiB PSRAM.
#else
  35000;
#endif
 if(active||!valid(profile)||reconOwnsRadio()||reconStorageBusy()||bleLabOwnsRadio()||status.recording||ESP.getFreeHeap()<minimumHeap){note("Choose a saved profile; stop other radio sessions");return false;}
 esp_err_t err=esp_netif_init();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return false;
 iface=esp_netif_create_default_wifi_sta();if(!iface){note("Network interface unavailable");return false;}
 joiningProfile=profile;resume=radioEnabled;oldLock=channelLock;setRadio(false);active=true;connected=false;address={};started=millis();lastCheck=0;disconnectReason=0;disconnectCount=0;handledDisconnect=0;attempts=1;lastAttempt=started;
 if(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiEvent,nullptr,&wifiEvents)!=ESP_OK){networkStop();return false;}
 wifi_config_t cfg{};memcpy(cfg.sta.ssid,joiningProfile.name,strlen(joiningProfile.name));memcpy(cfg.sta.password,joiningProfile.password,strlen(joiningProfile.password));cfg.sta.threshold.authmode=*joiningProfile.password?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;cfg.sta.pmf_cfg.capable=true;cfg.sta.pmf_cfg.required=false;
 err=esp_wifi_set_mode(WIFI_MODE_STA);if(err==ESP_OK)err=esp_wifi_set_config(WIFI_IF_STA,&cfg);memset(cfg.sta.password,0,sizeof(cfg.sta.password));memset(joiningProfile.password,0,sizeof(joiningProfile.password));if(err==ESP_OK)err=esp_wifi_connect();
 if(err!=ESP_OK){networkStop();note("Wi-Fi connection setup failed");return false;}note("Joining saved network / 25-second deadline");return true;
}
bool networkConnect(unsigned index,bool clockOnly){return index<8&&connectProfile(profiles[index],clockOnly);}
bool networkConnectCredentials(const char* ssid,const char* password){
 Profile p;if(strlen(ssid)>32||strlen(password)>63)return false;strlcpy(p.name,ssid,sizeof(p.name));strlcpy(p.password,password,sizeof(p.password));
 bool ok=connectProfile(p,false);memset(&p,0,sizeof(p));return ok;
}
void networkLoop(){uint32_t now=millis();
#ifdef DRAGON_WIFI_CLOCK
 if(!active&&timeProfile>=0&&timeSchedule.due(now)&&!reconOwnsRadio()&&!bleLabOwnsRadio()&&!reconStorageBusy()){
   if(!requestTime())timeSchedule.finished(now,false);
 }
 if(timeWaiting&&connected&&!timeService&&!startTimeService())finishTime(false);
 if(timeService){int64_t epoch=timeReceived.exchange(0);
   if(epoch){bool ok=dragon::validEpoch(epoch)&&configureClock(uint32_t(epoch));
     if(!ok&&timePrevious)setWallTime(timePrevious+(now-timeStarted)/1000);
     finishTime(ok);
   }else if(now-timeStarted>=15000)finishTime(false);
 }
#endif
 if(!active)return;if(now-started>=15*60*1000u){networkStop();note("15-minute network limit / radar restored");return;}
 if(now-lastCheck>=300){lastCheck=now;wifi_ap_record_t ap{};esp_netif_ip_info_t got{};bool online=esp_wifi_sta_get_ap_info(&ap)==ESP_OK&&esp_netif_get_ip_info(iface,&got)==ESP_OK&&got.ip.addr;
 if(online){address=got;if(!connected){connected=true;note("Connected / ping gateway or scan local subnet");}}
 else if(connected){networkStop();note("Network lost / radar restored");return;}
 else if(now-started>25000){networkStop();note("Join timed out / check Wi-Fi settings");return;}
 else if(attempts<3&&disconnectCount.load()!=handledDisconnect&&now-lastAttempt>=1500){handledDisconnect=disconnectCount.load();++attempts;lastAttempt=now;esp_err_t retry=esp_wifi_connect();if(retry!=ESP_OK){networkStop();note("Connection retry failed / radar restored");return;}snprintf(message,sizeof(message),"Joining / attempt %u of 3 / reason %u",attempts,disconnectReason.load());}}
 scanLoop(now);
}
bool networkPingGateway(){uint32_t gw=ntohl(address.gw.addr);return scanStart(gw,gw);}
bool networkScanLocal(){uint32_t own=ntohl(address.ip.addr),mask=ntohl(address.netmask.addr);if(!mask)return false;uint32_t narrow=mask|0xffffff00u;uint32_t base=own&narrow;uint32_t last=base|~narrow;uint32_t first=base+1;--last;if(first==own)++first;if(last==own)--last;return scanStart(first,last);}
void networkStatus(char* out,size_t cap,bool readable){esp_netif_dns_info_t dns{};if(iface&&connected)esp_netif_get_dns_info(iface,ESP_NETIF_DNS_MAIN,&dns);wifi_ap_record_t ap{};bool associated=active&&esp_wifi_sta_get_ap_info(&ap)==ESP_OK;
 const char* bootConnection="No automatic connection at boot.";
#ifdef DRAGON_WIFI_CLOCK
 if(timeProfile>=0)bootConnection="Saved Wi-Fi clock recovery enabled.";
#endif
 if(readable&&!connected){snprintf(out,cap,"%s\n%s\n\n%s\n\nLast scan: %u sent / %u replied\n%u no reply / %u send errors\n\n%s",active?"JOINING / WAITING FOR IP":"DISCONNECTED",active?joiningProfile.name:"Choose Saved networks above",bootConnection,sent,received,missed,errors,message);return;}
 if(readable)snprintf(out,cap,"%s\n%s\nIP %s\nGateway %s\nMask %s / CH%u\nDNS %s / %d dBm\n\nPing: %u sent / %u replied\n%u no reply / %u send errors\n%s\n\n%s",connected?"NETWORK CONNECTED":active?"JOINING":"DISCONNECTED",active?joiningProfile.name:"No automatic connection",ip(ntohl(address.ip.addr)).c_str(),ip(ntohl(address.gw.addr)).c_str(),ip(ntohl(address.netmask.addr)).c_str(),ap.primary,ip(ntohl(dns.ip.u_addr.ip4.addr)).c_str(),ap.rssi,sent,received,missed,errors,scanning?"SCAN RUNNING":"SCAN IDLE",message);
 else snprintf(out,cap,"active=%u connected=%u scanning=%u ip=%s gateway=%s mask=%s dns=%s channel=%u rssi=%d associated=%u disconnect_reason=%u attempts=%u sent=%u received=%u timeouts=%u errors=%u retained=%u dirty=%u message=%s",active,connected,scanning,ip(ntohl(address.ip.addr)).c_str(),ip(ntohl(address.gw.addr)).c_str(),ip(ntohl(address.netmask.addr)).c_str(),ip(ntohl(dns.ip.u_addr.ip4.addr)).c_str(),ap.primary,ap.rssi,associated,disconnectReason.load(),attempts,sent,received,missed,errors,count,dirty,message);
}
bool networkSaveResults(){if(scanning||!dirty)return false;auto* fs=bleLabStorage();if(!fs||!bleLabStorageSpace(32768))return false;char names[32][48];size_t sizes[32];if(dragon::toolkit::fileList(names,sizes,32)>=32)return false;fs->mkdir("/captures");char path[64];snprintf(path,sizeof(path),"/captures/net-%08lx.csv",esp_random());String temp=String(path)+".part";if(fs->exists(path)||fs->exists(temp))return false;File f=fs->open(temp,FILE_WRITE);if(!f)return false;bool ok=f.printf("# sent=%u,replied=%u,no_reply=%u,send_errors=%u\nipv4,rtt_ms\n",sent,received,missed,errors)>0;for(unsigned i=0;i<count&&ok;++i)ok=f.printf("%s,%u\n",ip(results[i].host).c_str(),results[i].elapsed)>0;f.flush();f.close();ok=ok&&fs->rename(temp,path);if(ok){dirty=false;snprintf(message,sizeof(message),"Saved %s",path+10);}else fs->remove(temp);return ok;}
bool networkCommand(const String& c){if(!c.startsWith("net "))return false;unsigned n;char tail;
#ifdef DRAGON_WIFI_CLOCK
 if(c=="net time status"){Serial.printf("RECON_CLOCK enabled=%u profile=%d busy=%u last=%lu result=%s\nRECON_OK\n",timeProfile>=0,timeProfile,timeWaiting||timeService,timeLastSync,timeResult);return true;}
 if(c=="net time off"){
   if(prefs.putChar("time-profile",-1)!=1){Serial.println("RECON_ERROR time preference save failed");return true;}
   timeProfile=-1;if(timeOwned)networkStop();else stopTimeService();timeResult="disabled";Serial.println("RECON_OK clock recovery disabled");return true;
 }
 if(c.startsWith("net time profile ")){
   bool ok=sscanf(c.c_str(),"net time profile %u %c",&n,&tail)==1&&n<8&&valid(profiles[n])&&!active;
   if(ok)ok=prefs.putChar("time-profile",n)==1;
   if(ok){timeProfile=n;timeSchedule.next=millis();timeResult="scheduled";}
   Serial.println(ok?"RECON_OK clock recovery enabled":"RECON_ERROR save a network first or disconnect");return true;
 }
 if(c=="net time sync"){Serial.println(requestTime()?"RECON_OK synchronizing time":"RECON_ERROR choose a time profile or join Wi-Fi first");return true;}
#endif
 if(c=="net status"){char s[480];networkStatus(s,sizeof(s));Serial.printf("RECON_NETWORK %s\nRECON_OK\n",s);}
 else if(c=="net disconnect"){networkStop();Serial.println("RECON_OK");}
 else if(c=="net profiles"){for(unsigned i=0;i<8;++i)if(valid(profiles[i]))Serial.printf("RECON_NETWORK_PROFILE %u secured=%u name=%s\n",i,*profiles[i].password!=0,profiles[i].name);Serial.println("RECON_OK");}
 else if(c.startsWith("net profile put ")){Profile p;bool ok=!active&&decodeProfile(c,n,p)&&store(n,p);memset(&p,0,sizeof(p));Serial.println(ok?"RECON_OK profile saved":"RECON_ERROR invalid profile or busy");}
 else if(sscanf(c.c_str(),"net profile remove %u %c",&n,&tail)==1&&c.startsWith("net profile remove ")&&n<8&&!active){char key[4];snprintf(key,sizeof(key),"p%u",n);bool ok=!prefs.isKey(key)||prefs.remove(key);if(ok)memset(&profiles[n],0,sizeof(Profile));
#ifdef DRAGON_WIFI_CLOCK
   if(ok&&timeProfile==int(n)){timeProfile=-1;prefs.putChar("time-profile",-1);timeResult="disabled";}
#endif
   Serial.println(ok?"RECON_OK":"RECON_ERROR profile removal");}
 else if(sscanf(c.c_str(),"net connect %u %c",&n,&tail)==1&&c.startsWith("net connect ")&&n<8)Serial.println(networkConnect(n)?"RECON_OK joining":"RECON_ERROR network busy or unavailable");
 else if(c=="net ping gateway")Serial.println(networkPingGateway()?"RECON_OK ping started":"RECON_ERROR join / save results first");
 else if(c=="net scan local")Serial.println(networkScanLocal()?"RECON_OK subnet scan started":"RECON_ERROR join / save results first");
 else if(c=="net scan stop"){scanStop();note("Scan stopped; results retained");Serial.println("RECON_OK");}
 else if(c.startsWith("net scan ")){char a[16],b[16];uint32_t first,last;bool ok=sscanf(c.c_str(),"net scan %15s %15s %c",a,b,&tail)==2&&dragon::net::ipv4(a,first)&&dragon::net::ipv4(b,last)&&scanStart(first,last);Serial.println(ok?"RECON_OK scan started":"RECON_ERROR local range, unsaved results, or connection");}
 else if(c=="net results"){Serial.printf("RECON_NETWORK_SUMMARY sent=%u replied=%u timeouts=%u errors=%u\n",sent,received,missed,errors);for(unsigned i=0;i<count;++i)Serial.printf("RECON_HOST %s %u\n",ip(results[i].host).c_str(),results[i].elapsed);Serial.println("RECON_OK");}
 else if(c=="net save")Serial.println(networkSaveResults()?"RECON_OK results saved":"RECON_ERROR stop scan / no results / storage");
 else if(c=="net clear"&&!scanning){dirty=false;count=sent=received=missed=errors=0;Serial.println("RECON_OK");}
 else Serial.println("RECON_ERROR invalid network command or busy");return true;
}
#endif
