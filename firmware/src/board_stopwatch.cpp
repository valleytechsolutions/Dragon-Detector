#include "app.h"
#include <M5Unified.h>
#include <lvgl.h>
#include <Wire.h>

extern void uiTouch(bool,int,int);
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;
static bool injected=false,injectedPress=false;
static int injectedX=0,injectedY=0;
static uint32_t vibrationUntil=0,lastFeedback=0,lastAlert=0;
bool alertSound=false,alertVibration=true;
void boardBegin(){
  Wire.begin(47,48,100000);Serial.print("I2C before M5:");for(int a=8;a<120;++a){Wire.beginTransmission(a);if(Wire.endTransmission()==0)Serial.printf(" %02x",a);}Serial.println();Wire.end();
  auto cfg=M5.config();cfg.internal_spk=true;cfg.internal_mic=false;
  cfg.fallback_board=m5::board_t::board_M5StopWatch;M5.begin(cfg);
  // CO5300's M5 driver exposes a 468x468 transfer canvas around the 466px panel.
  if(M5.getBoard()!=m5::board_t::board_M5StopWatch || M5.Display.width()<466 || M5.Display.width()>468){
    Serial.printf("FATAL: StopWatch display not identified board=%d expected=%d size=%dx%d gfx=%d\n",int(M5.getBoard()),int(m5::board_t::board_M5StopWatch),M5.Display.width(),M5.Display.height(),int(M5.Display.getBoard()));while(true)delay(100);
  }
  M5.Display.setBrightness(150);M5.Display.fillScreen(TFT_BLACK);
  Serial.printf("BOARD StopWatch display=%dx%d touch=%d keys=2,1\n",M5.Display.width(),M5.Display.height(),M5.Touch.isEnabled());
}
void set_brightness(uint8_t b){M5.Display.setBrightness((unsigned)b*255/100);}
void testTouch(int x,int y,bool pressed){injected=true;injectedX=x;injectedY=y;injectedPress=pressed;}
static void readTouch(lv_indev_drv_t*,lv_indev_data_t* d){
  bool pressed=false;int x=0,y=0;
  if(injected){x=injectedX;y=injectedY;pressed=injectedPress;if(!pressed)injected=false;}
  else {auto t=M5.Touch.getDetail();pressed=M5.Touch.getCount() && t.isPressed();x=t.x-(M5.Display.width()-466)/2;y=t.y-(M5.Display.height()-466)/2;}
  d->state=pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
  if(pressed){d->point.x=constrain(x,0,465);d->point.y=constrain(y,0,465);}
  uiTouch(pressed,x,y);
}
void scr_lvgl_init(){
  lv_init();auto* buf=(lv_color_t*)heap_caps_malloc(466*24*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(!buf){Serial.println("FATAL display buffer");abort();}
  lv_disp_draw_buf_init(&drawBuffer,buf,nullptr,466*24);
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
  M5.update();
  if(M5.BtnA.wasPressed()){Serial.println("KEY A radar");uiKey(true);}
  if(M5.BtnB.wasClicked()){Serial.println("KEY B navigation");uiKey(false);}
  if(vibrationUntil && int32_t(millis()-vibrationUntil)>=0){M5.Power.setVibration(0);vibrationUntil=0;}
  if(alertTotal!=lastAlert){lastAlert=alertTotal;if(millis()-lastFeedback>10000){lastFeedback=millis();
    if(alertVibration){M5.Power.setVibration(100);vibrationUntil=millis()+80;}
    if(alertSound)audioBeep();
  }}
}

int batteryPercent(){static uint32_t at=0;static int value=-1;if(!at || millis()-at>5000){at=millis();value=M5.Power.getBatteryLevel();}return value;}
bool batteryCharging(){return M5.Power.isCharging();}
void audioBeep(){if(M5.Speaker.begin())M5.Speaker.tone(1200,70);}
