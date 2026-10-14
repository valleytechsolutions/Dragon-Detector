#include "app.h"
#include <M5Unified.h>
#include <lvgl.h>
#include <time.h>
#include <atomic>

extern bool uiTouch(bool,int,int);
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;
static bool injected=false,injectedPress=false;
static int injectedX=0,injectedY=0;
static uint32_t vibrationUntil=0,lastFeedback=0,lastAlert=0;
static uint32_t touchPresses=0,touchSamples=0,lvglPresses=0;
static int lastTouchX=0,lastTouchY=0,lastRawX=0,lastRawY=0;
struct TouchEvent {bool pressed;int x,y;};
struct KeyEvent {bool start;uint64_t at;};
static TouchEvent latestTouch{};
static QueueHandle_t touchEvents=nullptr,keyEvents=nullptr;
static portMUX_TYPE inputMux=portMUX_INITIALIZER_UNLOCKED;
static bool inputTaskReady=false;
static std::atomic<uint32_t> inputDrops{0};
static void pollInputs(){
  M5.update();auto detail=M5.Touch.getDetail();bool pressed=M5.Touch.getCount()&&detail.isPressed();
  int x=detail.x-(M5.Display.width()-466)/2,y=detail.y-(M5.Display.height()-466)/2;auto raw=M5.Touch.getTouchPointRaw();
  portENTER_CRITICAL(&inputMux);bool changed=pressed!=latestTouch.pressed;latestTouch={pressed,x,y};
  if(pressed){++touchSamples;lastTouchX=detail.x;lastTouchY=detail.y;lastRawX=raw.x;lastRawY=raw.y;if(changed)++touchPresses;}
  portEXIT_CRITICAL(&inputMux);
  if(changed){TouchEvent event{pressed,x,y};if(xQueueSend(touchEvents,&event,0)!=pdTRUE){TouchEvent old;xQueueReceive(touchEvents,&old,0);xQueueSend(touchEvents,&event,0);++inputDrops;}}
  for(int i=0;i<2;++i)if(i?M5.BtnB.wasPressed():M5.BtnA.wasPressed()){KeyEvent event{i==0,stopwatchNow()};if(xQueueSend(keyEvents,&event,0)!=pdTRUE)++inputDrops;}
}
static void inputTask(void*){TickType_t wake=xTaskGetTickCount(),period=pdMS_TO_TICKS(10);for(;;){pollInputs();vTaskDelayUntil(&wake,period);if(xTaskGetTickCount()-wake>period)wake=xTaskGetTickCount();}}
void stopwatchTouchStatus(){
  portENTER_CRITICAL(&inputMux);uint32_t samples=touchSamples,presses=touchPresses;int x=lastTouchX,y=lastTouchY,rawX=lastRawX,rawY=lastRawY;portEXIT_CRITICAL(&inputMux);
  Serial.printf("TOUCH_STATUS enabled=%d irq=%d samples=%lu presses=%lu lvgl=%lu xy=%d,%d raw=%d,%d drops=%lu\n",M5.Touch.isEnabled(),digitalRead(13),samples,presses,lvglPresses,x,y,rawX,rawY,inputDrops.load());Serial.println();
}
bool alertSound=true,alertVibration=true;
void boardBegin(){
  auto cfg=M5.config();cfg.internal_spk=true;cfg.internal_mic=false;cfg.internal_imu=false;
  cfg.fallback_board=m5::board_t::board_M5StopWatch;M5.begin(cfg);
  // CO5300's M5 driver exposes a 468x468 transfer canvas around the 466px panel.
  if(M5.getBoard()!=m5::board_t::board_M5StopWatch || M5.Display.width()<466 || M5.Display.width()>468){
    Serial.printf("FATAL: StopWatch display not identified board=%d expected=%d size=%dx%d gfx=%d\n",int(M5.getBoard()),int(m5::board_t::board_M5StopWatch),M5.Display.width(),M5.Display.height(),int(M5.Display.getBoard()));while(true)delay(100);
  }
  // This CST820B reports native 466px coordinates. Explicitly map them into
  // the centered transfer canvas; the library's generic touch scaling can
  // otherwise stretch the outer controls beyond their visible positions.
  float touchMap[6]={1,0,float((M5.Display.width()-466)/2),0,1,float((M5.Display.height()-466)/2)};
  M5.Display.panel()->setCalibrateAffine(touchMap);
  M5.Display.setBrightness(150);M5.Display.fillScreen(TFT_BLACK);
  Serial.printf("BOARD StopWatch display=%dx%d touch=%d keys=2,1\n",M5.Display.width(),M5.Display.height(),M5.Touch.isEnabled());
  touchEvents=xQueueCreate(16,sizeof(TouchEvent));keyEvents=xQueueCreate(8,sizeof(KeyEvent));
  if(!touchEvents||!keyEvents){Serial.println("FATAL input queues");abort();}
  inputTaskReady=xTaskCreatePinnedToCore(inputTask,"dd-input",4096,nullptr,2,nullptr,ARDUINO_RUNNING_CORE)==pdPASS;
}
void set_brightness(uint8_t b){M5.Display.setBrightness((unsigned)b*255/100);}
void testTouch(int x,int y,bool pressed){injected=true;injectedX=x;injectedY=y;injectedPress=pressed;}
static void readTouch(lv_indev_drv_t*,lv_indev_data_t* d){
  bool pressed=false,hasPoint=false;int x=0,y=0;
  if(injected){x=injectedX;y=injectedY;pressed=injectedPress;hasPoint=true;if(!pressed)injected=false;}
  else {TouchEvent event;hasPoint=xQueueReceive(touchEvents,&event,0)==pdTRUE;if(!hasPoint){portENTER_CRITICAL(&inputMux);event=latestTouch;portEXIT_CRITICAL(&inputMux);}pressed=event.pressed;x=event.x;y=event.y;hasPoint=hasPoint||pressed;d->continue_reading=uxQueueMessagesWaiting(touchEvents)>0;}
  d->state=pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
  if(hasPoint){d->point.x=constrain(x,0,465);d->point.y=constrain(y,0,465);}
  static bool was=false;if(pressed&&!was)++lvglPresses;was=pressed;
  if(!uiTouch(pressed,x,y))d->state=LV_INDEV_STATE_REL;
}
void scr_lvgl_init(){
  // M5GFX pushImage is synchronous and accepts PSRAM. A persistent full frame
  // avoids re-running the complete radar draw callback for twenty tiny tiles.
  lv_init();auto* buf=(lv_color_t*)heap_caps_malloc(466*466*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!buf){Serial.println("FATAL display buffer");abort();}
  lv_disp_draw_buf_init(&drawBuffer,buf,nullptr,466*466);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=466;displayDriver.ver_res=466;displayDriver.draw_buf=&drawBuffer;
  displayDriver.flush_cb=[](lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* pixels){
    // LV_COLOR_16_SWAP=1: LVGL already supplies big-endian RGB565 bytes.
    // The typed overload prevents a second byte swap in M5GFX.
    M5.Display.startWrite();
    M5.Display.pushImage(a->x1+(M5.Display.width()-466)/2,a->y1+(M5.Display.height()-466)/2,a->x2-a->x1+1,a->y2-a->y1+1,reinterpret_cast<const lgfx::swap565_t*>(pixels));
    M5.Display.endWrite();lv_disp_flush_ready(d);
  };
  lv_disp_drv_register(&displayDriver);
  static lv_indev_drv_t touch;lv_indev_drv_init(&touch);touch.type=LV_INDEV_TYPE_POINTER;touch.read_cb=readTouch;lv_indev_drv_register(&touch);
}
void boardLoop(){
  if(!inputTaskReady)pollInputs();
  KeyEvent key;for(int n=0;n<8&&xQueueReceive(keyEvents,&key,0)==pdTRUE;++n)if(!uiStopwatchKey(key.start,key.at))uiKey(key.start);
  if(vibrationUntil && int32_t(millis()-vibrationUntil)>=0){M5.Power.setVibration(0);vibrationUntil=0;}
  if(alertTotal!=lastAlert){lastAlert=alertTotal;if(!detector.quiet && millis()-lastFeedback>10000){lastFeedback=millis();
    if(alertVibration){M5.Power.setVibration(100);vibrationUntil=millis()+80;}
  }}
}

int batteryPercent(){static uint32_t at=0;static int value=-1;if(!at || millis()-at>5000){at=millis();value=M5.Power.getBatteryLevel();}return value;}
bool batteryCharging(){return M5.Power.isCharging();}

void boardSensorsBegin(){rtcAvailable=M5.Rtc.isEnabled();}
bool boardReadRtc(uint32_t& epoch){
  if(!rtcAvailable||M5.Rtc.getVoltLow())return false;
  m5::rtc_datetime_t d;if(!M5.Rtc.getDateTime(&d))return false;
  dragon::Calendar date{d.date.year,d.date.month,d.date.date,d.time.hours,d.time.minutes,d.time.seconds};
  epoch=dragon::dateEpoch(date);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
  time_t value=epoch;struct tm t{};gmtime_r(&value,&t);m5::rtc_datetime_t d(t);
  if(!rtcAvailable||!M5.Rtc.getRtcInstancePtr()->setDateTime(&d.date,&d.time))return false;
  // Clear the latched low-voltage indication after an explicit time setting.
  if(!M5.In_I2C.bitOff(0x32,0x1D,0x80,400000))return false;
  uint32_t readback=0;return boardReadRtc(readback)&&readback>=epoch&&readback-epoch<=1;
}
