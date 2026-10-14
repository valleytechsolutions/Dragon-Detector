#include <Arduino.h>
#include "app.h"
#include "board_pins.h"
#include <lvgl.h>
#include <esp_timer.h>
static String serialLine;
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
    delay(1);
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
  auto* display=lv_disp_get_default();
  captureWidth=lv_disp_get_hor_res(display);captureHeight=lv_disp_get_ver_res(display);
  const size_t bytes=size_t(captureWidth)*captureHeight*sizeof(lv_color_t);
  capturePixels=(lv_color_t*)ps_calloc(1,bytes);
  if(!capturePixels){Serial.println("SCREENSHOT_FAILED memory");return;}
  captureStage(1);const bool wasEnabled=radioEnabled;
  captureStage(2);panelFlush=display->driver->flush_cb;display->driver->flush_cb=captureFlush;
  lv_obj_invalidate(lv_scr_act());lv_refr_now(display);
  display->driver->flush_cb=panelFlush;
  // Capture the live sweep before pausing reception for the bulk USB transfer.
  setRadio(false);
  captureStage(3);
  const uint32_t crc=dragon::crc32((const uint8_t*)capturePixels,bytes)^0xffffffffu;
  Serial.printf("SCREENSHOT %u %u %u\n",captureWidth,captureHeight,unsigned(bytes));
  const bool complete=serialBytes((const uint8_t*)capturePixels,bytes);
  Serial.printf("\nSCREENSHOT_%s %08lx\n",complete?"END":"FAILED",crc);
  captureStage(4);free(capturePixels);capturePixels=nullptr;
  captureStage(5);setRadio(wasEnabled);captureStage(0);
}
static void health(){
  unsigned count=0;for(auto& c:contacts)if(c.used&&millis()-c.seen<30000)++count;
  Serial.printf("HEALTH uptime=%lu heap=%u psram=%u contacts=%u wifi=%lu ble=%lu alerts=%lu drops=%lu storage_drops=%lu malformed=%lu channel=%u phase=%s sd=%s audio=%u\n",
    millis(),ESP.getFreeHeap(),ESP.getFreePsram(),count,wifiFrames,bleFrames,alertTotal,queueDrops,storageDrops,malformedFrames,wifiChannel,blePhase?"BLE":"WIFI",sdReady?"OK":"MISSING",unsigned(audioStatus().mode));
  Serial.printf("DISPLAY %dx%d\n",lv_disp_get_hor_res(nullptr),lv_disp_get_ver_res(nullptr));
  Serial.printf("RUNTIME reset=%d stack_free=%u capture_previous=%lu\n",esp_reset_reason(),unsigned(uxTaskGetStackHighWaterMark(nullptr)),previousCaptureStage);
  Serial.printf("RADIO_STATS capacity=%u queue_high=%u batch_us=%lu lag_ms=%lu evictions=%lu cache_hits=%lu cache_misses=%lu active_ble=%u\n",unsigned(MAX_CONTACTS),radioQueueHigh,radioMaxBatchUs,radioMaxLagMs,contactEvictions,contactCacheHits,contactCacheMisses,activeBle);
  if(batteryPercent()>=0)Serial.printf("BATTERY %d%% %s\n",batteryPercent(),batteryCharging()?"charging":"discharging");
}
static void command(const String& c){
  if(badgeCommand(c) || wearableCommand(c) || detectorCommand(c) || audioCommand(c) || uiCommand(c))return;
  if(c=="status"){health();Serial.printf("SESSION %s\n",sessionPath.c_str());}
  else if(c=="selftest")runSelfTests();
#ifdef BOARD_STOPWATCH
  else if(c=="touch status"){extern void stopwatchTouchStatus();stopwatchTouchStatus();}
#endif
  else if(c=="pause")setRadio(false);
  else if(c=="resume")setRadio(true);
  else if(c=="record")audioRecord();
  else if(c=="stop")audioStop();
  else if(c.startsWith("screen "))uiShow(c.substring(7));
  else if(c=="audio status"){auto a=audioStatus();Serial.printf("AUDIO mode=%u seconds=%lu peak=%u volume=%u file=%s\n",unsigned(a.mode),a.seconds,a.peak,a.volume,a.message);}
  else if(c.startsWith("volume "))audioVolume(c.substring(7).toInt());
  else if(c.startsWith("ls ") && sdReady){xSemaphoreTake(sdMutex,portMAX_DELAY);File dir=SD_MMC.open(c.substring(3));File f=dir.openNextFile();for(int i=0;f&&i<128;++i){Serial.printf("%s %u %s\n",f.isDirectory()?"DIR":"FILE",unsigned(f.size()),f.path());f.close();f=dir.openNextFile();}f.close();dir.close();xSemaphoreGive(sdMutex);}
  else if(c.startsWith("put ") && sdReady){
    int split=c.lastIndexOf(' ');String path=c.substring(4,split);long total=c.substring(split+1).toInt();
    if(split<5 || total<=0 || total>16*1024*1024 || path.indexOf("..")>=0 || (!path.startsWith("/music/")&&!path.startsWith("/dragon/"))){Serial.println("PUT_ERROR invalid path or length");return;}
    if(audioStatus().mode==AudioMode::Recording || audioStatus().mode==AudioMode::Playing){Serial.println("PUT_ERROR stop audio first");return;}
    xSemaphoreTake(sdMutex,portMAX_DELAY);bool exists=SD_MMC.exists(path)||SD_MMC.exists(path+".part");File file;if(!exists)file=SD_MMC.open(path+".part",FILE_WRITE);xSemaphoreGive(sdMutex);
    if(!file){Serial.println("PUT_ERROR file exists or cannot open");return;}
    bool enabled=radioEnabled;setRadio(false);Serial.println("PUT_READY");uint8_t buffer[1024];size_t written=0;uint32_t last=millis();bool ok=true;
    while(written<size_t(total) && millis()-last<5000){int available=Serial.available();if(available){size_t n=Serial.readBytes(buffer,std::min<size_t>(sizeof(buffer),std::min<size_t>(available,total-written)));xSemaphoreTake(sdMutex,portMAX_DELAY);size_t result=file.write(buffer,n);xSemaphoreGive(sdMutex);if(result!=n){ok=false;break;}written+=n;last=millis();Serial.printf("PUT_ACK %u\n",unsigned(written));}else delay(1);}
    xSemaphoreTake(sdMutex,portMAX_DELAY);file.close();ok=ok&&written==size_t(total);if(ok)ok=SD_MMC.rename(path+".part",path);if(!ok)SD_MMC.remove(path+".part");xSemaphoreGive(sdMutex);setRadio(enabled);Serial.println(ok?"PUT_OK":"PUT_ERROR incomplete transfer");
  }
  else if(c.startsWith("get ") && sdReady){
    xSemaphoreTake(sdMutex,portMAX_DELAY);File file=SD_MMC.open(c.substring(4));xSemaphoreGive(sdMutex);
    if(!file||file.isDirectory()){Serial.println("GET_ERROR");return;}bool enabled=radioEnabled;setRadio(false);size_t remaining=file.size();Serial.printf("FILE_BYTES %u\n",unsigned(remaining));uint8_t data[1024];
    while(remaining){xSemaphoreTake(sdMutex,portMAX_DELAY);size_t n=file.read(data,std::min(sizeof(data),remaining));xSemaphoreGive(sdMutex);if(!n||!serialBytes(data,n))break;remaining-=n;}xSemaphoreTake(sdMutex,portMAX_DELAY);file.close();xSemaphoreGive(sdMutex);Serial.println("\nFILE_END");setRadio(enabled);
  }
  else if(c.startsWith("play "))audioPlay(c.substring(5));
  else if(c=="capture on")setCapture(true);
  else if(c=="capture off")setCapture(false);
  else if(c=="contacts"){for(auto& d:contacts)if(d.used){char mac[18];macText(d.mac,mac);Serial.printf("%s %s %d %s [%s] %s\n",d.ble?"BLE":"WIFI",mac,d.rssi,d.name,dragon::familyName(d.match.family),d.match.evidence);}}
  else if(c=="screenshot")screenshot();
  else Serial.println("Commands: status, contacts, selftest, pause, resume, record, stop, play /path.mp3, capture on|off, screenshot");
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
  if(ESP.getPsramSize()<4*1024*1024){Serial.println("FATAL: expected octal PSRAM");while(true)delay(100);}
  contacts.data=(Contact*)ps_malloc(MAX_CONTACTS*sizeof(Contact));
  if(!contacts.data){Serial.println("FATAL: contact storage allocation failed");while(true)delay(100);}
  for(size_t i=0;i<MAX_CONTACTS;++i)new(&contacts[i]) Contact{};
  runSelfTests();boardBegin();uiBegin();wearableBegin();
  uint32_t at=millis();while(millis()-at<1600){uiLoop();delay(5);}
  storageBegin();badgeInit();audioBegin();detectorBegin();radioBegin();Serial.println("DRAGON READY");health();
}
void loop(){
  boardLoop();radioLoop();storageLoop();detectorLoop();audioLoop();uiLoop();wearableLoop();badgeLoop();
  while(Serial.available()){char c=Serial.read();if(c=='\n'){serialLine.trim();if(serialOverflow)Serial.println("COMMAND_ERROR too long");else command(serialLine);serialLine="";serialOverflow=false;}else if(serialLine.length()<600)serialLine+=c;else serialOverflow=true;}
  if(millis()-lastHealth>10000){lastHealth=millis();health();}
  delay(2);
}
