#include "app.h"
#include "board_pins.h"
#include <Preferences.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <time.h>
#include <stdarg.h>

dragon::Stopwatch stopwatch;
uint8_t displayRotation=0;
bool rtcAvailable=false;
int clockOffsetMinutes=0;
static Preferences prefs;
static lv_color_t* rotationBuffer=nullptr;
static void (*physicalFlush)(lv_disp_drv_t*,const lv_area_t*,lv_color_t*)=nullptr;
uint64_t stopwatchNow(){return esp_timer_get_time();}
static void rotatedFlush(lv_disp_drv_t* drv,const lv_area_t* a,lv_color_t* pixels){
  const uint8_t r=drv->rotated;
  if(!r){physicalFlush(drv,a,pixels);return;}
  const int w=a->x2-a->x1+1,h=a->y2-a->y1+1;
  // One persistent rotation buffer, no allocation or LVGL buffer subdivision per
  // frame. Even CO5300 update windows stay even in every orientation.
  auto p=dragon::rotatePoint(a->x1,a->y1,SCREEN_RES,r),q=dragon::rotatePoint(a->x2,a->y2,SCREEN_RES,r);
  lv_area_t out{lv_coord_t(std::min(p.x,q.x)),lv_coord_t(std::min(p.y,q.y)),lv_coord_t(std::max(p.x,q.x)),lv_coord_t(std::max(p.y,q.y))};
  const int stride=out.x2-out.x1+1;
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto dest=dragon::rotatePoint(x+a->x1,y+a->y1,SCREEN_RES,r);rotationBuffer[(dest.y-out.y1)*stride+dest.x-out.x1]=pixels[y*w+x];}
  // DMA completion still releases the original LVGL buffer, so neither buffer
  // can be overwritten while the panel is transferring it.
  physicalFlush(drv,&out,rotationBuffer);
}
static void applyRotation(){
  auto* display=lv_disp_get_default();
  if(display->driver->rotated==displayRotation)return;
  lv_disp_set_rotation(display,lv_disp_rot_t(displayRotation));
  for(auto* input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))lv_indev_reset(input,nullptr);
  lv_obj_invalidate(lv_scr_act());
}
void wearableBegin(){
  prefs.begin("dd-wearable",false);
  clockOffsetMinutes=constrain(prefs.getInt("utc-offset",0),-720,840);
  displayRotation=prefs.getUChar("rotation",0)&3;
  if(prefs.getBool("auto",false))prefs.putBool("auto",false);
  boardSensorsBegin();
  auto* driver=lv_disp_get_default()->driver;
  #if defined(BOARD_STOPWATCH) || defined(BOARD_XIAO_ROUND) || defined(BOARD_CROWPANEL_21)
  // These synchronous display transfers can read the persistent PSRAM frame.
  constexpr uint32_t rotationCaps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
  #else
  constexpr uint32_t rotationCaps=MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA;
  #endif
  rotationBuffer=(lv_color_t*)heap_caps_malloc(driver->draw_buf->size*sizeof(lv_color_t),rotationCaps);
  if(rotationBuffer){physicalFlush=driver->flush_cb;driver->flush_cb=rotatedFlush;driver->sw_rotate=0;applyRotation();}
  else {displayRotation=0;Serial.println("ORIENTATION_ERROR memory; upright retained");}
  uint32_t epoch=0;if(rtcAvailable&&boardReadRtc(epoch))setWallTime(epoch);
  Serial.printf("WEARABLE_READY rtc=%u rotation=%u manual=1\n",rtcAvailable,displayRotation);
}
bool configureClock(uint32_t epoch){
  if(!dragon::validEpoch(epoch))return false;
#if defined(BOARD_STOPWATCH) || defined(BOARD_XIAO_ROUND) || defined(BOARD_AMOLED_175)
  // These profiles promise a hardware clock. Never silently report a volatile
  // software-only write as successful if their RTC is disconnected or failed.
  if(!rtcAvailable)return false;
#endif
  if(rtcAvailable&&!boardWriteRtc(epoch))return false;
  setWallTime(epoch);return true;
}
void configureClockOffset(int minutes){clockOffsetMinutes=constrain(minutes,-720,840);prefs.putInt("utc-offset",clockOffsetMinutes);}
void clockLocal(dragon::Calendar& date){
  uint32_t epoch=wallTime();if(!epoch){date={};return;}
  time_t local=int64_t(epoch)+clockOffsetMinutes*60;struct tm t{};gmtime_r(&local,&t);
  date={t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec};
}
bool configureOrientation(int degrees){
  if(!rotationBuffer||degrees<0||degrees>270||degrees%90)return false;
  displayRotation=(4-degrees/90)&3;prefs.putUChar("rotation",displayRotation);applyRotation();return true;
}
static void reply(const char* format,...){
  char line[256];va_list args;va_start(args,format);int n=vsnprintf(line,sizeof(line),format,args);va_end(args);
  if(n<=0||n>=int(sizeof(line)))return;
  Serial.write((const uint8_t*)line,n);
  // ESP32-S3 USB-JTAG/Windows can hold an exactly full 64-byte IN packet
  // until another packet arrives. Terminate that boundary with a blank line.
  if(n%64==0)Serial.write('\n');
}
bool wearableCommand(const String& c){
  if(c=="clock hardware"){
    uint32_t epoch=0;bool valid=rtcAvailable&&boardReadRtc(epoch);
    reply("CLOCK_HARDWARE detected=%u valid=%u epoch=%lu\n",rtcAvailable,valid,valid?epoch:0);return true;
  }
  if(c=="clock status"){reply("CLOCK_STATUS rtc=%u valid=%u epoch=%lu offset=%d\n",rtcAvailable,wallTime()!=0,wallTime(),clockOffsetMinutes);return true;}
  if(c.startsWith("clock offset ")){int minutes;char extra;if(sscanf(c.c_str(),"clock offset %d %c",&minutes,&extra)!=1||minutes< -720||minutes>840){Serial.println("CLOCK_ERROR offset");return true;}configureClockOffset(minutes);Serial.println("CLOCK_OK");return true;}
  if(c=="orientation status"){reply("ORIENTATION_STATUS manual=1 degrees=%u\n",((4-displayRotation)&3)*90);return true;}
  if(c.startsWith("orientation set ")){int degrees=-1;char extra;bool valid=sscanf(c.c_str(),"orientation set %d %c",&degrees,&extra)==1;Serial.println(valid&&configureOrientation(degrees)?"ORIENTATION_OK":"ORIENTATION_ERROR unavailable or invalid angle");return true;}
  if(c=="stopwatch status"){reply("STOPWATCH_STATUS running=%u elapsed_us=%llu laps=%lu retained=%u\n",stopwatch.running,stopwatch.elapsed(stopwatchNow()),stopwatch.lapCount,unsigned(stopwatch.retained()));return true;}
  if(c=="stopwatch laps"){for(size_t i=0;i<stopwatch.retained();++i){auto* l=stopwatch.newest(i);reply("LAP %lu split_us=%llu total_us=%llu\n",l->number,l->split,l->total);}Serial.println("LAPS_END");return true;}
  if(c=="stopwatch start"){stopwatch.start(stopwatchNow());Serial.println("STOPWATCH_OK");return true;}
  if(c=="stopwatch pause"){stopwatch.pause(stopwatchNow());Serial.println("STOPWATCH_OK");return true;}
  if(c=="stopwatch lap"){Serial.println(stopwatch.lap(stopwatchNow())?"STOPWATCH_OK":"STOPWATCH_ERROR not running or zero lap");return true;}
  if(c=="stopwatch finish"){Serial.println(stopwatch.finish(stopwatchNow())?"STOPWATCH_OK":"STOPWATCH_ERROR not running or zero attempt");return true;}
  if(c=="stopwatch reset"){Serial.println(stopwatch.reset()?"STOPWATCH_OK":"STOPWATCH_ERROR pause before reset");return true;}
  return false;
}
