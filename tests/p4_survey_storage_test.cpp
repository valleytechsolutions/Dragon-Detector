#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ble_profile.h"
#include "p4_survey_csv.h"
#include <cassert>
#include <map>
#include <string>

namespace {
constexpr int pdTRUE=1,FILE_WRITE=1;
int sdMutex=1,held=0;
bool sdReady=true,exported=false,contention=false,present=true;
bool failWrite=false,failRename=false;
uint64_t capacity=128u<<20;
std::map<std::string,std::string> files;
p4_radar_wifi_snapshot_t snapshot{};
struct { bool privacy=false; } detector;
uint32_t millis(){return 4;}
uint32_t esp_random(){return 0x123;}
int pdMS_TO_TICKS(int ms){return ms;}
int xSemaphoreTake(int,int){if(contention)return 0;assert(!held);return ++held;}
void xSemaphoreGive(int){assert(held==1);--held;}
bool usbDriveOn(){return exported;}
const p4_radar_wifi_snapshot_t* p4LatestWifiSurvey(){return present?&snapshot:nullptr;}
struct File {
  std::string name;
  bool dir=false;
  size_t cursor=0;
  explicit operator bool()const{return !name.empty();}
  bool isDirectory()const{return dir;}
  File openNextFile(){return cursor++<files.size()?File{"entry"}:File{};}
  void close(){name.clear();}
  void flush(){}
  size_t size()const{return files.at(name).size();}
  bool getWriteError()const{return failWrite;}
  size_t write(const uint8_t* data,size_t n){
    assert(held);size_t accepted=failWrite?n-1:n;
    files.at(name).append(reinterpret_cast<const char*>(data),accepted);return accepted;
  }
};
struct {
  bool exists(const char* path){return !strcmp(path,"/dragon/surveys") || files.count(path);}
  File open(const char* path,int mode=0){
    if(!strcmp(path,"/dragon/surveys"))return File{path,true};
    if(mode==FILE_WRITE)files[path].clear();
    return files.count(path)?File{path}:File{};
  }
  bool mkdir(const char*){return true;}
  uint64_t totalBytes(){return capacity;}
  uint64_t usedBytes(){return 1024;}
  void remove(const char* p){files.erase(p);}
  bool rename(const char* from,const char* to){
    if(failRename || files.count(to))return false;
    files[to]=files.at(from);files.erase(from);return true;
  }
} CardFS;
}

#include "p4_survey_save.inc"

void testSurveyExport(){
  char note[128];std::string folder="/dragon/surveys/";
  auto save=[&](const char* name){bool ok=p4SaveWifiSurvey(name,note,sizeof(note));assert(!held);return ok;};
  present=false;assert(!save(nullptr) && strstr(note,"first Wi-Fi"));present=true;
  assert(!save(nullptr) && strstr(note,"no access points"));
  snapshot.finished_ms=UINT32_MAX-3;snapshot.count=3;snapshot.total=40;
  auto& ap=snapshot.aps[0];ap.bssid[0]=2;ap.bssid[5]=0xfe;ap.channel=6;ap.rssi=-71;ap.security=P4_AP_WPA2;
  const uint8_t ssid[]={ '=',',','"',0,0xff,'\n' };
  ap.ssid_length=sizeof(ssid);memcpy(ap.ssid,ssid,sizeof(ssid));
  snapshot.aps[1]=ap;snapshot.aps[1].rssi=0;
  snapshot.aps[2]=ap;snapshot.aps[2].ssid_length=0;snapshot.aps[2].bssid[5]=1;snapshot.aps[2].security=255;
  sdReady=false;assert(!save(nullptr)&&strstr(note,"microSD"));sdReady=true;
  exported=true;assert(!save(nullptr)&&strstr(note,"USB Drive"));exported=false;
  contention=true;assert(!save(nullptr)&&strstr(note,"busy"));contention=false;
  capacity=1000;assert(!save(nullptr)&&strstr(note,"space"));capacity=128u<<20;
  assert(!save("../test") && files.empty());
  assert(save("named") && strstr(note,"2 of 40"));
  const std::string expected=std::string(dragon::p4::surveyColumns)
    +"02:00:00:00:00:FE,6,-71,WPA2,8,3d2c2200ff0a\n02:00:00:00:00:01,6,-71,Unreported,8,\n";
  assert(files.at(folder+"named.csv")==expected);
  assert(!save("named") && files.at(folder+"named.csv")==expected);
  files[folder+"survey-00000123.csv.part"]="other";
  assert(save(nullptr) && files.at(folder+"survey-00000124.csv")==expected);
  assert(files.at(folder+"survey-00000123.csv.part")=="other");
  detector.privacy=true;assert(save("redacted"));
  assert(files.at(folder+"redacted.csv")==std::string(dragon::p4::surveyColumns)+",6,-71,WPA2,8,\n,6,-71,Unreported,8,\n");
  detector.privacy=false;size_t count=files.size();
  failWrite=true;assert(!save("broken") && files.size()==count);failWrite=false;
  failRename=true;assert(!save("broken") && files.size()==count);failRename=false;
  for(int i=0;i<512;++i)files[std::to_string(i)]="";
  assert(!save("full") && strstr(note,"folder is full"));files.clear();
}

#ifdef P4_SURVEY_STANDALONE
int main(){testSurveyExport();}
#endif
