#ifdef BOARD_CROWPANEL_21
#include "app.h"
#include "rotary_input.h"
#include "rotary_ui.h"
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>
#include <hal/gpio_ll.h>
#include <soc/gpio_struct.h>

extern bool uiTouch(bool,int,int);
namespace {
// Elecrow Board21.h, hardware revision V1.0; see docs/ELECROW-ROTARY-21.md.
Arduino_SWSPI initBus(GFX_NOT_DEFINED,16,2,1,GFX_NOT_DEFINED);
Arduino_ESP32RGBPanel rgb(40,7,15,41,46,3,8,18,17,14,13,12,11,10,9,
  5,45,48,47,21,1,10,4,20,1,10,4,20,0,12000000,false,0,0,480*20);
Arduino_RGB_Display panel(480,480,&rgb,0,true,&initBus,GFX_NOT_DEFINED,
  st7701_type5_init_operations,sizeof(st7701_type5_init_operations));
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
uint8_t expander=0xff;
bool touchReady=false,expanderReady=false;
dragon::RotaryButton push;
portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED;
dragon::RotaryDecoder rotary;
volatile int detents=0;
volatile uint32_t edges=0;
uint32_t clicks=0,holds=0,touchPresses=0,touchSamples=0,i2cErrors=0;
int lastX=0,lastY=0,position=0;
bool injected=false,injectedPress=false;
int injectedX=0,injectedY=0;

bool writeExpander(uint8_t value){
  Wire.beginTransmission(CROW_EXPANDER_ADDR);Wire.write(value);
  if(Wire.endTransmission()!=0){++i2cErrors;return false;}expander=value;return true;
}
bool expanderPin(unsigned pin,bool high){
  return writeExpander(high?expander|(1u<<pin):expander&~(1u<<pin));
}
bool readTouchRegs(uint8_t reg,uint8_t* bytes,size_t length){
  Wire.beginTransmission(CROW_TOUCH_ADDR);Wire.write(reg);
  if(Wire.endTransmission(false)!=0||Wire.requestFrom(uint8_t(CROW_TOUCH_ADDR),length)!=length){++i2cErrors;return false;}
  for(size_t i=0;i<length;++i)bytes[i]=Wire.read();return true;
}
void ARDUINO_ISR_ATTR encoderChanged(){
  uint8_t state=(gpio_ll_get_level(&GPIO,CROW_ENCODER_A)<<1)|gpio_ll_get_level(&GPIO,CROW_ENCODER_B);
  portENTER_CRITICAL_ISR(&encoderMux);
  ++edges;int step=rotary.update(state);
  if((step>0&&detents<48)||(step<0&&detents>-48))detents+=step;
  portEXIT_CRITICAL_ISR(&encoderMux);
}
void readTouch(lv_indev_drv_t*,lv_indev_data_t* data){
  bool pressed=false;int x=lastX,y=lastY;
  if(injected){pressed=injectedPress;x=injectedX;y=injectedY;if(!pressed)injected=false;}
  else if(touchReady){
    uint8_t point[5]{};
    if(readTouchRegs(0x02,point,sizeof(point))&&point[0]==1){
      x=((point[1]&15)<<8)|point[2];y=((point[3]&15)<<8)|point[4];
      pressed=x<480&&y<480;
    }
  }
  static bool was=false;
  if(pressed){++touchSamples;if(!was)++touchPresses;lastX=x;lastY=y;}was=pressed;
  data->point.x=constrain(x,0,479);data->point.y=constrain(y,0,479);
  data->state=pressed&&uiTouch(pressed,x,y)?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
  if(!pressed)uiTouch(false,x,y);
}
}
bool alertSound=false,alertVibration=false;
void boardBegin(){
  pinMode(CROW_BACKLIGHT,OUTPUT);digitalWrite(CROW_BACKLIGHT,LOW);
  Wire.begin(CROW_SDA,CROW_SCL,400000);Wire.setTimeOut(20);
  expanderReady=writeExpander(0xff);
  if(!expanderReady){Serial.println("FATAL CrowPanel expander 0x21 missing");while(true)delay(100);}
  expanderPin(3,true);delay(100);
  expanderPin(4,false);delay(120);expanderPin(4,true);delay(120);
  expanderPin(0,false);delay(120);expanderPin(0,true);delay(120);
  expanderPin(2,true);delay(120);
  if(!panel.begin()){Serial.println("FATAL CrowPanel RGB panel");while(true)delay(100);}
  initBus.beginWrite();initBus.writeCommand(0x36);initBus.write(0x08);initBus.endWrite();
  panel.fillScreen(0);
  uint8_t id=0;touchReady=readTouchRegs(0xaa,&id,1)&&id==0x11;
  ledcAttach(CROW_BACKLIGHT,5000,8);ledcWrite(CROW_BACKLIGHT,204);
  pinMode(CROW_ENCODER_A,INPUT_PULLUP);pinMode(CROW_ENCODER_B,INPUT_PULLUP);
  rotary.reset((digitalRead(CROW_ENCODER_A)<<1)|digitalRead(CROW_ENCODER_B));
  attachInterrupt(CROW_ENCODER_A,encoderChanged,CHANGE);attachInterrupt(CROW_ENCODER_B,encoderChanged,CHANGE);
  Serial.printf("BOARD CrowPanel display=480x480 touch=%u chip=%02x expander=%u encoder=42,4 push=PCF8574.P5\n",touchReady,id,expanderReady);
}
void set_brightness(uint8_t level){ledcWrite(CROW_BACKLIGHT,unsigned(level)*255/100);}
void testTouch(int x,int y,bool pressed){injected=true;injectedX=x;injectedY=y;injectedPress=pressed;}
void scr_lvgl_init(){
  lv_init();
  auto* pixels=(lv_color_t*)heap_caps_malloc(480*480*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("FATAL CrowPanel LVGL buffer");abort();}
  lv_disp_draw_buf_init(&drawBuffer,pixels,nullptr,480*480);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=480;displayDriver.ver_res=480;displayDriver.draw_buf=&drawBuffer;
  displayDriver.flush_cb=[](lv_disp_drv_t* drv,const lv_area_t* area,lv_color_t* data){
    panel.draw16bitBeRGBBitmap(area->x1,area->y1,(uint16_t*)data,area->x2-area->x1+1,area->y2-area->y1+1);
    lv_disp_flush_ready(drv);
  };
  lv_disp_drv_register(&displayDriver);
  static lv_indev_drv_t touch;lv_indev_drv_init(&touch);touch.type=LV_INDEV_TYPE_POINTER;touch.read_cb=readTouch;lv_indev_drv_register(&touch);
  rotaryUiBegin();
}
void boardLoop(){
  portENTER_CRITICAL(&encoderMux);int steps=detents;detents=0;portEXIT_CRITICAL(&encoderMux);
  position+=steps;bool click=false,back=false;
  static uint32_t polled=0;
  if(millis()-polled>=10){
    polled=millis();
    if(Wire.requestFrom(uint8_t(CROW_EXPANDER_ADDR),size_t(1))==1){
      auto event=push.update(!(Wire.read()&(1u<<5)),polled);
      click=event==dragon::RotaryPress::Click;back=event==dragon::RotaryPress::Back;
      if(click)++clicks;if(back)++holds;
    }else ++i2cErrors;
  }
  rotaryUiInput(steps,click,back);
}
void crowInputStatus(){
  Serial.printf("ROTARY_STATUS position=%d edges=%lu clicks=%lu holds=%lu errors=%lu\n",position,edges,clicks,holds,i2cErrors);
  Serial.printf("TOUCH_STATUS enabled=%u presses=%lu samples=%lu xy=%d,%d\n",touchReady,touchPresses,touchSamples,lastX,lastY);
}
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}
void boardSensorsBegin(){rtcAvailable=false;}
bool boardReadRtc(uint32_t&){return false;}
bool boardWriteRtc(uint32_t){return false;}
#endif
