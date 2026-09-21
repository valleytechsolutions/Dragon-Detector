#include "app.h"
#include "board_pins.h"
#include <esp_timer.h>

Contact contacts[MAX_CONTACTS]; Alert alerts[MAX_ALERTS];
uint32_t alertTotal=0,wifiFrames=0,bleFrames=0,eapolFrames=0,queueDrops=0,malformedFrames=0;
uint32_t storageDrops=0;
uint32_t channelFrames[14]{},channelDwellMs[14]{},wifiListenMs=0,bleListenMs=0;
bool sdReady=false,loggingEnabled=false,captureEnabled=false;
SemaphoreHandle_t sdMutex; String sessionPath;
static File eventFile,pcapFile; static uint32_t lastFlush=0,segment=0;
void macText(const uint8_t* m,char* out){snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);}
static String quoted(const char* s){String o="\"";const char* start=s;while(*start==' '||*start=='\t')++start;if(*start && strchr("=+-@",*start))o+='\'';for(;*s;++s){if(*s=='"')o+='"';if(*s!='\r'&&*s!='\n')o+=*s;}return o+'"';}
static bool openLog(){
  String p=sessionPath+"/events-"+String(segment++)+".csv";
  eventFile=SD_MMC.open(p,FILE_WRITE);
  if(eventFile) eventFile.println("uptime_ms,event,radio,mac,name,rssi,channel,family,confidence,evidence");
  return bool(eventFile);
}
void storageBegin(){
  sdMutex=xSemaphoreCreateMutex();
  sdReady=LittleFS.begin(false,"/littlefs",8,"spiffs");
  if(!sdReady){Serial.println("SD: not mounted; filesystem image missing; flash factory image");return;}
  SD_MMC.mkdir("/badges");SD_MMC.mkdir("/dragon");
  char s[40];snprintf(s,sizeof(s),"/dragon/session-%08lx",(unsigned long)esp_random());sessionPath=s;
  SD_MMC.mkdir(sessionPath);loggingEnabled=false; // Flash logging is opt-in to limit wear.
  File info=SD_MMC.open(sessionPath+"/session.txt",FILE_WRITE);
  info.println("Dragon Detector 0.1.0\nTimes are milliseconds since boot, not UTC. Wi-Fi channels 1-11 and BLE alternate. Radar angles are decorative. RSSI is not distance. Signatures can be spoofed. Recording state of detected devices is unknown.");info.close();
  Serial.printf("SD: %llu MB; session created\n",(unsigned long long)LittleFS.totalBytes()/1048576);
}
static void append(const String& line){
  if(!sdReady || !loggingEnabled)return;
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(20))!=pdTRUE){++storageDrops;return;}
  if(eventFile && eventFile.size()>256*1024){eventFile.close();openLog();}
  if(!eventFile)openLog();
  if(!storageHasSpace(65536) || !eventFile || eventFile.println(line)==0){++storageDrops;loggingEnabled=false;Serial.println("SD log write failed");}
  xSemaphoreGive(sdMutex);
}
void logContact(const Contact& c,const char* event){
  char mac[18];macText(c.mac,mac);
  String line=String(millis())+","+event+","+(c.ble?"BLE":"WiFi")+","+mac+","+quoted(c.name)+","+String(c.rssi)+","+String(c.channel)+","+quoted(dragon::familyName(c.match.family))+","+dragon::confidenceName(c.match.confidence)+","+quoted(c.match.evidence);
  append(line);
}
void addAlert(const char* title,const char* detail,const uint8_t* mac){
  Alert& a=alerts[alertTotal%MAX_ALERTS];a={};a.at=millis();strlcpy(a.title,title,sizeof(a.title));strlcpy(a.detail,detail,sizeof(a.detail));if(mac)macText(mac,a.mac);++alertTotal;
  append(String(a.at)+",alert,,"+a.mac+","+quoted(title)+",,,,,"+quoted(detail));
  Serial.printf("ALERT %s: %s\n",title,detail);
}
bool setCapture(bool enabled){
  if(!sdReady)return false;
  xSemaphoreTake(sdMutex,portMAX_DELAY);
  captureEnabled=false;if(pcapFile){pcapFile.flush();pcapFile.close();}
  if(enabled){
    String p=sessionPath+"/capture-"+String(millis())+".pcap";pcapFile=SD_MMC.open(p,FILE_WRITE);
    // DLT_IEEE802_11, timestamps are monotonic uptime.
    const uint32_t h[]={0xa1b2c3d4,0x00040002,0,0,1024,105};
    if(pcapFile)captureEnabled=pcapFile.write((const uint8_t*)h,sizeof(h))==sizeof(h);
  }
  xSemaphoreGive(sdMutex);return captureEnabled;
}
void writePcap(const uint8_t* p,size_t n,size_t original,uint64_t us){
  if(!captureEnabled || !sdReady)return;
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(5))!=pdTRUE){++storageDrops;return;}
  uint32_t h[]={uint32_t(us/1000000),uint32_t(us%1000000),uint32_t(n),uint32_t(original)};
  bool ok=pcapFile.write((uint8_t*)h,sizeof(h))==sizeof(h) && pcapFile.write(p,n)==n;
  bool full=pcapFile.size()>=512*1024 || !storageHasSpace(65536);
  if(!ok || full){captureEnabled=false;pcapFile.flush();pcapFile.close();}
  xSemaphoreGive(sdMutex);
  if(!ok)addAlert("SD capture failed","Capture stopped after a write failure");
  else if(full)addAlert("Capture complete","512 KB capture limit reached; start a new capture to continue");
}
void storageLoop(){if(sdReady && millis()-lastFlush>2000 && xSemaphoreTake(sdMutex,0)==pdTRUE){lastFlush=millis();if(eventFile)eventFile.flush();if(pcapFile)pcapFile.flush();xSemaphoreGive(sdMutex);}}

bool storageHasSpace(size_t bytes){return sdReady && LittleFS.totalBytes()-LittleFS.usedBytes()>bytes;}
