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
  // Read RX8130 validity ourselves before importing UTC. M5Unified's automatic
  // clock import ignores VLF and its getVoltLow() reports VBLF instead.
  cfg.internal_rtc=false;cfg.external_rtc=false;
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

static bool rtcRead(uint8_t reg,uint8_t* data,size_t count){return M5.In_I2C.readRegister(0x32,reg,data,count,400000);}
static bool rtcWrite(uint8_t reg,const uint8_t* data,size_t count){return M5.In_I2C.writeRegister(0x32,reg,data,count,400000);}
static bool rtcPowerHold(){
  // C152's RX8130 VDD is 3V3_L1; VBAT has only a 10 uF capacitor.
  // M5PM1 HOLD_CFG.LDO (0x07 bit 5) keeps L1 alive on button shutdown.
  // Preserve all other rail/GPIO settings; verify both enable and hold.
  uint8_t power=0,hold=0;
  if(!M5.In_I2C.readRegister(0x6e,0x06,&power,1,100000)||
     !M5.In_I2C.readRegister(0x6e,0x07,&hold,1,100000))return false;
  if(!(power&4)&&!M5.In_I2C.writeRegister8(0x6e,0x06,power|4,100000))return false;
  if(!(hold&0x20)&&!M5.In_I2C.writeRegister8(0x6e,0x07,hold|0x20,100000))return false;
  return M5.In_I2C.readRegister(0x6e,0x06,&power,1,100000)&&
    M5.In_I2C.readRegister(0x6e,0x07,&hold,1,100000)&&(power&4)&&(hold&0x20);
}
static bool rtcBackupBegin(){
  uint8_t control=0;
  if(!rtcRead(0x1f,&control,1))return false;
  // Initialize switchover and charge the board's backup capacitor. Do not
  // reset calendar/control/flags: a stopped clock must still require a sync.
  if((control&0x30)!=0x30){control|=0x30;if(!rtcWrite(0x1f,&control,1))return false;}
  return rtcRead(0x1f,&control,1)&&(control&0x30)==0x30;
}
void boardSensorsBegin(){
  bool held=false;rtcAvailable=false;
  for(unsigned attempt=0;attempt<3;++attempt){
    held=rtcPowerHold();uint8_t flags=0;
    rtcAvailable=rtcRead(0x1d,&flags,1);
    if(held&&rtcAvailable&&rtcBackupBegin())break;
    delay(10);
  }
  if(!held)Serial.println("RTC_WARNING power hold unavailable; off-time retention unverified");
}
static int decodeBcd(uint8_t v){return (v&15)<=9&&(v>>4)<=9?(v>>4)*10+(v&15):-1;}
bool boardReadRtc(uint32_t& epoch){
  uint8_t state[2]{},b[7]{};
  // VLF (bit 1) means the oscillator stopped. VBLF (bit 7) is only a
  // backup-voltage warning and must not discard an otherwise valid clock.
  if(!rtcAvailable||!rtcRead(0x1d,state,2)||(state[0]&2)||(state[1]&0xc0)||!rtcRead(0x10,b,7))return false;
  if(!b[3]||(b[3]&0x80)||(b[3]&(b[3]-1)))return false;
  dragon::Calendar date{2000+decodeBcd(b[6]),decodeBcd(b[5]&31),decodeBcd(b[4]&63),decodeBcd(b[2]&63),decodeBcd(b[1]&127),decodeBcd(b[0]&127)};
  epoch=dragon::dateEpoch(date);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
  if(!rtcAvailable||!dragon::validEpoch(epoch)||!rtcPowerHold()||!rtcBackupBegin())return false;
  time_t value=epoch;struct tm t{};gmtime_r(&value,&t);
  auto bcd=[](int v){return uint8_t((v/10)*16+v%10);};uint8_t control=0;
  if(!rtcRead(0x1e,&control,1))return false;
  control=(control&~0x80)|0x40;if(!rtcWrite(0x1e,&control,1))return false;
  uint8_t b[]={bcd(t.tm_sec),bcd(t.tm_min),bcd(t.tm_hour),uint8_t(1u<<t.tm_wday),bcd(t.tm_mday),bcd(t.tm_mon+1),bcd(t.tm_year%100)};
  bool written=rtcWrite(0x10,b,7);
  // Flags are write-zero-to-clear. Clear VLF/VBLF only after a complete
  // explicit date write; preserve any concurrently raised interrupt flags.
  uint8_t flags=0x3d;bool cleared=written&&rtcWrite(0x1d,&flags,1);
  control&=~0x40;bool restarted=rtcWrite(0x1e,&control,1);
  uint32_t readback=0;return written&&cleared&&restarted&&boardReadRtc(readback)&&readback>=epoch&&readback-epoch<=1;
}
void boardRtcDiagnostics(){
  uint8_t s[3]{},p[2]{};
  bool ok=rtcRead(0x1d,s,3),power=M5.In_I2C.readRegister(0x6e,0x06,p,2,100000);
  Serial.printf("CLOCK_DIAGNOSTICS chip=RX8130 io=%u flags=%02x control=%02x backup=%02x power_io=%u ldo=%u hold=%u\n",ok,s[0],s[1],s[2],power,(p[0]&4)!=0,(p[1]&0x20)!=0);
}
