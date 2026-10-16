#include "portal.h"
#include "background_radio.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "recon_internal.h"
#include "ble_lab.h"
#include "ble_profile.h"
#include "dns_reply.h"
#include "ble_insights.h"
#include "attack_frames.h"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_wifi_default.h>
#include <esp_http_server.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <atomic>
namespace {
bool active=false,resume=false;uint8_t oldLock=0;uint32_t started=0;unsigned duration=30;
esp_netif_t* portalNetif=nullptr;httpd_handle_t server=nullptr;int dns=-1;
std::atomic<uint32_t> requests{0},submissions{0},dropped{0};
struct Submission {uint32_t at;uint16_t length;char text[512];};
QueueHandle_t queue=nullptr;StaticQueue_t queueControl;uint8_t* queueMemory=nullptr;Submission* events=nullptr;unsigned eventCount=0;bool logDirty=false;
char name[33]="Dragon-Lab",message[100]="Portal stopped";char* customHtml=nullptr;size_t customLength=0;
const char html[]=R"HTML(<!doctype html><html><meta name="viewport" content="width=device-width"><title>Dragon Detector Lab</title><style>body{background:#071510;color:#d9efe0;font:18px system-ui;margin:3rem auto;max-width:32rem;padding:1rem}input,button{padding:1rem;margin:.5rem 0;width:90%}button{background:#c9fc68;border:0}</style><h1>Dragon Detector Lab</h1><p>This is a local wireless security demonstration. It provides no Internet service.</p><p>Use invented test values only.</p><form method="post" action="/submit"><label>Test identifier<input name="test_id" maxlength="64"></label><label>Test phrase<input name="test_phrase" maxlength="64"></label><button>Submit test</button></form></html>)HTML";
bool emit(const char* p,size_t n){uint32_t at=millis();while(n){size_t size=std::min<size_t>(128,std::min(n,size_t(Serial.availableForWrite())));if(size){size=Serial.write((const uint8_t*)p,size);p+=size;n-=size;at=millis();}else if(millis()-at>5000)return false;delay(1);}return true;}
bool loadHtml(){customLength=0;auto* fs=bleLabStorage();if(!fs)return false;if(!fs->exists("/portal/template.ddh"))return true;File f=fs->open("/portal/template.ddh");if(!f)return false;size_t n=f.size();if(n<5||n>16388){f.close();return false;}if(!customHtml)customHtml=(char*)ps_malloc(16385);uint32_t crc=0;bool ok=customHtml&&f.read((uint8_t*)customHtml,n-4)==n-4&&f.read((uint8_t*)&crc,4)==4&&dragon::lab::checksum((uint8_t*)customHtml,n-4)==crc;f.close();if(ok){customLength=n-4;customHtml[customLength]=0;}return ok;}
void importHtml(const String& command){unsigned n;unsigned long crc;char tail;auto* fs=bleLabStorage();if(active||reconOwnsRadio()||reconStorageBusy()||bleLabOwnsRadio()||dragon::toolkit::status.recording||!fs||sscanf(command.c_str(),"portal html import %u %lx %c",&n,&crc,&tail)!=2||!n||n>16384||!bleLabStorageSpace(n+16384)){Serial.println("RECON_ERROR HTML size, storage, or busy");return;}auto* data=(uint8_t*)ps_malloc(n);if(!data){Serial.println("RECON_ERROR memory");return;}RadioTransfer transfer;Serial.println("RECON_READY");size_t pos=0;uint32_t at=millis(),ack=at;while(pos<n&&millis()-at<5000){size_t z=std::min<size_t>(256,std::min<size_t>(Serial.available(),n-pos));if(z){pos+=Serial.readBytes(data+pos,z);at=ack=millis();Serial.printf("RECON_ACK %u\n\n",unsigned(pos));}else{if(millis()-ack>250){Serial.printf("RECON_ACK %u\n\n",unsigned(pos));ack=millis();}}radioTransferTick();delay(1);}bool ok=pos==n&&dragon::lab::checksum(data,n)==crc&&!memchr(data,0,n);if(ok){fs->mkdir("/portal");File f=fs->open("/portal/template.part",FILE_WRITE);uint32_t check=crc;ok=f&&f.write(data,n)==n&&f.write((uint8_t*)&check,4)==4;f.flush();f.close();ok=ok&&fs->rename("/portal/template.part","/portal/template.ddh");if(!ok)fs->remove("/portal/template.part");}free(data);Serial.println(ok?"RECON_OK template saved":"RECON_ERROR HTML transfer/CRC/storage");}
bool saveLog(){auto* fs=bleLabStorage();if(active||reconOwnsRadio()||reconStorageBusy()||!eventCount||!fs||!bleLabStorageSpace(65536))return false;char names[32][48];size_t sizes[32];if(dragon::toolkit::fileList(names,sizes,32)>=32)return false;fs->mkdir("/captures");char path[64];snprintf(path,sizeof(path),"/captures/portal-%08lx.csv",esp_random());String temp=String(path)+".part";if(fs->exists(path)||fs->exists(temp))return false;File f=fs->open(temp,FILE_WRITE);if(!f)return false;bool ok=f.print("uptime_ms,request_body_hex\n")>0;for(unsigned i=0;i<eventCount&&ok;++i){auto& e=events[i];char line[1060];int n=snprintf(line,sizeof(line),"%lu,",e.at);for(unsigned j=0;j<e.length;++j)n+=snprintf(line+n,sizeof(line)-n,"%02x",uint8_t(e.text[j]));line[n++]='\n';ok=f.write((uint8_t*)line,n)==size_t(n);}f.flush();f.close();ok=ok&&fs->rename(temp,path);if(!ok)fs->remove(temp);else{logDirty=false;snprintf(message,sizeof(message),"Saved %s",path+10);}return ok;}
esp_err_t getPage(httpd_req_t* req){++requests;httpd_resp_set_type(req,"text/html; charset=utf-8");httpd_resp_set_hdr(req,"Cache-Control","no-store");return httpd_resp_send(req,customLength?customHtml:html,customLength?customLength:strlen(html));}
esp_err_t postPage(httpd_req_t* req){if(req->content_len>511){httpd_resp_set_status(req,"413 Payload Too Large");return httpd_resp_sendstr(req,"Test submission limit: 511 bytes");}Submission e{};e.at=millis();while(e.length<req->content_len){int n=httpd_req_recv(req,e.text+e.length,req->content_len-e.length);if(n<=0)return ESP_FAIL;e.length+=n;}if(queue&&xQueueSend(queue,&e,0)==pdTRUE)++submissions;else ++dropped;return httpd_resp_sendstr(req,"Test received. You may disconnect from this lab network.");}
bool validName(const char* p){size_t n=strlen(p);if(!n||n>32)return false;for(size_t i=0;i<n;++i)if(p[i]<32||p[i]>126)return false;return true;}
// Match the raw identity, never a recyclable inventory row number.
bool takeProbe(const char* address,const char* hex){using namespace dragon::toolkit;
 uint8_t mac[6],ssid[32];size_t length=0;
 if(!dragon::tx::parseMac(address,mac,false)||!dragon::lab::decodeHex(hex,ssid,sizeof(ssid),length)||memchr(ssid,0,length))return false;
 for(unsigned i=0;probes&&i<96;++i){auto& p=probes[i];if(!p.used||millis()-p.seen>30000||memcmp(p.mac,mac,6)||p.length!=length||memcmp(p.ssid,ssid,length))continue;
  char text[33];memcpy(text,ssid,length);text[length]=0;if(!validName(text))return false;strlcpy(name,text,33);target.channel=p.channel;return true;}
 return false;
}
}
bool portalActive(){return active;}
void drainLog(){Submission event;for(unsigned i=0;queue&&i<4&&xQueueReceive(queue,&event,0)==pdTRUE;++i){if(eventCount<32){events[eventCount++]=event;logDirty=true;}else ++dropped;}}
void portalStop(){if(!active&&!server&&dns<0)return;if(server){httpd_stop(server);server=nullptr;}drainLog();if(dns>=0){close(dns);dns=-1;}esp_wifi_set_promiscuous(false);esp_wifi_set_mode(WIFI_MODE_NULL);if(portalNetif){esp_netif_destroy_default_wifi(portalNetif);portalNetif=nullptr;}active=false;channelLock=oldLock;setRadio(resume);strlcpy(message,"Portal stopped / radar restored",sizeof(message));}
bool portalStart(){using namespace dragon::toolkit;if(active||logDirty||reconOwnsRadio()||reconStorageBusy()||bleLabOwnsRadio()||status.recording||!validName(name)||ESP.getFreeHeap()<35000)return false;
 if(!loadHtml()){strlcpy(message,"Portal HTML failed CRC; import or reset template",sizeof(message));return false;}
 if(!queue){if(!events)events=(Submission*)ps_calloc(32,sizeof(Submission));if(!queueMemory)queueMemory=(uint8_t*)ps_malloc(4*sizeof(Submission));if(!events||!queueMemory)return false;queue=xQueueCreateStatic(4,sizeof(Submission),queueMemory,&queueControl);if(!queue)return false;}
 if(!portalNetif){esp_err_t err=esp_netif_init();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return false;portalNetif=esp_netif_create_default_wifi_ap();if(!portalNetif)return false;}
 resume=radioEnabled;oldLock=channelLock;setRadio(false);active=true;eventCount=0;requests=0;submissions=0;dropped=0;Submission stale;while(xQueueReceive(queue,&stale,0)==pdTRUE){}
 wifi_config_t cfg{};memcpy(cfg.ap.ssid,name,strlen(name));cfg.ap.ssid_len=strlen(name);cfg.ap.channel=target.channel;cfg.ap.max_connection=2;cfg.ap.authmode=WIFI_AUTH_OPEN;
 esp_err_t err=esp_wifi_set_mode(WIFI_MODE_AP);if(err==ESP_OK)err=esp_wifi_set_config(WIFI_IF_AP,&cfg);
 if(err!=ESP_OK){portalStop();return false;}
 dns=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(53);addr.sin_addr.s_addr=htonl(INADDR_ANY);
 if(dns<0||bind(dns,(sockaddr*)&addr,sizeof(addr))||fcntl(dns,F_SETFL,O_NONBLOCK)<0){portalStop();return false;}
 httpd_config_t http=HTTPD_DEFAULT_CONFIG();http.stack_size=4096;http.max_open_sockets=2;http.max_uri_handlers=2;http.lru_purge_enable=true;http.recv_wait_timeout=2;http.send_wait_timeout=2;http.uri_match_fn=httpd_uri_match_wildcard;
 if(httpd_start(&server,&http)!=ESP_OK){portalStop();return false;}
 httpd_uri_t get{};get.uri="/*";get.method=HTTP_GET;get.handler=getPage;httpd_uri_t post=get;post.method=HTTP_POST;post.handler=postPage;
 if(httpd_register_uri_handler(server,&get)!=ESP_OK||httpd_register_uri_handler(server,&post)!=ESP_OK){portalStop();return false;}
 started=millis();duration=seconds;strlcpy(message,"Local portal on 192.168.4.1 / Home stops",sizeof(message));return true;
}
void portalLoop(){if(!active)return;if(millis()-started>=duration*1000u){portalStop();return;}uint8_t packet[512];sockaddr_in peer{};socklen_t length=sizeof(peer);int n=recvfrom(dns,packet,sizeof(packet),MSG_DONTWAIT,(sockaddr*)&peer,&length);if(n>0){size_t size=dragon::captiveDns(packet,n,sizeof(packet));if(size)sendto(dns,packet,size,0,(sockaddr*)&peer,length);}drainLog();}
void portalStatus(char* out,size_t capacity,bool readable){wifi_sta_list_t stations{};if(active)esp_wifi_ap_get_sta_list(&stations);unsigned left=active?std::max<int>(0,int(duration)-int((millis()-started)/1000)):0;
 if(readable)snprintf(out,capacity,"%s\nAP: %s\n%us remaining / %u clients\nRequests %lu / submissions %lu\nRetained %u / dropped %lu\n\n%s",active?"PORTAL ACTIVE":"PORTAL STOPPED",name,left,stations.num,requests.load(),submissions.load(),eventCount,dropped.load(),message);
 else snprintf(out,capacity,"active=%u name=%s requests=%lu submissions=%lu retained=%u drops=%lu clients=%u left=%u message=%s",active,name,requests.load(),submissions.load(),eventCount,dropped.load(),stations.num,left,message);
}
bool portalCommand(const String& c){if(!c.startsWith("portal "))return false;
 if(c=="portal status"){char text[300];portalStatus(text,sizeof(text));Serial.printf("RECON_PORTAL %s\nRECON_OK\n",text);}
 else if(c=="portal stop"){portalStop();Serial.println("RECON_OK");}
 else if(c.startsWith("portal name ")&&!active){bool ok=validName(c.c_str()+12);if(ok)strlcpy(name,c.c_str()+12,33);Serial.println(ok?"RECON_OK":"RECON_ERROR SSID must be printable, 1-32 bytes");}
 else if(c=="portal start")Serial.println(portalStart()?"RECON_OK":"RECON_ERROR busy, memory, or AP setup");
 else if(c.startsWith("portal probe ")&&!active){char mac[18],hex[65],extra;bool ok=sscanf(c.c_str(),"portal probe %17s %64s %c",mac,hex,&extra)==2&&takeProbe(mac,hex);Serial.println(ok?"RECON_OK selected requested SSID; review before portal start":"RECON_ERROR stale/invalid probe");}
 else if(c=="portal probes"){using namespace dragon::toolkit;for(unsigned i=0;probes&&i<96;++i)if(probes[i].used&&probes[i].length&&millis()-probes[i].seen<=30000){auto& p=probes[i];char ssid[33],hex[65],mac[18];dragon::safeText(ssid,sizeof(ssid),p.ssid,p.length);macText(p.mac,mac);for(unsigned j=0;j<p.length;++j)snprintf(hex+j*2,3,"%02x",p.ssid[j]);Serial.printf("RECON_PORTAL_PROBE %s %s %s\n",mac,hex,ssid);}Serial.println("RECON_OK");}
 else if(c=="portal log"){for(unsigned i=0;i<eventCount;++i){auto& e=events[i];char line[1080];int n=snprintf(line,sizeof(line),"RECON_SUBMIT %lu ",e.at);for(unsigned j=0;j<e.length;++j)n+=snprintf(line+n,sizeof(line)-n,"%02x",uint8_t(e.text[j]));line[n++]='\n';if(!emit(line,n))break;}Serial.println("RECON_OK");}
 else if(c=="portal save")Serial.println(saveLog()?"RECON_OK log saved":"RECON_ERROR stop first / empty / storage");
 else if(c.startsWith("portal html import "))importHtml(c);
 else if(c=="portal html reset"&&!active){auto* fs=bleLabStorage();bool ok=fs&&(!fs->exists("/portal/template.ddh")||fs->remove("/portal/template.ddh"));if(ok)customLength=0;Serial.println(ok?"RECON_OK original template restored":"RECON_ERROR storage");}
 else if(c=="portal clear"&&!active){eventCount=0;logDirty=false;Serial.println("RECON_OK");}
 else Serial.println("RECON_ERROR invalid portal command or busy");return true;
}
#endif
