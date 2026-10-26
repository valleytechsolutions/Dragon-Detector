#include "wardrive_upload.h"
#ifdef BOARD_AMOLED_175
#include "wardrive_upload_format.h"
#include "wardrive.h"
#include "app.h"
#include "recon.h"
#include "ble_lab.h"
#include "network_tools.h"
#include "storage_replace.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#include <cJSON.h>
#include <atomic>

namespace {
using namespace dragon::upload;
constexpr const char* ConfigPath="/wardriver.cfg";
constexpr const char* Directory="/dragon/wardrive";
constexpr const char* Receipts="/dragon/wardrive-uploads";
constexpr const char* Boundary="DragonDetectorWardriveBoundary";
Config config;
WardriveUploadSnapshot snapshot;
portMUX_TYPE snapshotLock=portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> cancelled{false},workerDone{false};
bool owned=false,workerStarted=false,resumeRadio=false,checkJobs=false,retryUnknown=false;
Service service=Service::Wigle;
void publish(const WardriveUploadSnapshot& s){portENTER_CRITICAL(&snapshotLock);snapshot=s;portEXIT_CRITICAL(&snapshotLock);}
void note(const char* message){auto s=wardriveUploadSnapshot();strlcpy(s.message,message,sizeof(s.message));publish(s);}
struct CardLock {CardLock(){xSemaphoreTake(sdMutex,portMAX_DELAY);}~CardLock(){xSemaphoreGive(sdMutex);}};
void* tlsAllocate(size_t count,size_t size){
  void* p=heap_caps_calloc(count,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  return p?p:heap_caps_calloc(count,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
}
bool readConfig(){
  memset(&config,0,sizeof(config));config.profile=-1;
  auto previous=wardriveUploadSnapshot();previous.wigleConfigured=previous.wdgConfigured=false;publish(previous);
  if(!sdReady||!sdMutex){note("Insert the configured FAT32 SD card");return false;}
  CardLock lock;File f=CardFS.open(ConfigPath,FILE_READ);
  if(!f){note("Fill /wardriver.cfg on the SD card");return false;}
  if(f.size()>16384){f.close();note("Configuration too large (16 KiB maximum)");return false;}
  char line[512];size_t used=0;bool ok=true;
  while(f.available()&&ok){int c=f.read();if(c<0){ok=false;break;}
    if(c=='\n'){line[used]=0;ok=configLine(config,line);used=0;}
    else if(!c||used==sizeof(line)-1)ok=false;else line[used++]=char(c);
  }
  if(ok&&used){line[used]=0;ok=configLine(config,line);}f.close();memset(line,0,sizeof(line));
  auto s=wardriveUploadSnapshot();s.wigleConfigured=ok&&*config.wigle;s.wdgConfigured=ok&&*config.wdg;publish(s);
  if(!ok){memset(&config,0,sizeof(config));note("Invalid /wardriver.cfg; check field lengths and tokens");}
  return ok;
}
void receiptPath(char* path,size_t cap,const char* name){snprintf(path,cap,"%s/%s.%s",Receipts,name,service==Service::Wigle?"wigle":"wdg");}
ReceiptRead readReceipt(const char* name,Receipt& value){
  char path[128],text[96]{};receiptPath(path,sizeof(path),name);CardLock lock;
  if(!dragon::recoverFile(CardFS,String(path)))return ReceiptRead::Invalid;
  if(!CardFS.exists(path))return CardFS.exists(String(path)+".part")?ReceiptRead::Invalid:ReceiptRead::Missing;
  File f=CardFS.open(path,FILE_READ);if(!f)return ReceiptRead::Invalid;size_t n=f.size();
  bool ok=n&&n<sizeof(text)&&f.read((uint8_t*)text,n)==n;f.close();return ok&&strlen(text)==n&&receipt(text,value)?ReceiptRead::Valid:ReceiptRead::Invalid;
}
bool saveReceipt(const char* name,const Receipt& value){
  char path[128],temp[136],text[96];receiptPath(path,sizeof(path),name);snprintf(temp,sizeof(temp),"%s.part",path);
  int n=snprintf(text,sizeof(text),"DDUP1 %lu %08lx %u %lu\n",value.bytes,value.crc,unsigned(value.state),value.job);
  CardLock lock;if(!CardFS.exists(Receipts)&&!CardFS.mkdir(Receipts))return false;
  File f=CardFS.open(temp,FILE_WRITE);if(!f)return false;bool ok=f.write((const uint8_t*)text,n)==size_t(n);f.flush();f.close();
  if(ok)ok=dragon::replaceFile(CardFS,String(temp),String(path));return ok;
}
struct FileInfo {uint32_t bytes=0,crc=0;bool data=false;};
bool inspectFile(File& file,FileInfo& info){
  {CardLock lock;info.bytes=file.size();file.seek(0);}
  if(info.bytes>32u*1024*1024)return false;
  uint8_t data[1024];uint32_t offset=0,crc=0xffffffffu;unsigned newlines=0;char first[16]{};
  while(offset<info.bytes&&!cancelled.load()){
    size_t wanted=std::min<size_t>(sizeof(data),info.bytes-offset),n;
    {CardLock lock;n=file.read(data,wanted);}if(n!=wanted)return false;
    if(!offset)memcpy(first,data,std::min<size_t>(n,15));
    for(size_t i=0;i<n;++i){if(newlines>=2&&data[i]!='\r'&&data[i]!='\n'&&data[i]!=' '&&data[i]!='\t')info.data=true;if(data[i]=='\n')++newlines;}
    crc=dragon::crc32(data,n,crc);offset+=n;vTaskDelay(1);
  }
  info.crc=crc^0xffffffffu;return offset==info.bytes&&newlines>=2&&!strncmp(first,"WigleWifi-1.6,",14);
}
struct Reply {int code=0;bool complete=false,success=false,done=false,failed=false;uint32_t job=0;};
// Reject excessive nesting before entering cJSON's recursive parser.
bool boundedJson(const char* body){int depth=0;bool quoted=false,escape=false;for(;*body;++body){
  if(quoted){if(escape)escape=false;else if(*body=='\\')escape=true;else if(*body=='"')quoted=false;continue;}
  if(*body=='"')quoted=true;else if(*body=='{'||*body=='['){if(++depth>12)return false;}else if(*body=='}'||*body==']'){if(--depth<0)return false;}
 }return depth==0&&!quoted;}
Reply response(esp_http_client_handle_t http){
  Reply out;if(esp_http_client_fetch_headers(http)<0)return out;out.code=esp_http_client_get_status_code(http);
  char* body=(char*)ps_calloc(4097,1);if(!body)return out;size_t used=0;
  while(used<4096&&!cancelled.load()){
    int n=esp_http_client_read(http,body+used,4096-used);if(n<0)break;if(!n){out.complete=esp_http_client_is_complete_data_received(http);break;}used+=n;
    if(esp_http_client_is_complete_data_received(http)){out.complete=true;break;}vTaskDelay(1);
  }
  if(out.complete&&strlen(body)==used&&boundedJson(body)){
    cJSON* object=cJSON_ParseWithLengthOpts(body,used+1,nullptr,true);if(cJSON_IsObject(object)){
      out.success=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object,service==Service::Wigle?"success":"ok"));
      auto* job=cJSON_GetObjectItemCaseSensitive(object,"job_id");if(cJSON_IsNumber(job)&&job->valuedouble>=1&&job->valuedouble<=UINT32_MAX&&job->valuedouble==uint32_t(job->valuedouble))out.job=uint32_t(job->valuedouble);
      auto* state=cJSON_GetObjectItemCaseSensitive(object,"status");
      if(!cJSON_IsString(state)){auto* nested=cJSON_GetObjectItemCaseSensitive(object,"job");state=cJSON_GetObjectItemCaseSensitive(nested,"status");}
      if(cJSON_IsString(state)){out.done=!strcmp(state->valuestring,"done");out.failed=!strcmp(state->valuestring,"failed");}
    }cJSON_Delete(object);
  }
  free(body);return out;
}
esp_http_client_handle_t connection(const char* path,bool post){
  char url[192];snprintf(url,sizeof(url),"https://%s%s",service==Service::Wigle?"api.wigle.net":"wdgwars.pl",path);
  esp_http_client_config_t cfg{};cfg.url=url;cfg.timeout_ms=8000;cfg.buffer_size=1024;cfg.buffer_size_tx=1024;
  cfg.crt_bundle_attach=esp_crt_bundle_attach;cfg.disable_auto_redirect=true;cfg.method=post?HTTP_METHOD_POST:HTTP_METHOD_GET;
  auto http=esp_http_client_init(&cfg);if(!http)return nullptr;
  char auth[400];if(service==Service::Wigle){snprintf(auth,sizeof(auth),"Basic %s",config.wigle);esp_http_client_set_header(http,"Authorization",auth);memset(auth,0,sizeof(auth));}
  else esp_http_client_set_header(http,"X-API-Key",config.wdg);
  esp_http_client_set_header(http,"User-Agent","DragonDetector/" DRAGON_VERSION);
  esp_http_client_set_header(http,"Connection","close");return http;
}
bool send(esp_http_client_handle_t http,const void* bytes,size_t size){
  size_t offset=0;while(offset<size&&!cancelled.load()){
    int n=esp_http_client_write(http,(const char*)bytes+offset,size-offset);if(n<=0)return false;offset+=n;vTaskDelay(1);
  }return offset==size;
}
Reply submit(File& file,const char* name,const FileInfo& info,WardriveUploadSnapshot& progress){
  Reply reply;auto http=connection(service==Service::Wigle?"/api/v2/file/upload":"/api/v2/upload-csv",true);if(!http)return reply;
  char pre[256],post[80],type[112];size_t first=multipart(pre,sizeof(pre),Boundary,name);
  int last=snprintf(post,sizeof(post),"\r\n--%s--\r\n",Boundary);
  snprintf(type,sizeof(type),"multipart/form-data; boundary=%s",Boundary);esp_http_client_set_header(http,"Content-Type",type);
  bool ok=first&&esp_http_client_open(http,first+info.bytes+last)==ESP_OK;
  if(ok)ok=send(http,pre,first);
  {CardLock lock;ok=ok&&file.seek(0);}
  uint8_t bytes[1024];uint32_t sent=0,lastUpdate=0;
  while(ok&&sent<info.bytes&&!cancelled.load()){
    size_t wanted=std::min<size_t>(sizeof(bytes),info.bytes-sent),n;
    {CardLock lock;n=file.read(bytes,wanted);}if(n!=wanted){ok=false;break;}
    ok=send(http,bytes,n);if(ok)sent+=n;
    if(millis()-lastUpdate>=200){progress.sent=sent;publish(progress);lastUpdate=millis();}vTaskDelay(1);
  }
  progress.sent=sent;publish(progress);
  if(ok&&sent==info.bytes&&!cancelled.load()&&send(http,post,last))reply=response(http);
  esp_http_client_close(http);esp_http_client_cleanup(http);return reply;
}
Reply checkJob(uint32_t job){
  char path[80];snprintf(path,sizeof(path),"/api/v2/upload-job/%lu",job);auto http=connection(path,false);Reply reply;
  if(http){if(esp_http_client_open(http,0)==ESP_OK)reply=response(http);esp_http_client_close(http);esp_http_client_cleanup(http);}return reply;
}
void worker(void*){
  auto s=wardriveUploadSnapshot();File dir;bool fatal=false,uncertain=false;
  {CardLock lock;dir=CardFS.open(Directory);}
  if(!dir){strlcpy(s.message,"No wardrive directory on SD",sizeof(s.message));publish(s);workerDone=true;vTaskDelete(nullptr);return;}
  // Traverse the directory without a RAM-sized list of every saved session.
  while(!cancelled.load()){
    File file;char name[48]{};
    {CardLock lock;file=dir.openNextFile();if(file){const char* path=file.name();const char* slash=strrchr(path,'/');strlcpy(name,slash?slash+1:path,sizeof(name));}}
    if(!file)break;
    if(!leaf(name)||file.isDirectory()){CardLock lock;file.close();continue;}
    ++s.total;strlcpy(s.file,name,sizeof(s.file));s.sent=0;{CardLock lock;s.size=file.size();}strlcpy(s.message,"Checking saved CSV",sizeof(s.message));publish(s);
    FileInfo info;if(!inspectFile(file,info)){++s.failed;{CardLock lock;file.close();}if(cancelled.load())break;continue;}
    if(!info.data){++s.skipped;{CardLock lock;file.close();}publish(s);continue;}
    Receipt prior;auto read=readReceipt(name,prior);auto action=uploadAction(read,prior,info.bytes,info.crc,checkJobs,retryUnknown);
    if(action==UploadAction::Skip){++s.skipped;{CardLock lock;file.close();}publish(s);continue;}
    if(action==UploadAction::Blocked){uncertain=true;++s.failed;{CardLock lock;file.close();}strlcpy(s.message,"Uncertain previous upload; check service before Retry",sizeof(s.message));publish(s);continue;}
    bool pending=action==UploadAction::Check;
    Receipt next{info.bytes,info.crc,0,ReceiptState::Uncertain};
    if(!pending&&!saveReceipt(name,next)){fatal=true;++s.failed;{CardLock lock;file.close();}strlcpy(s.message,"Cannot save upload receipt; stopped",sizeof(s.message));publish(s);break;}
    strlcpy(s.message,pending?"Checking queued import":"Uploading saved CSV",sizeof(s.message));publish(s);
    Reply result=pending?checkJob(prior.job):submit(file,name,info,s);
    {CardLock lock;file.close();}s.http=result.code;
    if(pending){
      next=prior;
      if(result.code==200&&result.complete&&result.done){next.state=ReceiptState::Done;++s.accepted;}
      else if(result.code==200&&result.complete&&result.failed){next.state=ReceiptState::Failed;++s.failed;}
      else ++s.pending;
    }else if(service==Service::Wigle&&result.code==200&&result.complete&&result.success){next.state=ReceiptState::Accepted;++s.accepted;}
    else if(service==Service::WdgWars&&result.code==202&&result.complete&&result.success&&result.job){next.state=ReceiptState::Pending;next.job=result.job;++s.pending;}
    else{++s.failed;if(result.code>=400&&result.code<500)next.state=ReceiptState::Failed;else uncertain=true;}
    if(!saveReceipt(name,next)){fatal=true;strlcpy(s.message,"Service replied; SD receipt save failed",sizeof(s.message));++s.failed;publish(s);break;}
    ++s.done;publish(s);
    // Authentication/rate errors stop the batch. Retrying is an explicit action.
    if(result.code==401||result.code==403||result.code==429)break;
    for(unsigned i=0;i<10&&!cancelled.load();++i)vTaskDelay(pdMS_TO_TICKS(100));
  }
  {CardLock lock;dir.close();}
  if(fatal){} // Keep the actionable SD error instead of hiding it in totals.
  else if(cancelled.load())strlcpy(s.message,"Cancelled; saved CSVs retained",sizeof(s.message));
  else if(s.http==401||s.http==403)strlcpy(s.message,"Authentication rejected; check SD token/key",sizeof(s.message));
  else if(s.http==429)strlcpy(s.message,"Service rate limit; retry later",sizeof(s.message));
  else if(uncertain)strlcpy(s.message,"Upload unconfirmed; check service before RETRY UNCLEAR",sizeof(s.message));
  else snprintf(s.message,sizeof(s.message),"Accepted %lu / queued %lu / skipped %lu / failed %lu",s.accepted,s.pending,s.skipped,s.failed);
  publish(s);workerDone=true;vTaskDelete(nullptr);
}
}
WardriveUploadSnapshot wardriveUploadSnapshot(){portENTER_CRITICAL(&snapshotLock);auto s=snapshot;portEXIT_CRITICAL(&snapshotLock);return s;}
bool wardriveUploadBusy(){return owned;}
void wardriveUploadBegin(){
  // Stable allocator hooks before the first TLS session. TLS record/certificate
  // buffers use PSRAM instead of starving the LCD DMA and Wi-Fi internal heap.
  mbedtls_platform_set_calloc_free(tlsAllocate,heap_caps_free);
  note("Uploads all saved wardrives. Set SD /wardriver.cfg, then choose a service.");
  if(!sdReady||!sdMutex)return;
  {CardLock lock;if(!CardFS.exists(ConfigPath)&&!CardFS.exists("/wardriver.cfg.part")){
    const char* sample="# Dragon Detector uploads: edit locally; credentials stay on this SD card.\n# Compatible Piglet field names; uploads start only from the device menu.\nhomeSsid=\nhomePsk=\n# Or use a saved network slot 0-7 instead of homeSsid/homePsk:\nuploadNetworkProfile=\n# WiGLE account: copy the Encoded for Use value (without Basic).\nwigleBasicToken=\n# WDGoWars profile API key:\nwdgwarsApiKey=\n";
    File f=CardFS.open("/wardriver.cfg.part",FILE_WRITE);bool ok=f&&f.print(sample)==strlen(sample);if(f){f.flush();f.close();}if(ok)CardFS.rename("/wardriver.cfg.part",ConfigPath);
  }}
  readConfig();memset(&config,0,sizeof(config));
}
bool wardriveUploadStart(bool wdg,bool onlyJobs,bool retry){
  if(owned)return false;
  if(wardriveBusy()||reconOwnsRadio()||reconStorageBusy()||bleLabOwnsRadio()||captureEnabled||stopwatch.running||foxTransmit()){note("Stop recording, timers and other radio sessions first");return false;}
  if(!readConfig())return false;
  if(!networkConfigured(config)){note("Add home WiFi or a saved profile to /wardriver.cfg");memset(&config,0,sizeof(config));return false;}
  if(!(wdg?*config.wdg:*config.wigle)){note(wdg?"Set wdgwarsApiKey in /wardriver.cfg":"Set wigleBasicToken in /wardriver.cfg");memset(&config,0,sizeof(config));return false;}
  if(!wallTime()){note("Set the clock before using verified HTTPS");memset(&config,0,sizeof(config));return false;}
  resumeRadio=radioEnabled;
  bool joined=config.profile>=0?networkConnect(config.profile):networkConnectCredentials(config.ssid,config.password);
  memset(config.password,0,sizeof(config.password));
  if(!joined){memset(&config,0,sizeof(config));note("Cannot join WiFi; check settings, free memory and radio state");return false;}
  audioStop();owned=true;workerStarted=false;workerDone=false;cancelled=false;service=wdg?Service::WdgWars:Service::Wigle;checkJobs=onlyJobs;retryUnknown=retry;
  auto old=wardriveUploadSnapshot();WardriveUploadSnapshot next;next.busy=true;next.wigleConfigured=old.wigleConfigured;next.wdgConfigured=old.wdgConfigured;
  strlcpy(next.service,wdg?"WDGoWars":"WiGLE",sizeof(next.service));strlcpy(next.message,"Joining WiFi (25-second deadline)",sizeof(next.message));publish(next);uiShow("wardrive-upload");return true;
}
void wardriveUploadCancel(){if(owned){cancelled=true;auto s=wardriveUploadSnapshot();s.cancelling=true;strlcpy(s.message,"Cancelling; waiting for the current network operation",sizeof(s.message));publish(s);}}
void wardriveUploadLoop(){
  if(!owned)return;networkLoop();
  if(!workerStarted&&!cancelled.load()&&networkConnected()){
    workerStarted=xTaskCreatePinnedToCore(worker,"wardrive-upload",12288,nullptr,1,nullptr,0)==pdPASS;
    if(!workerStarted){note("Upload task allocation failed; saved files retained");cancelled=true;}
  }
  if(workerStarted&&!networkActive())cancelled=true;
  if(workerDone.load()||(!workerStarted&&(cancelled.load()||!networkActive()))){
    if(!workerStarted&&!cancelled.load())note("WiFi join failed; check /wardriver.cfg");
    owned=false;networkStop();setRadio(resumeRadio);memset(&config,0,sizeof(config));
    auto s=wardriveUploadSnapshot();s.busy=s.cancelling=false;publish(s);
  }
}
bool wardriveUploadCommand(const String& c){
  if(c=="wardrive upload status"){
    auto s=wardriveUploadSnapshot();Serial.printf("WARDRIVE_UPLOAD busy=%u wigle_token=%u wdg_key=%u service=%s total=%lu done=%lu accepted=%lu pending=%lu skipped=%lu failed=%lu sent=%lu bytes=%lu http=%d message=%s\n",s.busy,s.wigleConfigured,s.wdgConfigured,s.service,s.total,s.done,s.accepted,s.pending,s.skipped,s.failed,s.sent,s.size,s.http,s.message);return true;
  }
  if(c=="wardrive upload cancel"){wardriveUploadCancel();Serial.println("WARDRIVE_UPLOAD cancelling");return true;}
  if(c=="wardrive upload wigle"||c=="wardrive upload wdgwars"||c=="wardrive upload check"){
    bool ok=wardriveUploadStart(c!="wardrive upload wigle",c=="wardrive upload check");Serial.println(ok?"WARDRIVE_UPLOAD started":"WARDRIVE_UPLOAD unavailable; see status");return true;
  }return false;
}
#endif
