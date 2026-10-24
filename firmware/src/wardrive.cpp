#include "wardrive.h"
#if BOARD_HAS_GPS
#include "app.h"
#include "recon_internal.h"
#include "ble_lab.h"
#include "missions.h"
#include "recipes.h"
#include "sd_records.h"
#include <Wire.h>
#ifdef BOARD_WATCHER
#include <HardwareSerial.h>
#endif
namespace {
dragon::wardrive::Nmea gps;
SdRecords driveLog;
bool running=false,owned=false,present=false;
uint32_t wifi=0,ble=0,noFix=0,duplicate=0,gpsErrors=0,late=0;
char notice[120]="Insert FAT32 SD; GPS variant required for wardriving";
struct Seen {uint8_t mac[6];uint8_t kind;bool used;uint32_t at;};
Seen* seen=nullptr;
constexpr unsigned SeenCount=512;
// Per-address/radio cooldown, bounded memory. Expiry allows revisits while
// moving; a full cache replaces its oldest entry rather than losing new APs.
bool allow(const uint8_t* mac,uint8_t kind,uint32_t now){
  unsigned slot=0;uint32_t oldest=0;
  for(unsigned i=0;i<SeenCount;++i){
    if(!seen[i].used){slot=i;break;}
    if(seen[i].kind==kind&&!memcmp(seen[i].mac,mac,6)){
      if(now-seen[i].at<5000){++duplicate;return false;}
      slot=i;break;
    }
    if(now-seen[i].at>=oldest){oldest=now-seen[i].at;slot=i;}
  }
  seen[slot].used=true;seen[slot].kind=kind;seen[slot].at=now;memcpy(seen[slot].mac,mac,6);return true;
}
#ifdef BOARD_GPS_LC76G
enum class Phase {LengthCommand,LengthRead,DataCommand,DataRead};
Phase phase=Phase::LengthCommand;
uint32_t due=0;uint16_t remaining=0,discard=0;
bool command(uint8_t first,uint8_t second,uint32_t length){
  uint8_t data[]={first,second,0x51,0xaa,uint8_t(length),uint8_t(length>>8),uint8_t(length>>16),uint8_t(length>>24)};
  Wire.beginTransmission(0x50);Wire.write(data,sizeof(data));return Wire.endTransmission()==0;
}
void gpsLoop(){
  uint32_t now=millis();if(int32_t(now-due)<0)return;
  // The shared bus already exists. Preserve its clock outside each transaction.
  const uint32_t previousClock=Wire.getClock();Wire.setClock(100000);
  bool ok=true;
  if(phase==Phase::LengthCommand){
    ok=command(0x08,0,4);phase=Phase::LengthRead;due=now+100;
  }else if(phase==Phase::LengthRead){
    ok=Wire.requestFrom(uint8_t(0x54),size_t(4))==4;
    if(ok){
      uint32_t length=0;for(unsigned i=0;i<4;++i)length|=uint32_t(uint8_t(Wire.read()))<<(8*i);
      // Never allocate or request an unbounded length from a faulty device.
      if(length>16384)ok=false;
      else if(!length){phase=Phase::LengthCommand;due=now+200;present=true;}
      else{
        // Drain the advertised FIFO completely. On backlog, parse only its tail
        // so older buffered fixes cannot briefly be recorded as fresh positions.
        remaining=length;discard=length>2048?length-2048:0;
        if(discard){gps.fix.position=false;gps.discardPartial();}
        phase=Phase::DataCommand;due=now+100;
      }
    }
  }else if(phase==Phase::DataCommand){
    ok=command(0,0x20,remaining);phase=Phase::DataRead;due=now+100;
  }else{
    // LC76G I2C FIFO, bounded reads within Arduino Wire's 128-byte buffer.
    size_t count=std::min<size_t>(remaining,128);
    ok=Wire.requestFrom(uint8_t(0x54),count)==count;
    if(ok){
      for(size_t i=0;i<count;++i){char c=Wire.read();if(discard)--discard;else gps.feed(c,now);}
      remaining-=count;present=true;
      phase=remaining?Phase::DataRead:Phase::LengthCommand;due=now+2;
    }
  }
  Wire.setClock(previousClock);
  if(!ok){++gpsErrors;present=false;remaining=discard=0;gps.fix.position=false;phase=Phase::LengthCommand;due=now+2000;while(Wire.available())Wire.read();}
}
#endif
#ifdef BOARD_WATCHER
HardwareSerial gpsSerial(1);
uint32_t lastGpsByte=0;
void gpsLoop(){
  uint32_t now=millis();unsigned waiting=gpsSerial.available();
  // Bounded per-loop work, keeping only fresh tail data after a stalled UI.
  if(waiting>2048){gps.fix.position=false;gps.discardPartial();++gpsErrors;unsigned discard=waiting-1024;while(discard--&&gpsSerial.available())gpsSerial.read();}
  unsigned budget=512;while(budget--&&gpsSerial.available()){gps.feed(char(gpsSerial.read()),now);lastGpsByte=now;}
  present=lastGpsByte&&uint32_t(now-lastGpsByte)<3000;
  if(!present)gps.fix.position=false;
}
#endif
bool located(uint32_t now){
#ifdef BOARD_GPS_LC76G
  // Do not consume a partially drained FIFO while its newest fix is pending.
  if(remaining)return false;
#endif
  return gps.fix.usable(now);
}
void record(const uint8_t* mac,const uint8_t* name,size_t length,const char* auth,int rssi,uint8_t channel,bool isBle,uint8_t kind,uint32_t received){
  if(!running)return;
  uint32_t now=millis();
  if(now-received>1000){++late;return;}
  if(!located(now)){++noFix;return;}
  if(!allow(mac,kind,now))return;
  char row[384];size_t size=dragon::wardrive::csv(row,sizeof(row),mac,name,length,auth,rssi,channel,isBle,gps.fix,now);
  if(size&&driveLog.append(row,size)){if(isBle)++ble;else ++wifi;}
}
}
bool wardriveActive(){return running;}
bool wardriveBusy(){return owned;}
void wardriveBegin(){
#ifdef BOARD_WATCHER
  gpsSerial.setRxBufferSize(4096);gpsSerial.begin(115200,SERIAL_8N1,WATCHER_GPS_RX,WATCHER_GPS_TX);
  strlcpy(notice,"External AT6668: UART RX19/TX20; separate verified 5V power",sizeof(notice));
#endif
#ifdef BOARD_GPS_LC76G
  // Do not reset the shared expander or change LCD/touch/PMU wiring.
  strlcpy(notice,"LC76G GPS / SD wardriving ready",sizeof(notice));
#endif
}
bool wardriveStart(){
#if !defined(BOARD_GPS_LC76G) && !defined(BOARD_WATCHER)
  strlcpy(notice,"Flash the 1.75-G GPS profile on LC76G hardware",sizeof(notice));return false;
#else
  if(owned)return false;
  if(radioStandby()||!radioEnabled||bleLabOwnsRadio()||reconOwnsRadio()||reconStorageBusy()||
     dragon::toolkit::status.recording||missionActive()||recipeActive()||captureEnabled||foxTransmit()){
    strlcpy(notice,"Stop other sessions / capture / fox before wardriving",sizeof(notice));return false;
  }
  if(!seen)seen=(Seen*)ps_calloc(SeenCount,sizeof(Seen));
  if(!seen){strlcpy(notice,"Wardrive cache allocation failed",sizeof(notice));return false;}
  memset(seen,0,SeenCount*sizeof(Seen));
  char header[512];
#ifdef BOARD_WATCHER
  snprintf(header,sizeof(header),"WigleWifi-1.6,appRelease=" DRAGON_VERSION ",model=SenseCAP-Watcher,release=" DRAGON_VERSION ",device=DragonDetector,display=LCD,board=watcher,brand=Seeed\n%s",dragon::wardrive::Columns);
#else
  snprintf(header,sizeof(header),"WigleWifi-1.6,appRelease=" DRAGON_VERSION ",model=ESP32-S3-Touch-AMOLED-1.75-G,release=" DRAGON_VERSION ",device=DragonDetector,display=AMOLED,board=waveshare_175_gps,brand=Waveshare\n%s",dragon::wardrive::Columns);
#endif
  if(!driveLog.begin("/wardrive","wigle","csv",header,strlen(header))){strlcpy(notice,driveLog.message,sizeof(notice));return false;}
  wifi=ble=noFix=duplicate=late=0;running=owned=true;radioWardrive(true);
  strlcpy(notice,"Passive Wi-Fi + BLE; waiting for a valid GPS fix",sizeof(notice));return true;
#endif
}
void wardriveStop(){
  if(!running)return;running=false;driveLog.stop();
  strlcpy(notice,"Stopping; keep SD inserted until saved",sizeof(notice));
}
void wardriveLoop(){
#if defined(BOARD_GPS_LC76G) || defined(BOARD_WATCHER)
  gpsLoop();
#endif
  driveLog.loop();
  if(owned&&!driveLog.active()&&running){running=false;strlcpy(notice,driveLog.message,sizeof(notice));}
  if(owned&&!driveLog.busy()){
    owned=false;radioWardrive(false);strlcpy(notice,driveLog.message,sizeof(notice));
  }
}
void wardriveWifi(const uint8_t* data,size_t length,int rssi,uint8_t channel,uint32_t received){
  auto f=dragon::parseWifi(data,length);
  if(!f.valid||(f.frameType!=0x80&&f.frameType!=0x50))return;
  char auth[96];dragon::wardrive::authentication(auth,sizeof(auth),data,length,f.security);
  record(f.bssid,f.ssid,f.ssidLength,auth,rssi,f.channel?f.channel:channel,false,0,received);
}
void wardriveBle(const uint8_t* mac,uint8_t addressType,int rssi,const uint8_t* data,size_t length,uint32_t received){
  const uint8_t* name=nullptr;size_t nameSize=0;
  for(size_t at=0;at<length;){
    unsigned n=data[at++];if(!n)break;if(n>length-at)return;
    if(data[at]==8||data[at]==9){name=data+at+1;nameSize=std::min<size_t>(n-1,64);}
    at+=n;
  }
  record(mac,name,nameSize,"[BLE]",rssi,0,true,1+addressType,received);
}
void wardriveStatus(char* out,size_t cap){
  const bool fixed=located(millis());
  snprintf(out,cap,"%s\n%s\n\nGPS: %s / %u satellites\nWi-Fi %lu   BLE %lu\nNo fix %lu   Late %lu\nDuplicates %lu   SD drops %lu\nSaved %llu KB / queued %u B\nGPS errors %lu\n\nSD /dragon/wardrive\n%s",
    running?"WARDRIVING":owned?"SAVING":"WARDRIVE IDLE",
#ifdef BOARD_GPS_LC76G
    "LC76G / passive Wi-Fi + BLE",
#elif defined(BOARD_WATCHER)
    "AT6668 UART / passive Wi-Fi + BLE",
#else
    "GPS firmware profile required",
#endif
    fixed?"FIX":present?"waiting":"no data",gps.fix.satellites,
    wifi,ble,noFix,late,duplicate,driveLog.dropped,(unsigned long long)(driveLog.bytes/1024),unsigned(driveLog.pending()),gpsErrors,
    owned?(fixed?"Recording located observations":"No valid fix: located rows paused"):notice);
}
bool wardriveCommand(const String& c){
  if(c=="wardrive start")Serial.println(wardriveStart()?"WARDRIVE_OK started":"WARDRIVE_ERROR start failed; see status");
  else if(c=="wardrive stop"){wardriveStop();Serial.println("WARDRIVE_OK stopping");}
  else if(c=="wardrive status"||c=="gps status"){
    char text[520];wardriveStatus(text,sizeof(text));Serial.println(text);
    if(located(millis()))Serial.printf("GPS_FIX %.7f %.7f %.2f epoch=%lu hdop=%.2f\n",gps.fix.latitude,gps.fix.longitude,gps.fix.altitude,gps.fix.epoch,gps.fix.hdop);
    Serial.println("WARDRIVE_END");
  }else return false;
  return true;
}
#endif
