#include "missions.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "recon_internal.h"
#include "ble_lab.h"
#include "ble_profile.h"
#include "background_radio.h"
#include "packet_helpers.h"
#include "mission_format.h"
#include <esp_system.h>
namespace {
using namespace dragon;using namespace dragon::mission;
constexpr unsigned MaxSeen=512,MaxRelations=128,MaxSessions=32;
#if defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175) || defined(BOARD_STOPWATCH)
constexpr size_t SessionLimit=2*1024*1024;
#else
constexpr size_t SessionLimit=256*1024;
#endif
struct Buffer {uint8_t data[1440];size_t size=0;};
struct Session {Seen seen[MaxSeen];uint8_t relations[MaxRelations][12];unsigned relationCount=0;Buffer buffers[3];uint32_t start=0,lastFlush=0,lastManifest=0,counts[7]{},dropped=0;size_t bytes=12;bool active=false,ble=false;char name[33]{},reason[64]="Idle";}* session=nullptr;
const char* files[]={"obs.rlog","probes.rlog","relations.rlog","session.json"};const char* headers[]={"RCN1","PRB1","REL1"};
bool serialWrite(const uint8_t* p,size_t n){uint32_t last=millis();while(n){size_t k=std::min(n,size_t(Serial.availableForWrite()));if(k){k=Serial.write(p,std::min<size_t>(k,128));p+=k;n-=k;last=millis();}else if(millis()-last>5000)return false;radioTransferTick();delay(1);}return true;}
String directory(const char* name){return String("/missions/")+name;}
String directory(const String& name){return directory(name.c_str());}
bool safe(const char* name){return lab::safeName(name);}
bool append(unsigned file,const uint8_t* p,size_t n){if(!session||!session->active||file>2)return false;auto& b=session->buffers[file];if(n>sizeof(b.data)-b.size||session->bytes+n>SessionLimit){++session->dropped;return false;}memcpy(b.data+b.size,p,n);b.size+=n;session->bytes+=n;return true;}
bool flush(unsigned index){auto& b=session->buffers[index];if(!b.size)return true;auto* fs=bleLabStorage();if(!fs||!bleLabStorageSpace(b.size+32768))return false;File f=fs->open(directory(session->name)+"/"+files[index],FILE_APPEND);if(!f)return false;bool ok=f.write(b.data,b.size)==b.size;f.flush();f.close();if(ok)b.size=0;return ok;}
bool manifest(){auto* fs=bleLabStorage();if(!fs||!session)return false;String path=directory(session->name)+"/session.json";File f=fs->open(path+".part",FILE_WRITE);if(!f)return false;auto* c=session->counts;
 int n=f.printf("{\"schema\":1,\"state\":\"%s\",\"mode\":\"%s\",\"start_ms\":%lu,\"duration_ms\":%lu,\"ap\":%lu,\"station\":%lu,\"ble\":%lu,\"ibeacon\":%lu,\"probe\":%lu,\"repeat\":%lu,\"deauth\":%lu,\"dropped\":%lu,\"gps_fix\":false,\"observations\":\"obs.rlog\",\"probes\":\"probes.rlog\",\"relationships\":\"relations.rlog\",\"capture\":\"\",\"producer\":\"Dragon Detector\",\"reason\":\"%s\",\"bytes\":%u}\n",session->active?"active":"complete",session->ble?"ble":"wifi",session->start,millis()-session->start,c[0],c[1],c[2],c[3],c[4],c[5],c[6],session->dropped,session->reason,unsigned(session->bytes));f.flush();f.close();if(n<100)return false;return fs->rename(path+".part",path);
}
void finish(const char* reason){if(!session||!session->active)return;bool ok=true;for(unsigned i=0;i<3;++i)ok=flush(i)&&ok;session->active=false;strlcpy(session->reason,ok?reason:"Storage write failed",sizeof(session->reason));manifest();}
void observe(char type,uint8_t addressType,const uint8_t* mac,int rssi,uint8_t channel){if(sizeof(session->buffers[0].data)-session->buffers[0].size<ObservationSize||session->bytes+ObservationSize>SessionLimit){++session->dropped;return;}bool full=false;unsigned action=accept(session->seen,MaxSeen,mac,type,addressType,rssi,millis()-session->start,full);if(full)++session->dropped;if(!action)return;uint8_t data[ObservationSize];observation(data,millis()-session->start,mac,rssi,channel,action==2?char(type-'a'+'A'):type);if(append(0,data,sizeof(data))){if(action==2)++session->counts[5];else ++session->counts[type=='a'?0:type=='s'?1:type=='b'?2:3];}}
bool exportFile(const String& path){auto* fs=bleLabStorage();if(!fs||missionActive()||reconStorageBusy())return false;File f=fs->open(path);if(!f||f.isDirectory()||f.size()>SessionLimit)return false;RadioTransfer transfer;Serial.printf("RECON_FILE %u\n",unsigned(f.size()));uint8_t data[128];size_t offset=0;uint32_t crc=0xffffffffu;bool ok=true;
 while(f.available()&&ok){size_t n=f.read(data,sizeof(data));if(!n){ok=false;break;}crc=dragon::crc32(data,n,crc);char line[300];int used=snprintf(line,sizeof(line),"RECON_DATA %u ",unsigned(offset));for(size_t i=0;i<n;++i)used+=snprintf(line+used,sizeof(line)-used,"%02x",data[i]);line[used++]='\n';ok=serialWrite((uint8_t*)line,used);offset+=n;radioTransferTick();}f.close();if(ok)Serial.printf("RECON_END %u %08lx\n",unsigned(offset),crc^0xffffffffu);return ok;
}
}
bool missionActive(){return session&&session->active;}
bool missionStart(bool ble,const char* name){auto* fs=bleLabStorage();if(!fs||!safe(name)||missionActive()||bleLabOwnsRadio()||reconOwnsRadio()||reconStorageBusy()||!radioEnabled||!bleLabStorageSpace(65536))return false;
 fs->mkdir("/missions");File dir=fs->open("/missions");unsigned count=0;for(File f=dir.openNextFile();f;f=dir.openNextFile()){count+=f.isDirectory();f.close();}dir.close();if(count>=MaxSessions||fs->exists(directory(name)))return false;
 if(!session)session=(Session*)ps_calloc(1,sizeof(Session));if(!session)return false;memset(session,0,sizeof(*session));strlcpy(session->name,name,sizeof(session->name));session->ble=ble;session->start=session->lastFlush=session->lastManifest=millis();session->bytes=12;strlcpy(session->reason,"Recording",sizeof(session->reason));
 if(!fs->mkdir(directory(name)))return false;bool ok=true;for(unsigned i=0;i<3;++i){File f=fs->open(directory(name)+"/"+files[i],FILE_WRITE);ok=bool(f)&&f.write((const uint8_t*)headers[i],4)==4&&ok;f.close();}session->active=ok;if(!ok){strlcpy(session->reason,"Cannot create logs",sizeof(session->reason));manifest();return false;}if(!manifest()){session->active=false;strlcpy(session->reason,"Cannot create manifest",sizeof(session->reason));return false;}return true;
}
void missionStop(){finish("Stopped by user");}
void missionLoop(){if(!missionActive())return;if(session->bytes>=SessionLimit-256){finish("Session size limit");return;}if(millis()-session->start>=60*60*1000){finish("One hour limit");return;}if(millis()-session->lastFlush>=500){session->lastFlush=millis();static unsigned next=0;if(!flush(next++%3)){finish("Storage reserve reached");return;}}if(millis()-session->lastManifest>=10000){session->lastManifest=millis();if(!manifest())finish("Manifest write failed");}}
void missionStatus(char* out,size_t cap){if(!session){snprintf(out,cap,"No session started.\nRecords one protocol using the current radio schedule.");return;}snprintf(out,cap,"%s / %s\n%s\n%u bytes / %lu dropped\nAP %lu / Station %lu / BLE %lu\niBeacon %lu / Probes %lu\nUpdates %lu / Disconnect %lu\n\nCurrent radio schedule retained.\nDownloads: web Radio tools.",session->name,session->ble?"BLE":"Wi-Fi",session->reason,unsigned(session->bytes),session->dropped,session->counts[0],session->counts[1],session->counts[2],session->counts[3],session->counts[4],session->counts[5],session->counts[6]);}
void missionWifi(const dragon::WifiFrame& f,const uint8_t* p,size_t n,int rssi,uint8_t channel){if(!missionActive()||session->ble||!f.valid)return;if(f.beacon)observe('a',0,f.source,rssi,channel);
 if(f.probe&&!f.retry){uint8_t data[ProbeSize];probe(data,millis()-session->start,f.source,rssi,channel,f.ssid,f.ssidLength);if(append(1,data,sizeof(data)))++session->counts[4];}
 if((f.deauth||f.disassoc)&&!f.retry){uint8_t data[ObservationSize];observation(data,millis()-session->start,f.source,rssi,channel,'d');if(append(0,data,sizeof(data)))++session->counts[6];}
 uint8_t ap[6],station[6];if(dragon::tx::stationPair(p,n,ap,station)){observe('s',0,station,rssi,channel);uint8_t pair[12];memcpy(pair,station,6);memcpy(pair+6,ap,6);unsigned i=0;for(;i<session->relationCount;++i)if(!memcmp(pair,session->relations[i],12))break;if(i==session->relationCount){if(i==MaxRelations){++session->dropped;return;}if(append(2,pair,12))memcpy(session->relations[session->relationCount++],pair,12);}}
}
void missionBle(const uint8_t* mac,uint8_t type,int rssi,const uint8_t* p,size_t n){if(!missionActive()||!session->ble)return;bool beacon=false;for(size_t i=0;i<n;){size_t k=p[i++];if(!k)break;if(k>n-i)break;if(k>=26&&p[i]==0xff&&p[i+1]==0x4c&&p[i+2]==0&&p[i+3]==2&&p[i+4]==0x15)beacon=true;i+=k;}observe(beacon?'i':'b',type,mac,rssi,0);}
bool surveyCommand(const String& c){if(!c.startsWith("survey "))return false;auto* fs=bleLabStorage();
 if(c=="survey list"){if(fs){File dir=fs->open("/surveys");for(File f=dir.openNextFile();f;f=dir.openNextFile()){String name=f.name();name=name.substring(name.lastIndexOf('/')+1);if(!f.isDirectory()&&name.endsWith(".csv")){name.remove(name.length()-4);if(safe(name.c_str()))Serial.printf("RECON_SURVEY %s %u\n",name.c_str(),unsigned(f.size()));}f.close();}dir.close();}Serial.println("RECON_OK");}
 else if(c.startsWith("survey save ")){String name=c.substring(12),path=String("/surveys/")+name+".csv";bool ok=fs&&safe(name.c_str())&&!reconStorageBusy()&&bleLabStorageSpace(32768)&&!fs->exists(path)&&dragon::toolkit::aps;unsigned total=0;
  if(ok){fs->mkdir("/surveys");File dir=fs->open("/surveys");for(File f=dir.openNextFile();f;f=dir.openNextFile()){++total;f.close();}dir.close();ok=total<32;}
  if(ok){File f=fs->open(path+".part",FILE_WRITE);ok=bool(f);if(ok)ok=f.print("bssid,channel,rssi,security,age_ms,packets,ssid_hex\n")>0;
   for(unsigned i=0;ok&&i<dragon::toolkit::ApCapacity;++i){auto& a=dragon::toolkit::aps[i];if(!a.used)continue;char mac[18],line[200];macText(a.mac,mac);int n=snprintf(line,sizeof(line),"%s,%u,%d,%u,%lu,%lu,",mac,a.channel,a.rssi,a.security,millis()-a.seen,a.packets);auto frame=dragon::parseWifi(a.beacon,a.length);for(unsigned j=0;frame.valid&&j<frame.ssidLength;++j)n+=snprintf(line+n,sizeof(line)-n,"%02x",frame.ssid[j]);line[n++]='\n';ok=f.write((uint8_t*)line,n)==size_t(n);}
   f.flush();f.close();ok=ok&&fs->rename(path+".part",path);if(!ok)fs->remove(path+".part");}
  Serial.println(ok?"RECON_OK survey saved":"RECON_ERROR name exists, capacity, or storage");
 }
 else if(c.startsWith("survey load ")||c.startsWith("survey get ")||c.startsWith("survey remove ")){bool load=c.startsWith("survey load "),remove=c.startsWith("survey remove ");String name=c.substring(load?12:remove?14:11),path=String("/surveys/")+name+".csv";bool ok=fs&&safe(name.c_str())&&!reconStorageBusy();
  if(ok&&remove)ok=fs->remove(path);else if(ok&&!load){ok=exportFile(path);if(ok)return true;}
  else if(ok){File f=fs->open(path);ok=f&&f.size()<=16384;if(ok){RadioTransfer transfer;while(f.available()){String line=f.readStringUntil('\n');if(line.length()>220){ok=false;break;}String output=String("RECON_ARCHIVED ")+line+"\n";if(!serialWrite((uint8_t*)output.c_str(),output.length())){ok=false;break;}}}f.close();}
  Serial.println(ok?"RECON_OK archived observations; select only a fresh live AP for testing":"RECON_ERROR file or name");
 }else Serial.println("RECON_ERROR survey command");return true;
}
bool missionCommand(const String& c){if(surveyCommand(c))return true;if(!c.startsWith("mission "))return false;auto* fs=bleLabStorage();
 if(c=="mission status"){Serial.printf("MISSION_STATUS active=%u name=%s protocol=%s bytes=%u limit=%u dropped=%lu reason=%s\nRECON_OK\n",missionActive(),session?session->name:"none",session&&session->ble?"ble":"wifi",session?unsigned(session->bytes):0,unsigned(SessionLimit),session?session->dropped:0,session?session->reason:"Idle");}
 else if(c=="mission stop"){missionStop();Serial.println("RECON_OK");}
 else if(c.startsWith("mission start ")){char mode[5],name[33],extra;bool ok=sscanf(c.c_str(),"mission start %4s %32s %c",mode,name,&extra)==2&&(!strcmp(mode,"wifi")||!strcmp(mode,"ble"))&&missionStart(!strcmp(mode,"ble"),name);Serial.println(ok?"RECON_OK mission recording; background scanning continues":"RECON_ERROR name exists, storage, or busy");}
 else if(c=="mission list"){if(fs){File d=fs->open("/missions");for(File f=d.openNextFile();f;f=d.openNextFile()){if(f.isDirectory()){String name=f.name();name=name.substring(name.lastIndexOf('/')+1);if(safe(name.c_str()))Serial.printf("MISSION_SAVED %s\n",name.c_str());}f.close();}d.close();}Serial.println("RECON_OK");}
 else if(c.startsWith("mission get ")){char name[33]{},file[24]{},extra;bool ok=sscanf(c.c_str(),"mission get %32s %23s %c",name,file,&extra)==2&&safe(name);bool known=false;for(auto* f:files)known|=!strcmp(file,f);if(!ok||!known||!exportFile(directory(name)+"/"+file))Serial.println("RECON_ERROR mission file unavailable or busy");}
 else if(c.startsWith("mission remove ")){String name=c.substring(15);bool ok=fs&&safe(name.c_str())&&!missionActive()&&!reconStorageBusy()&&fs->exists(directory(name));if(ok){for(auto* f:files){String path=directory(name)+"/"+f;if(fs->exists(path))ok=fs->remove(path)&&ok;}fs->remove(directory(name)+"/session.json.part");ok=fs->rmdir(directory(name))&&ok;}Serial.println(ok?"RECON_OK":"RECON_ERROR cannot remove mission");}
 else Serial.println("RECON_ERROR mission command");return true;
}
#endif
