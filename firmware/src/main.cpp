#include <Arduino.h>
#include "app.h"
#include "wardrive.h"
#include "personal_theme.h"
#include "notebook.h"
#include "vendor_registry.h"
#include "background_radio.h"
#include "ble_lab.h"
#include "recon.h"
#include "board_pins.h"
#include "flash_budget.h"
#include <lvgl.h>
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include <LittleFS.h>
static String serialLine;
#ifndef BOARD_RADAR_LITE
SET_LOOP_TASK_STACK_SIZE(12288);
#endif
static bool serialOverflow=false;
static uint32_t lastHealth=0;
RTC_NOINIT_ATTR static volatile uint32_t captureTrace[2];
static constexpr uint32_t CAPTURE_TRACE_MAGIC=0xdd050001;
static uint32_t previousCaptureStage=0;
static void captureStage(uint32_t stage){captureTrace[0]=CAPTURE_TRACE_MAGIC;captureTrace[1]=stage;}
static bool serialBytes(const uint8_t* data,size_t length){
  size_t sent=0;uint32_t last=millis();
  while(sent<length){
    // Small writes with backpressure avoid the native USB driver's overflow
    // policy, which can discard already queued bytes during a stalled transfer.
    size_t room=Serial.availableForWrite();
    size_t n=room?Serial.write(data+sent,std::min<size_t>(128,std::min(room,length-sent))):0;
    if(n){sent+=n;last=millis();}else if(millis()-last>5000)return false;
    radioTransferTick();delay(1);
  }
  return true;
}
static lv_color_t* capturePixels=nullptr;
static int captureWidth=0,captureHeight=0;
static void (*panelFlush)(lv_disp_drv_t*,const lv_area_t*,lv_color_t*)=nullptr;
static void captureFlush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels){
  // Mirror the ordinary display refresh before a board driver swaps/reuses its
  // draw buffer. Do not create LVGL's temporary display/draw context on the stack.
  if(capturePixels && area->x1>=0 && area->y1>=0 && area->x2<captureWidth && area->y2<captureHeight){
    const int width=area->x2-area->x1+1;
    if(width>0)for(int y=area->y1;y<=area->y2;++y)
      memcpy(capturePixels+y*captureWidth+area->x1,pixels+(y-area->y1)*width,width*sizeof(lv_color_t));
  }
  panelFlush(driver,area,pixels);
}
static void screenshot(){
  // Finish pending partial refreshes before mirroring a complete frame.
  auto* display=lv_disp_get_default();
  lv_refr_now(display);
  captureWidth=lv_disp_get_hor_res(display);captureHeight=lv_disp_get_ver_res(display);
  const size_t bytes=size_t(captureWidth)*captureHeight*sizeof(lv_color_t);
  capturePixels=(lv_color_t*)ps_calloc(1,bytes);
  if(!capturePixels){Serial.println("SCREENSHOT_FAILED memory");return;}
  captureStage(1);RadioTransfer transfer;
  captureStage(2);panelFlush=display->driver->flush_cb;display->driver->flush_cb=captureFlush;
  _lv_inv_area(display,nullptr);lv_obj_invalidate(lv_scr_act());lv_refr_now(display);
  display->driver->flush_cb=panelFlush;
  // Reception continues while the mirrored pixels are streamed over USB.
  captureStage(3);
  const uint32_t crc=dragon::crc32((const uint8_t*)capturePixels,bytes)^0xffffffffu;
  Serial.printf("SCREENSHOT %u %u %u\n",captureWidth,captureHeight,unsigned(bytes));
  const bool complete=serialBytes((const uint8_t*)capturePixels,bytes);
  Serial.printf("\nSCREENSHOT_%s %08lx\n",complete?"END":"FAILED",crc);
  captureStage(4);free(capturePixels);capturePixels=nullptr;
  captureStage(5);captureStage(0);
}
static void health(){
  unsigned count=0;for(auto& c:contacts)if(c.used&&millis()-c.seen<30000)++count;
  Serial.printf("HEALTH uptime=%lu heap=%u psram=%u contacts=%u wifi=%lu ble=%lu alerts=%lu drops=%lu storage_drops=%lu malformed=%lu channel=%u phase=%s"
#if BOARD_HAS_SD
    " sd=%s"
#elif defined(BOARD_RADAR_LITE)
    " storage=RADAR_ONLY"
#else
    " storage=FLASH"
#endif
    " audio=%u\n",
    millis(),ESP.getFreeHeap(),ESP.getFreePsram(),count,wifiFrames,bleFrames,alertTotal,queueDrops,storageDrops,malformedFrames,wifiChannel,radioEnabled?(blePhase?"BLE":"WIFI"):"IDLE",
#if BOARD_HAS_SD
    sdReady?"OK":"MISSING",
#endif
    unsigned(audioStatus().mode));
  Serial.printf("DISPLAY %dx%d\n",lv_disp_get_hor_res(nullptr),lv_disp_get_ver_res(nullptr));
  Serial.printf("RUNTIME reset=%d stack_free=%u capture_previous=%lu\n",esp_reset_reason(),unsigned(uxTaskGetStackHighWaterMark(nullptr)),previousCaptureStage);
  Serial.printf("RADIO_STATS capacity=%u queue_high=%u batch_us=%lu lag_ms=%lu evictions=%lu cache_hits=%lu cache_misses=%lu active_ble=%u\n",unsigned(MAX_CONTACTS),radioQueueHigh,radioMaxBatchUs,radioMaxLagMs,contactEvictions,contactCacheHits,contactCacheMisses,activeBle);
  if(batteryPercent()>=0)Serial.printf("BATTERY %d%% %s\n",batteryPercent(),batteryCharging()?"charging":"discharging");
}
static uint32_t loopLastUs[6]{},loopMaxUs[6]{};
static void command(const String& c){
  if(wardriveCommand(c))return;
  if(wardriveBusy()&&c!="status"&&c!="scan status"&&c!="memory status"&&c!="loop diagnostics"&&c!="screen wardrive"){
    Serial.println("WARDRIVE_BUSY stop wardriving and wait for SD save first");return;
  }
  if(personalThemeCommand(c))return;
  if(c=="system restart"){
    if(stopwatch.running||reconOwnsRadio()||bleLabOwnsRadio()||reconStorageBusy()){Serial.println("SYSTEM_ERROR stop timing and radio sessions first");return;}
    Serial.println("SYSTEM_RESTARTING");Serial.flush();delay(50);ESP.restart();return;
  }
  if(c=="scan status"){Serial.printf("SCAN_STATUS enabled=%u ble_ms=%lu wifi_ms=%lu mode=%u lab=%u hunt=%d priority_channel=%u",radioEnabled,bleListenMs,wifiListenMs,unsigned(reconMode()),bleLabOwnsRadio()||reconOwnsRadio(),trackedContact,channelLock);for(unsigned i=1;i<=11;++i)Serial.printf(" ch%u=%lu",i,channelDwellMs[i]);Serial.printf(" standby=%u\n",radioStandby());return;}
  if(c=="memory status"){auto* app=esp_ota_get_running_partition();size_t size=ESP.getSketchSize(),slot=app?app->size:0;Serial.printf("MEMORY flash=%u app=%u slot=%u app_free=%u reserve_percent=%u legacy_used=%u legacy_total=%u heap=%u psram=%u\n",ESP.getFlashChipSize(),unsigned(size),unsigned(slot),unsigned(slot>=size?slot-size:0),dragon::FlashReservePercent,LittleFS.usedBytes(),LittleFS.totalBytes(),ESP.getFreeHeap(),ESP.getFreePsram());return;}
  if(c=="loop diagnostics"){const char* names[]={"board","radio","history","audio","ui","badge"};for(int i=0;i<6;++i)Serial.printf("LOOP_TIMING %s last_us=%lu max_us=%lu\n",names[i],loopLastUs[i],loopMaxUs[i]);Serial.println("LOOP_END");return;}
#ifdef BOARD_RADAR_LITE
  // Only locally operable Lite pages and timing controls are added here. Radio
  // ownership, audio, badge upload and full touch menus remain unavailable.
  const bool allowed=c=="badge info"||c=="status"||c=="contacts"||c=="selftest"||c=="screenshot"||c=="ui status"||c=="radar status"||c=="display status"||c=="axon status"||c=="traffic status"||c=="detector status";
  const bool timing=c.startsWith("time ")||c=="clock status"||c=="clock hardware"||c.startsWith("clock offset ")||c=="orientation status"||c=="stopwatch status"||c=="stopwatch laps"||c=="stopwatch start"||c=="stopwatch pause"||c=="stopwatch reset"||c=="stopwatch lap"||c=="stopwatch finish";
  const bool page=c=="screen radar"||c=="screen clock"||c=="screen stopwatch"||c=="screen signals"||c=="screen diagnostics"||c=="screen startup";
  if(!allowed&&!timing&&!page){Serial.println("LITE_LIMITED no touch menus, badge, audio or radio controls");return;}
#endif
  if(c.startsWith("vendor ")){uint8_t mac[6];unsigned v[6];char tail;if(sscanf(c.c_str(),"vendor %2x:%2x:%2x:%2x:%2x:%2x %c",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&tail)==6){for(unsigned i=0;i<6;++i)mac[i]=v[i];auto* name=dragon::registeredVendor(mac);Serial.printf("REGISTRY_VENDOR %s\n",name?name:"No universal MA-L match");}else Serial.println("REGISTRY_ERROR address");return;}
  if(c=="registry status"){Serial.printf("REGISTRY entries=%u bits=24 source=IEEE_MA_L\n",dragon::vendorRegistryCount());return;}
#ifndef BOARD_RADAR_LITE
  if(notebookCommand(c))return;
#endif
  if(reconCommand(c) || bleLabCommand(c) || badgeCommand(c) || wearableCommand(c) || detectorCommand(c) || audioCommand(c) || uiCommand(c))return;
  if(c=="status"){health();Serial.printf("SESSION %s\n",sessionPath.c_str());}
  else if(c=="selftest")runSelfTests();
#ifdef BOARD_XIAO_ROUND
  else if(c=="touch status"){extern void xiaoTouchStatus();xiaoTouchStatus();}
#endif
#ifdef BOARD_STOPWATCH
  else if(c=="touch status"){extern void stopwatchTouchStatus();stopwatchTouchStatus();}
#endif
  else if(c=="pause")Serial.println("RADIO_ALWAYS_ON select an explicit Waveshare radio mode instead");
  else if(c=="resume")setRadio(true);
  else if(c=="record")audioRecord();
  else if(c=="stop")audioStop();
  else if(c.startsWith("screen "))uiShow(c.substring(7));
  else if(c=="audio status"){auto a=audioStatus();Serial.printf("AUDIO mode=%u seconds=%lu peak=%u volume=%u file=%s\n",unsigned(a.mode),a.seconds,a.peak,a.volume,a.message);}
  else if(c.startsWith("volume "))audioVolume(c.substring(7).toInt());
#if BOARD_HAS_SD
  else if(c.startsWith("ls ") && sdReady){xSemaphoreTake(sdMutex,portMAX_DELAY);File dir=CardFS.open(c.substring(3));File f=dir.openNextFile();for(int i=0;f&&i<128;++i){Serial.printf("%s %u %s\n",f.isDirectory()?"DIR":"FILE",unsigned(f.size()),f.path());f.close();f=dir.openNextFile();}f.close();dir.close();xSemaphoreGive(sdMutex);}
  else if(c.startsWith("put ") && sdReady){
    int split=c.lastIndexOf(' ');String path=c.substring(4,split);long total=c.substring(split+1).toInt();
    if(split<5 || total<=0 || total>16*1024*1024 || path.indexOf("..")>=0 || (!path.startsWith("/music/")&&!path.startsWith("/dragon/"))){Serial.println("PUT_ERROR invalid path or length");return;}
    if(audioStatus().mode==AudioMode::Recording || audioStatus().mode==AudioMode::Playing){Serial.println("PUT_ERROR stop audio first");return;}
    xSemaphoreTake(sdMutex,portMAX_DELAY);bool exists=CardFS.exists(path)||CardFS.exists(path+".part");File file;if(!exists)file=CardFS.open(path+".part",FILE_WRITE);xSemaphoreGive(sdMutex);
    if(!file){Serial.println("PUT_ERROR file exists or cannot open");return;}
    RadioTransfer transfer;Serial.println("PUT_READY");uint8_t buffer[1024];size_t written=0;uint32_t last=millis();bool ok=true;
    while(written<size_t(total) && millis()-last<5000){int available=Serial.available();if(available){size_t n=Serial.readBytes(buffer,std::min<size_t>(sizeof(buffer),std::min<size_t>(available,total-written)));xSemaphoreTake(sdMutex,portMAX_DELAY);size_t result=file.write(buffer,n);xSemaphoreGive(sdMutex);if(result!=n){ok=false;break;}written+=n;last=millis();Serial.printf("PUT_ACK %u\n",unsigned(written));}radioTransferTick();delay(1);}
    xSemaphoreTake(sdMutex,portMAX_DELAY);file.close();ok=ok&&written==size_t(total);if(ok)ok=CardFS.rename(path+".part",path);if(!ok)CardFS.remove(path+".part");xSemaphoreGive(sdMutex);Serial.println(ok?"PUT_OK":"PUT_ERROR incomplete transfer");
  }
  else if(c.startsWith("get ") && sdReady){
    xSemaphoreTake(sdMutex,portMAX_DELAY);File file=CardFS.open(c.substring(4));xSemaphoreGive(sdMutex);
    if(!file||file.isDirectory()){Serial.println("GET_ERROR");return;}RadioTransfer transfer;size_t remaining=file.size();Serial.printf("FILE_BYTES %u\n",unsigned(remaining));uint8_t data[1024];
    while(remaining){xSemaphoreTake(sdMutex,portMAX_DELAY);size_t n=file.read(data,std::min(sizeof(data),remaining));xSemaphoreGive(sdMutex);if(!n||!serialBytes(data,n))break;remaining-=n;}xSemaphoreTake(sdMutex,portMAX_DELAY);file.close();xSemaphoreGive(sdMutex);Serial.println("\nFILE_END");
  }
#endif
  else if(c.startsWith("play "))audioPlay(c.substring(5));
#if BOARD_HAS_SD
  else if(c=="capture on")setCapture(true);
  else if(c=="capture off")setCapture(false);
#endif
  else if(c=="contacts"){for(auto& d:contacts)if(d.used){char mac[18];macText(d.mac,mac);Serial.printf("%s %s %d %s [%s] %s\n",d.ble?"BLE":"WIFI",mac,d.rssi,d.name,dragon::familyName(d.match.family),d.match.evidence);}}
  else if(c=="screenshot")screenshot();
  else Serial.println("Commands: status, contacts, selftest, resume, screen radar, screenshot"
#if BOARD_HAS_SD
    ", ls /, get /path, capture on|off"
#elif !defined(BOARD_RADAR_LITE)
    ", screen files, screen captures, tool status, lab status"
#endif
  );
}
void setup(){
  // The native USB CDC default is only 256 bytes. Reserve room before begin()
  // so flash writes cannot overflow incoming, acknowledged image chunks.
  Serial.setRxBufferSize(4096);
  Serial.setTxBufferSize(2048);
  Serial.begin(115200);delay(500);Serial.println("DRAGON DETECTOR " DRAGON_VERSION " / " BOARD_NAME " / boot");
  previousCaptureStage=captureTrace[0]==CAPTURE_TRACE_MAGIC?captureTrace[1]:0;
  Serial.printf("PREVIOUS_CAPTURE stage=%lu reset=%d\n",previousCaptureStage,esp_reset_reason());captureStage(0);
  Serial.printf("Flash=%u PSRAM=%u reset=%d\n",ESP.getFlashChipSize(),ESP.getPsramSize(),esp_reset_reason());
#ifdef BOARD_RADAR_LITE
  constexpr unsigned minimumPsram=2*1024*1024;
#else
  constexpr unsigned minimumPsram=4*1024*1024;
#endif
  if(ESP.getPsramSize()<minimumPsram){Serial.println("FATAL: insufficient PSRAM for board profile");while(true)delay(100);}
  contacts.data=(Contact*)ps_malloc(MAX_CONTACTS*sizeof(Contact));
  if(!contacts.data){Serial.println("FATAL: contact storage allocation failed");while(true)delay(100);}
  for(size_t i=0;i<MAX_CONTACTS;++i)new(&contacts[i]) Contact{};
  runSelfTests();boardBegin();personalThemeBegin();uiBegin();wearableBegin();
  uint32_t at=millis();while(millis()-at<1600){uiLoop();delay(5);}
  storageBegin();badgeInit();audioBegin();detectorBegin();notebookBegin();
#ifndef BOARD_RADAR_LITE
  if(!dragon::vendorRegistryBegin())Serial.println("REGISTRY unavailable: PSRAM allocation or integrity check");
#endif
  radioBegin();bleLabBegin();reconBegin();wardriveBegin();Serial.println("DRAGON READY");health();
}
void loop(){
  uint32_t stamp=micros();auto measured=[&](int i){uint32_t end=micros();loopLastUs[i]=end-stamp;loopMaxUs[i]=std::max(loopMaxUs[i],loopLastUs[i]);stamp=end;};
  boardLoop();wardriveLoop();bleLabLoop();reconLoop();measured(0);radioLoop();measured(1);storageLoop();detectorLoop();measured(2);audioLoop();measured(3);uiLoop();measured(4);badgeLoop();measured(5);
  while(Serial.available()){char c=Serial.read();if(c=='\n'){serialLine.trim();if(serialOverflow)Serial.println("COMMAND_ERROR too long");else command(serialLine);serialLine="";serialOverflow=false;}else if(serialLine.length()<1200)serialLine+=c;else serialOverflow=true;}
  if(millis()-lastHealth>10000){lastHealth=millis();health();}
  delay(2);
}
