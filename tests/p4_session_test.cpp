#include "p4_session_log.h"
#include "storage_replace.h"
#include "ble_profile.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <cstdlib>
using String=std::string;
using namespace dragon::mission;

namespace {
uint32_t now=UINT32_MAX-100;
bool sdReady=true,exported=false,slotReady=true,busy=false,gatt=false,standby=false,radioEnabled=true;
bool partial=false,failManifest=false;
int sdMutex=1,held=0,mode=0;
constexpr int pdTRUE=1;
constexpr const char* FILE_WRITE="w";
std::map<String,String> blobs;std::set<String> dirs;
const char* missionsRoot="/dragon/missions";
const char* files[]={"obs.rlog","probes.rlog","relations.rlog","session.json"};
const char* headers[]={"RCN1","PRB1","REL1"};
struct {bool privacy=false;} detector;
bool usbDriveOn(){return exported;}
bool p4CoprocReady(){return slotReady;}
bool p4CoprocBusy(){return busy;}
bool p4GattBusy(){return gatt;}
bool radioStandby(){return standby;}
int p4RadarScanMode(){return mode;}
int pdMS_TO_TICKS(int n){return n;}
int xSemaphoreTake(int,int){assert(!held);return ++held;}
void xSemaphoreGive(int){assert(held==1);--held;}
uint32_t millis(){return now;}
uint32_t esp_random(){static uint32_t v=0;return ++v;}
void* ps_calloc(size_t n,size_t s){return calloc(n,s);}
void strlcpy(char* out,const char* s,size_t cap){snprintf(out,cap,"%s",s);}
bool safe(const char* s){return dragon::lab::safeName(s);}
size_t sessionLimit(){return 64u<<20;}
String directory(const char* s){return String(missionsRoot)+"/"+s;}
struct File{
 String path;size_t cursor=0;
 explicit operator bool()const{return !path.empty();}
 bool isDirectory()const{return dirs.count(path);}
 File openNextFile(){return {};}
 size_t size()const{return blobs.at(path).size();}
 bool seek(size_t p){cursor=p;return cursor<=size();}
 size_t read(uint8_t* out,size_t n){assert(held);auto& b=blobs.at(path);if(cursor>b.size())return 0;n=std::min(n,b.size()-cursor);memcpy(out,b.data()+cursor,n);cursor+=n;return n;}
 size_t write(const uint8_t* data,size_t n){assert(held);if(partial && path.find("/obs.rlog")!=String::npos && n>4)n/=2;auto& b=blobs.at(path);b.replace(cursor,n,reinterpret_cast<const char*>(data),n);cursor+=n;return n;}
 bool getWriteError()const{return false;}
 void flush(){}void close(){path.clear();}
};
}
namespace fs {struct FS{
 uint64_t totalBytes(){return 512ull<<20;}uint64_t usedBytes(){return 10000;}
 bool exists(const String& s){assert(held);return dirs.count(s)||blobs.count(s);}
 bool mkdir(const String& s){assert(held);return dirs.insert(s).second;}
 bool rmdir(const String& s){assert(held);return dirs.erase(s);}
 bool remove(const String& s){assert(held);return blobs.erase(s);}
 File open(const String& s,const char* mode="r"){assert(held);if(!strcmp(mode,"w"))blobs[s].clear();return exists(s)?File{s}:File{};}
 bool rename(const String& src,const String& dst){assert(held);if(!blobs.count(src)||exists(dst)||(failManifest && src.find(".part")!=String::npos))return false;blobs[dst]=blobs.at(src);blobs.erase(src);return true;}
};}
namespace {
fs::FS CardFS;
#include "mission_record_core.inc"
bool missionActive(){return session && session->active;}
}
#include "p4_sessions.inc"

int main(){
 const uint8_t addr[6]={2,3,4,5,6,7};
 const String name=directory("known"),obs=name+"/obs.rlog",json=name+"/session.json";
 assert(!p4StartSession(false,"../bad"));
 exported=true;assert(!p4StartSession(false,"known"));exported=false;
 gatt=true;assert(!p4StartSession(false,"known"));gatt=false;
 mode=2;assert(!p4StartSession(false,"known"));mode=0;
 assert(p4StartSession(false,"known") && missionActive());
 assert(blobs[obs]=="RCN1" && blobs[name+"/probes.rlog"]=="PRB1" && blobs[name+"/relations.rlog"]=="REL1");
 p4RecordSessionContact(true,addr,0,-50,0,nullptr,0,now);
 p4RecordSessionContact(false,addr,0,-50,6,nullptr,0,now-1);
 assert(session->bytes==12);
 now+=120;p4RecordSessionContact(false,addr,0,-50,6,nullptr,0,now);
 p4RecordSessionContact(false,addr,0,-50,6,nullptr,0,now+1);
 now+=30;p4RecordSessionContact(false,addr,0,-75,6,nullptr,0,now);
 partial=true;p4FinishSession("Stopped by user");
 assert(missionActive() && finalizing && session->buffers[0].size==42 && durableBytes==4);
 assert(blobs[obs].size()==25 && blobs[json].find("\"state\":\"active\"")!=String::npos);
 auto good=blobs;blobs[json]="another card";partial=false;
 now+=1000;p4SessionLoop();assert(missionActive() && blobs[json]=="another card" && blobs[obs].size()==25);
 blobs=good;failManifest=true;now+=1000;p4SessionLoop();
 assert(missionActive() && session->buffers[0].size==0 && durableBytes==46 && blobs[obs].size()==46);
 failManifest=false;now+=1000;p4SessionLoop();assert(!missionActive());
 assert(blobs[json].find("\"state\":\"complete\"")!=String::npos && blobs[json].find("\"ap\":1")!=String::npos && blobs[json].find("\"repeat\":1")!=String::npos);
 assert(blobs[obs][4]==120 && blobs[obs][4+19]==6 && blobs[obs][4+20]=='a' && blobs[obs][25+20]=='A' && !memcmp(blobs[obs].data()+16,addr,6));
 assert(!p4StartSession(false,"known"));
 detector.privacy=true;assert(p4StartSession(true,"private"));
 uint8_t beacon[27]={26,0xff,0x4c,0,2,0x15};
 p4RecordSessionContact(true,addr,2,-62,0,beacon,sizeof(beacon),now);
 detector.privacy=false;now+=100;
 uint8_t other[6]={8,7,6,5,4,3};uint8_t malformed[]={26,0xff,0x4c,0,2,0x15};
 p4RecordSessionContact(true,other,3,-66,0,malformed,sizeof(malformed),now);
 now+=dragon::p4::SessionPeriod;p4SessionLoop();assert(!missionActive());
 const auto& privateObs=blobs[directory("private")+"/obs.rlog"];const char zeros[6]={};
 assert(privateObs.size()==46 && !memcmp(privateObs.data()+16,zeros,6) && !memcmp(privateObs.data()+37,zeros,6));
 assert(privateObs[4+20]=='i' && privateObs[25+20]=='b' && privateObs[4+19]==0);
 assert(p4StartSession(true,"standby"));standby=true;p4SessionLoop();
 assert(!missionActive() && !p4StartSession(true,"blocked"));standby=false;p4SessionLoop();assert(!missionActive());
 free(session);return 0;
}
