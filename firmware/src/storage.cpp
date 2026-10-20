#include "app.h"
#include "background_radio.h"
#include "board_pins.h"
#ifdef BOARD_AMOLED_175
#include "recon_internal.h"
#endif
#include <esp_timer.h>

ContactStore contacts; Alert alerts[MAX_ALERTS];
uint32_t alertTotal=0,wifiFrames=0,bleFrames=0,eapolFrames=0,queueDrops=0,malformedFrames=0;
uint32_t storageDrops=0;
uint32_t channelFrames[14]{},channelDwellMs[14]{},wifiListenMs=0,bleListenMs=0;
bool sdReady=false,loggingEnabled=false,captureEnabled=false;
SemaphoreHandle_t sdMutex; String sessionPath;

void macText(const uint8_t* m,char* out){snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);}
#if BOARD_HAS_SD
static File eventFile,pcapFile; static uint32_t lastFlush=0,segment=0;
static String quoted(const char* s){String o="\"";const char* start=s;while(*start==' '||*start=='\t')++start;if(*start && strchr("=+-@",*start))o+='\'';for(;*s;++s){if(*s=='"')o+='"';if(*s!='\r'&&*s!='\n')o+=*s;}return o+'"';}
static bool openLog(){
  String p=sessionPath+"/events-"+String(segment++)+".csv";
  eventFile=CardFS.open(p,FILE_WRITE);
  if(eventFile) eventFile.println("uptime_ms,event,radio,mac,name,rssi,channel,family,confidence,evidence");
  return bool(eventFile);
}
static void mount(){
#if defined(BOARD_SD_SPI)
  extern bool boardMountCard();bool mounted=boardMountCard();
#else
  CardFS.setPins(SD_MMC_CLK_PIN,SD_MMC_CMD_PIN,SD_MMC_D0_PIN);
  bool mounted=CardFS.begin("/sdcard",true,false,SDMMC_FREQ_DEFAULT,12);
#endif
  if(!mounted){Serial.println("SD: not mounted; insert FAT32 card and restart");return;}
  const char* dirs[]={"/dragon","/music","/recordings","/dragon/captures","/dragon/profiles","/dragon/events","/dragon/surveys","/dragon/missions","/dragon/wardrive"};
  for(const char* path:dirs){if(!CardFS.exists(path)&&!CardFS.mkdir(path)){Serial.printf("SD: cannot create %s; storage disabled\n",path);return;}}

  char s[40];snprintf(s,sizeof(s),"/dragon/session-%08lx",(unsigned long)esp_random());sessionPath=s;
  if(CardFS.exists(sessionPath)||!CardFS.mkdir(sessionPath)){Serial.println("SD: cannot create unique session");return;}loggingEnabled=false;
  File info=CardFS.open(sessionPath+"/session.txt",FILE_WRITE);
  if(!info){Serial.println("SD: session not writable");return;}
  const char* description="Dragon Detector " DRAGON_VERSION "\nTimes are milliseconds since boot, not UTC. Wi-Fi channels 1-11 and BLE alternate. Radar angles are decorative. RSSI is not distance. Signatures can be spoofed. Recording state of detected devices is unknown.\n";
  bool writable=info.write((const uint8_t*)description,strlen(description))==strlen(description);info.close();
  if(!writable){Serial.println("SD: session write failed");return;}
  Serial.printf("SD: %llu MB; session created\n",CardFS.cardSize()/1048576);
  sdReady=true;
}
void storageBegin(){
  sdMutex=xSemaphoreCreateMutex();
  mount();
}
static void append(const String& line){
  if(!sdReady || !loggingEnabled)return;
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(20))!=pdTRUE){++storageDrops;return;}
  if(!eventFile)openLog();
  if(eventFile && eventFile.size()>4*1024*1024){eventFile.close();openLog();}
  if(!eventFile || eventFile.println(line)==0){++storageDrops;loggingEnabled=false;if(!radioTransferDepth)Serial.println("SD log write failed");}
  xSemaphoreGive(sdMutex);
}
void logContact(const Contact& c,const char* event){
  if(!sdReady || !loggingEnabled)return;
  char mac[18];macText(c.mac,mac);
  String line=String(millis())+","+event+","+(c.ble?"BLE":"WiFi")+","+mac+","+quoted(c.name)+","+String(c.rssi)+","+String(c.channel)+","+quoted(dragon::familyName(c.match.family))+","+dragon::confidenceName(c.match.confidence)+","+quoted(c.match.evidence);
  append(line);
}
#else
// Flash history, badges and recon storage have their own independent drivers.
void storageBegin(){}
void logContact(const Contact&,const char*){}
#endif
void addAlert(const char* title,const char* detail,const uint8_t* mac){
  Alert& a=alerts[alertTotal%MAX_ALERTS];a={};a.at=millis();strlcpy(a.title,title,sizeof(a.title));strlcpy(a.detail,detail,sizeof(a.detail));if(mac)macText(mac,a.mac);++alertTotal;
#if BOARD_HAS_SD
  if(sdReady && loggingEnabled)append(String(a.at)+",alert,,"+a.mac+","+quoted(title)+",,,,,"+quoted(detail));
#endif
  if(!radioTransferDepth)Serial.printf("ALERT %s: %s\n",title,detail);
}
#if BOARD_HAS_SD
bool setCapture(bool enabled){
#ifdef BOARD_AMOLED_175
  if(enabled)return dragon::toolkit::captureStart(false);
  dragon::toolkit::captureStop();return false;
#else
  if(enabled&&radioStandby())return false;
  if(!sdReady)return false;
  xSemaphoreTake(sdMutex,portMAX_DELAY);
  captureEnabled=false;if(pcapFile){pcapFile.flush();pcapFile.close();}
  if(enabled){
    String p=sessionPath+"/capture-"+String(millis())+".pcap";pcapFile=CardFS.open(p,FILE_WRITE);
    // DLT_IEEE802_11, timestamps are monotonic uptime.
    const uint32_t h[]={0xa1b2c3d4,0x00040002,0,0,1024,105};
    if(pcapFile)captureEnabled=pcapFile.write((const uint8_t*)h,sizeof(h))==sizeof(h);
  }
  xSemaphoreGive(sdMutex);return captureEnabled;
#endif
}
void writePcap(const uint8_t* p,size_t n,size_t original,uint64_t us){
  if(!captureEnabled || !sdReady)return;
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(5))!=pdTRUE){++storageDrops;return;}
  uint32_t h[]={uint32_t(us/1000000),uint32_t(us%1000000),uint32_t(n),uint32_t(original)};
  bool ok=pcapFile.write((uint8_t*)h,sizeof(h))==sizeof(h) && pcapFile.write(p,n)==n;
  bool full=pcapFile.size()>=16*1024*1024;
  if(!ok || full){captureEnabled=false;pcapFile.flush();pcapFile.close();}
  xSemaphoreGive(sdMutex);
  if(!ok)addAlert("SD capture failed","Capture stopped after a write failure");
  else if(full)addAlert("Capture complete","16 MB capture limit reached; start a new capture to continue");
}
void storageLoop(){if(sdReady && millis()-lastFlush>2000 && xSemaphoreTake(sdMutex,0)==pdTRUE){lastFlush=millis();if(eventFile)eventFile.flush();if(pcapFile)pcapFile.flush();xSemaphoreGive(sdMutex);}}

#else
bool setCapture(bool){return false;}
void writePcap(const uint8_t*,size_t,size_t,uint64_t){}
void storageLoop(){}
#endif
