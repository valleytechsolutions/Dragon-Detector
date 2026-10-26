#include "wardrive.h"
#if BOARD_HAS_GPS
#include "app.h"
#include "recon_internal.h"
#include "ble_lab.h"
#include "missions.h"
#include "recipes.h"
#include "sd_records.h"
#include "wardrive_upload.h"
#include <Wire.h>
#if defined(BOARD_WATCHER) || defined(BOARD_GPS_UART)
#include <HardwareSerial.h>
#endif
#ifdef BOARD_GPS_UART
#include <Preferences.h>
#include <atomic>
#endif
namespace {
dragon::wardrive::Nmea gps;
SdRecords driveLog;
bool running=false,owned=false,present=false;
bool radioFailed=false;
uint32_t wifi=0,ble=0,noFix=0,duplicate=0,gpsErrors=0,late=0;
uint32_t seenWifi=0,seenBle=0,started=0,ended=0;
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
      if(now-seen[i].at<
#ifdef BOARD_AMOLED_175
         1500
#else
         5000
#endif
      ){++duplicate;return false;}
      slot=i;break;
    }
    if(now-seen[i].at>=oldest){oldest=now-seen[i].at;slot=i;}
  }
  seen[slot].used=true;seen[slot].kind=kind;seen[slot].at=now;memcpy(seen[slot].mac,mac,6);return true;
}
#ifdef BOARD_GPS_LC76G
enum class Phase {LengthCommand,LengthRead,DataCommand,DataRead,RecoverRead,RecoverWrite,WakeSpaceCommand,WakeSpaceRead,WakeCommand,WakeWrite};
dragon::wardrive::Fix stableFix;
Phase phase=Phase::RecoverRead;
uint32_t due=0;uint16_t remaining=0,discard=0;
uint32_t gpsReadBytes=0,gpsReadCount=0,gpsLastLength=0,gpsMaxReadUs=0;
unsigned gpsFailureStage=0,gpsWireStatus=0,gpsLastReply=0;
bool gpsReserved=false;unsigned failures=0,emptyReads=0,recoveries=0;
bool command(uint8_t first,uint8_t second,uint32_t length,uint8_t op=0x51){
  uint8_t data[]={first,second,op,0xaa,uint8_t(length),uint8_t(length>>8),uint8_t(length>>16),uint8_t(length>>24)};
  Wire.beginTransmission(0x50);Wire.write(data,sizeof(data));gpsWireStatus=Wire.endTransmission();return gpsWireStatus==0;
}
void gpsLoop(){
  uint32_t now=millis();if(int32_t(now-due)<0)return;
  // The shared bus already exists. Preserve its clock outside each transaction.
  const uint32_t previousClock=Wire.getClock();Wire.setClock(100000);
  bool ok=true;const unsigned stage=unsigned(phase);const uint32_t started=micros();
  if(phase==Phase::RecoverRead){
    // Finish an interrupted reply before waking a full/asleep transmitter.
    gpsReserved=true;++recoveries;Wire.requestFrom(uint8_t(0x54),size_t(1));while(Wire.available())Wire.read();
    phase=Phase::RecoverWrite;due=now+10;
  }else if(phase==Phase::RecoverWrite){
    Wire.beginTransmission(0x58);Wire.write(uint8_t(0));Wire.endTransmission();phase=Phase::WakeSpaceCommand;due=now+10;
  }else if(phase==Phase::WakeSpaceCommand){
    gpsReserved=true;ok=command(4,0,4);phase=Phase::WakeSpaceRead;due=now+10;
  }else if(phase==Phase::WakeSpaceRead){
    // An asleep transmitter can NACK this query. The single dummy byte still
    // fits an empty RX FIFO and wakes it through the documented data port.
    Wire.requestFrom(uint8_t(0x54),size_t(4));while(Wire.available())Wire.read();phase=Phase::WakeCommand;due=now+10;
  }else if(phase==Phase::WakeCommand){
    gpsReserved=true;ok=command(0,0x10,1,0x53);phase=Phase::WakeWrite;due=now+10;
  }else if(phase==Phase::WakeWrite){
    Wire.beginTransmission(0x58);Wire.write(uint8_t(0));gpsWireStatus=Wire.endTransmission();ok=gpsWireStatus==0;
    phase=Phase::LengthCommand;due=now+10;emptyReads=0;
  }else if(phase==Phase::LengthCommand){
    gpsReserved=true;ok=command(0x08,0,4);phase=Phase::LengthRead;due=now+10;
  }else if(phase==Phase::LengthRead){
    gpsLastReply=Wire.requestFrom(uint8_t(0x54),size_t(4));ok=gpsLastReply==4;
    if(ok){
      uint32_t length=0;for(unsigned i=0;i<4;++i)length|=uint32_t(uint8_t(Wire.read()))<<(8*i);
      gpsLastLength=length;
      // Never allocate or request an unbounded length from a faulty device.
      if(length>16384)ok=false;
      else if(!length){phase=++emptyReads>=5?Phase::WakeSpaceCommand:Phase::LengthCommand;due=now+200;present=true;gpsReserved=false;}
      else{
        // Drain the advertised FIFO completely. On backlog, parse only its tail
        // so older buffered fixes cannot briefly be recorded as fresh positions.
        remaining=length;discard=length>2048?length-2048:0;
        if(discard){gps.fix.position=false;stableFix.position=false;gps.discardPartial();}
        emptyReads=0;phase=Phase::DataCommand;due=now+10;
      }
    }
  }else if(phase==Phase::DataCommand){
    // Each bounded read has its own command and consumes the entire response.
    // A STOP after 128 bytes must not truncate a larger outstanding request.
    gpsReserved=true;ok=command(0,0x20,std::min<uint16_t>(remaining,128));phase=Phase::DataRead;due=now+10;
  }else{
    // LC76G I2C FIFO, bounded reads within Arduino Wire's 128-byte buffer.
    size_t count=std::min<size_t>(remaining,128);
    gpsLastReply=Wire.requestFrom(uint8_t(0x54),count);ok=gpsLastReply==count;
    if(ok){
      for(size_t i=0;i<count;++i){char c=Wire.read();if(discard)--discard;else gps.feed(c,now);}
      remaining-=count;present=true;gpsReadBytes+=count;++gpsReadCount;failures=0;
      if(!remaining)stableFix=gps.fix;
      phase=remaining?Phase::DataCommand:Phase::LengthCommand;due=now+10;gpsReserved=false;
    }
  }
  Wire.setClock(previousClock);
  gpsMaxReadUs=std::max(gpsMaxReadUs,uint32_t(micros()-started));
  if(!ok){gpsFailureStage=stage;++gpsErrors;present=false;remaining=discard=0;gps.fix.position=false;stableFix.position=false;gps.discardPartial();gpsReserved=false;
    phase=++failures>=3?Phase::RecoverRead:Phase::LengthCommand;if(failures>=3)failures=0;
    due=now+1000;while(Wire.available())Wire.read();}
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
#include "wardrive_uart.inc"
const dragon::wardrive::Fix& recordingFix(){
#ifdef BOARD_GPS_LC76G
  // Publish only complete FIFO batches. The previous fresh pair stays usable
  // during ordinary reads; backlog recovery and errors invalidate it at once.
  return stableFix;
#else
  return gps.fix;
#endif
}
bool located(uint32_t now){return recordingFix().usable(now);}
void record(const uint8_t* mac,const uint8_t* name,size_t length,const char* auth,int rssi,uint8_t channel,bool isBle,uint8_t kind,uint32_t received,bool dedupe=true){
  if(!running)return;
  uint32_t now=millis();
  if(now-received>1000){++late;return;}
  if(dedupe&&!allow(mac,kind,now))return;
  if(isBle)++seenBle;else ++seenWifi;
  if(!located(now)){++noFix;return;}
  char row[384];size_t size=dragon::wardrive::csv(row,sizeof(row),mac,name,length,auth,rssi,channel,isBle,recordingFix(),now);
  if(size&&driveLog.append(row,size)){if(isBle)++ble;else ++wifi;}
}
}
bool wardriveActive(){return running;}
bool wardriveI2cBusy(){
#ifdef BOARD_GPS_LC76G
  return gpsReserved;
#else
  return false;
#endif
}
bool wardriveBusy(){return owned;}
bool wardriveSyncClock(){
  if(wardriveI2cBusy()||!located(millis()))return false;
  const auto& fix=recordingFix();
  return configureClock(fix.epoch+uint32_t(millis()-fix.positionAt)/1000);
}
#ifdef BOARD_GPS_UART
dragon::gpsuart::Config gpsExternalConfig(){return externalGpsConfig;}
void gpsExternalSuspend(bool value){
  if(externalGpsSuspended==value)return;
  externalGpsSuspended=value;externalGpsApply();
}
bool gpsExternalConfigure(const dragon::gpsuart::Config& value){
  if(!dragon::gpsuart::valid(value)||externalGpsSuspended||owned||wardriveUploadBusy()||reconOwnsRadio()||reconStorageBusy())return false;
  if(!externalGpsPrefsReady||externalGpsPrefs.putUInt("config",dragon::gpsuart::encode(value))!=sizeof(uint32_t))return false;
  externalGpsConfig=value;externalGpsApply();
  strlcpy(notice,!value.enabled?"External GPS disabled":externalGpsOpen?"External NMEA GPS enabled; waiting for data":"GPS UART unavailable; check free memory",sizeof(notice));
  return !value.enabled||externalGpsOpen;
}
void gpsExternalStatus(char* out,size_t cap){
  snprintf(out,cap,"enabled=%u open=%u suspended=%u rx=%d tx=none baud=%lu bytes=%lu nmea=%lu checksum_bad=%lu errors=%lu present=%u fix=%u",externalGpsConfig.enabled,externalGpsOpen,externalGpsSuspended,externalGpsConfig.rx,(unsigned long)externalGpsConfig.baud,externalGpsBytes,gps.sentences,gps.badChecksum,gpsErrors,present,located(millis()));
}
#endif
WardriveSnapshot wardriveSnapshot(){
  WardriveSnapshot s;s.running=running;s.busy=owned;s.fixed=located(millis());s.present=present;s.failed=driveLog.failed()||radioFailed;
  s.wifi=wifi;s.ble=ble;s.seenWifi=seenWifi;s.seenBle=seenBle;s.elapsedMs=running?millis()-started:ended-started;
  s.noFix=noFix;s.late=late;s.duplicates=duplicate;s.drops=driveLog.dropped;s.gpsErrors=gpsErrors;s.queued=driveLog.pending();
  s.bytes=driveLog.bytes;s.satellites=gps.fix.satellites;strlcpy(s.file,driveLog.path(),sizeof(s.file));strlcpy(s.message,notice,sizeof(s.message));return s;
}
void wardriveBegin(){
#ifdef BOARD_GPS_UART
  externalGpsBegin();strlcpy(notice,externalGpsConfig.enabled?"External NMEA GPS configured; waiting for data":"Enable external GPS in GPS setup before wardriving",sizeof(notice));
#endif
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
#if !defined(BOARD_GPS_LC76G) && !defined(BOARD_WATCHER) && !defined(BOARD_GPS_UART)
  strlcpy(notice,"Flash the 1.75-G GPS profile on LC76G hardware",sizeof(notice));return false;
#else
  if(owned)return false;
#ifdef BOARD_GPS_UART
  if(!externalGpsConfig.enabled||!externalGpsOpen){strlcpy(notice,"Enable external GPS in GPS setup before wardriving",sizeof(notice));return false;}
#endif
  radioFailed=false;
  if(stopwatch.running){strlcpy(notice,"Pause the timer before starting a wardrive",sizeof(notice));return false;}
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
#elif defined(BOARD_GPS_LC76G)
  snprintf(header,sizeof(header),"WigleWifi-1.6,appRelease=" DRAGON_VERSION ",model=ESP32-S3-Touch-AMOLED-1.75-G,release=" DRAGON_VERSION ",device=DragonDetector,display=AMOLED,board=waveshare_175_gps,brand=Waveshare\n%s",dragon::wardrive::Columns);
#else
  snprintf(header,sizeof(header),"WigleWifi-1.6,appRelease=" DRAGON_VERSION ",model=ESP32-S3-Touch-AMOLED-1.75,release=" DRAGON_VERSION ",device=DragonDetector,display=AMOLED,board=waveshare_175,brand=Waveshare\n%s",dragon::wardrive::Columns);
#endif
  if(!driveLog.begin("/wardrive","wigle","csv",header,strlen(header))){strlcpy(notice,driveLog.message,sizeof(notice));return false;}
  wifi=ble=noFix=duplicate=late=seenWifi=seenBle=0;started=ended=millis();
  audioStop();running=owned=true;
  if(!radioWardrive(true)){radioFailed=true;running=false;driveLog.stop();strlcpy(notice,"Radio scan setup failed; session stopped",sizeof(notice));return false;}
  strlcpy(notice,"WiFi + BLE; waiting for a valid GPS fix",sizeof(notice));return true;
#endif
}
void wardriveStop(){
  if(!running)return;ended=millis();running=false;driveLog.stop();
  strlcpy(notice,"Stopping; keep SD inserted until saved",sizeof(notice));
}
void wardriveLoop(){
#if defined(BOARD_GPS_LC76G) || defined(BOARD_WATCHER) || defined(BOARD_GPS_UART)
  gpsLoop();
#endif
#ifdef BOARD_AMOLED_175
  // USB-only power loss invalidates the RTC. A checked fresh GNSS pair can
  // recover UTC after acquisition, without rewriting an already-valid clock.
  static uint32_t clockAttempt=0;
  if(!wallTime()&&!wardriveI2cBusy()&&located(millis())&&uint32_t(millis()-clockAttempt)>=5000){clockAttempt=millis();wardriveSyncClock();}
#endif
  driveLog.loop();
  if(owned&&!driveLog.active()&&running){ended=millis();running=false;strlcpy(notice,driveLog.message,sizeof(notice));}
  if(owned&&!driveLog.busy()){
    owned=false;radioWardrive(false);if(!radioFailed)strlcpy(notice,driveLog.message,sizeof(notice));
  }
}
void wardriveWifi(const uint8_t* data,size_t length,int rssi,uint8_t channel,uint32_t received){
  auto f=dragon::parseWifi(data,length);
  if(!f.valid||(f.frameType!=0x80&&f.frameType!=0x50))return;
  char auth[96];dragon::wardrive::authentication(auth,sizeof(auth),data,length,f.security);
  record(f.bssid,f.ssid,f.ssidLength,auth,rssi,f.channel?f.channel:channel,false,0,received);
}
void wardriveWifiResult(const uint8_t* mac,const uint8_t* name,size_t length,const char* auth,int rssi,uint8_t channel,uint32_t received){
  // Driver scan results contain one record per AP for this sweep. Repeated
  // sweeps intentionally retain new locations, matching survey-file semantics.
  record(mac,name,length,auth,rssi,channel,false,0,received,false);
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
  snprintf(out,cap,"%s\n%s\n\nGPS: %s / %u satellites\nWi-Fi %lu   BLE %lu\nNo fix %lu   Late %lu\nDuplicates %lu   SD drops %lu\nSaved %llu KB / queued %u B\nGPS errors %lu / NMEA %lu\n\nSD /dragon/wardrive\n%s",
    running?"WARDRIVING":owned?"SAVING":"WARDRIVE IDLE",
#ifdef BOARD_GPS_LC76G
    "LC76G / WiFi + BLE",
#elif defined(BOARD_GPS_UART)
    "External UART GPS / WiFi + BLE",
#elif defined(BOARD_WATCHER)
    "AT6668 UART / passive Wi-Fi + BLE",
#else
    "GPS firmware profile required",
#endif
    fixed?"FIX":present?"waiting":"no data",gps.fix.satellites,
    wifi,ble,noFix,late,duplicate,driveLog.dropped,(unsigned long long)(driveLog.bytes/1024),unsigned(driveLog.pending()),gpsErrors,gps.sentences,
    owned?(fixed?"Recording located observations":"No valid fix: located rows paused"):notice);
}
bool wardriveCommand(const String& c){
  if(c=="gps clock sync"){Serial.println(wardriveSyncClock()?"GPS_CLOCK_OK":"GPS_CLOCK_ERROR fresh fix / RTC required");return true;}
#ifdef BOARD_GPS_UART
  if(c=="gps external status"){char state[220];gpsExternalStatus(state,sizeof(state));Serial.printf("GPS_EXTERNAL %s\n",state);return true;}
  if(c=="gps external off"){auto value=gpsExternalConfig();value.enabled=false;Serial.println(gpsExternalConfigure(value)?"GPS_EXTERNAL_OK":"GPS_EXTERNAL_ERROR busy or save failed");return true;}
  if(c.startsWith("gps external on ")){
    int rx=-1;unsigned long baud=0;char tail;bool ok=sscanf(c.c_str(),"gps external on %d %lu %c",&rx,&baud,&tail)==2&&gpsExternalConfigure({true,rx,uint32_t(baud)});
    Serial.println(ok?"GPS_EXTERNAL_OK":"GPS_EXTERNAL_ERROR allowed pins 18/17/16/44; baud 4800/9600/19200/38400/57600/115200; stop sessions first");return true;
  }
#endif
  if(c=="gps reception"){
    bool recent=uint32_t(millis()-gps.viewAt)<=5000;
    Serial.printf("GPS_RECEPTION rmc=%lu gga=%lu gsv=%lu rmc_status=%c in_view=%u best_snr=%u quality=%u used=%u fresh=%u\n",gps.rmcSentences,gps.ggaSentences,gps.gsvSentences,gps.rmcStatus?gps.rmcStatus:'?',recent?gps.inView:0,recent?gps.bestSnr:0,gps.fix.quality,gps.fix.satellites,located(millis()));return true;
  }
  if(c=="wardrive radio"){radioWardriveStatus();return true;}
  if(c=="wardrive counters"){
    auto s=wardriveSnapshot();
    Serial.printf("WARDRIVE_COUNTERS running=%u busy=%u fix=%u present=%u failed=%u elapsed_ms=%lu wifi=%lu ble=%lu seen_wifi=%lu seen_ble=%lu no_fix=%lu late=%lu duplicates=%lu drops=%lu queued=%lu bytes=%llu gps_errors=%lu satellites=%u\n",s.running,s.busy,s.fixed,s.present,s.failed,s.elapsedMs,s.wifi,s.ble,s.seenWifi,s.seenBle,s.noFix,s.late,s.duplicates,s.drops,s.queued,(unsigned long long)s.bytes,s.gpsErrors,s.satellites);return true;
  }
#ifdef BOARD_GPS_LC76G
  if(c=="gps transport"){
    Serial.printf("GPS_TRANSPORT present=%u phase=%u failure_stage=%u wire_status=%u reply=%u length=%lu remaining=%u bytes=%lu reads=%lu errors=%lu max_us=%lu checksum_bad=%lu overlong=%lu positions=%lu sentences=%lu recoveries=%u reserved=%u bus_hz=%lu\n",present,unsigned(phase),gpsFailureStage,gpsWireStatus,gpsLastReply,gpsLastLength,remaining,gpsReadBytes,gpsReadCount,gpsErrors,gpsMaxReadUs,gps.badChecksum,gps.overlong,gps.positions,gps.sentences,recoveries,gpsReserved,Wire.getClock());return true;
  }
#endif
  if(c=="wardrive start")Serial.println(wardriveStart()?"WARDRIVE_OK started":"WARDRIVE_ERROR start failed; see status");
  else if(c=="wardrive stop"){wardriveStop();Serial.println("WARDRIVE_OK stopping");}
  else if(c=="wardrive end"){reconEndWardrive();Serial.println("WARDRIVE_OK finishing and returning to menu");}
  else if(c=="wardrive status"||c=="gps status"){
    char text[520];wardriveStatus(text,sizeof(text));Serial.println(text);
    if(located(millis()))Serial.printf("GPS_FIX %.7f %.7f %.2f epoch=%lu hdop=%.2f\n",gps.fix.latitude,gps.fix.longitude,gps.fix.altitude,gps.fix.epoch,gps.fix.hdop);
    Serial.println("WARDRIVE_END");
  }else return false;
  return true;
}
#endif
