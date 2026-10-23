// M5Stack Dial / Dial v1.1: 8MB flash, no PSRAM.
#ifdef BOARD_M5DIAL
#include "app.h"
#include "dial_touch.h"
#include "rotary_input.h"
#include "lite_button.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <hal/gpio_ll.h>
#include <time.h>
#include <atomic>
static TFT_eSPI lcd;
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;
static SemaphoreHandle_t i2cMutex=nullptr;
static dragon::RotaryDecoder encoder;
static dragon::RotaryNavigation navigation;
static dragon::LiteButton button;
static portMUX_TYPE inputMux=portMUX_INITIALIZER_UNLOCKED;
static volatile int pendingSteps=0;
static volatile uint32_t encoderEdges=0;
static dragon::DialTouch touch;
static QueueHandle_t touchQueue=nullptr,keyQueue=nullptr;
static bool inputTaskReady=false;
static uint32_t touchAt=0,touchRetryAt=0,busRecoveryAt=0;
static unsigned touchFailures=0;
static std::atomic<uint32_t> touchErrors{0},touchPresses{0},inputDrops{0};
static std::atomic<bool> touchReady{false};
extern void dialRotate(int);
extern void liteButtonEvent(dragon::LitePress);
static void IRAM_ATTR encoderChanged(){
  const uint8_t state=(gpio_ll_get_level(&GPIO,DIAL_ENCODER_A)<<1)|gpio_ll_get_level(&GPIO,DIAL_ENCODER_B);
  portENTER_CRITICAL_ISR(&inputMux);++encoderEdges;
  int next=pendingSteps+encoder.update(state);pendingSteps=next<-48?-48:next>48?48:next;
  portEXIT_CRITICAL_ISR(&inputMux);
}
static void pollTouch(){
  const uint32_t now=millis();if(touchFailures>=8&&int32_t(now-touchRetryAt)<0)return;bool valid=false;dragon::DialTouch previous;
  portENTER_CRITICAL(&inputMux);previous=touch;portEXIT_CRITICAL(&inputMux);
  dragon::DialTouch next=previous;
  if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(5))==pdTRUE){
    if(touchFailures>=8&&uint32_t(now-busRecoveryAt)>=2000){Wire.end();Wire.begin(DIAL_SDA,DIAL_SCL,400000);Wire.setTimeOut(20);busRecoveryAt=now;}
    uint8_t b[5]{};Wire.beginTransmission(0x38);Wire.write(0x02);
    if(Wire.endTransmission(false)==0&&Wire.requestFrom(uint8_t(0x38),uint8_t(5))==5){
      Wire.readBytes(b,5);valid=dragon::decodeDialTouch(b,5,next);
    }else while(Wire.available())Wire.read();
    xSemaphoreGive(i2cMutex);
  }
  if(valid){touchReady=true;touchFailures=0;touchAt=now;if(next.pressed&&!previous.pressed)++touchPresses;}
  else{++touchErrors;++touchFailures;if(touchFailures>=8)touchRetryAt=now+200;if(uint32_t(now-touchAt)>=60){next.pressed=false;touchReady=false;}}
  portENTER_CRITICAL(&inputMux);touch=next;portEXIT_CRITICAL(&inputMux);
  if(next.pressed!=previous.pressed&&xQueueSend(touchQueue,&next,0)!=pdTRUE){dragon::DialTouch discarded;xQueueReceive(touchQueue,&discarded,0);xQueueSend(touchQueue,&next,0);++inputDrops;}
}
static void pollInputs(){pollTouch();auto event=button.update(digitalRead(DIAL_BUTTON)==LOW,millis());if(event!=dragon::LitePress::None&&xQueueSend(keyQueue,&event,0)!=pdTRUE)++inputDrops;}
static void inputTask(void*){TickType_t wake=xTaskGetTickCount(),period=pdMS_TO_TICKS(10);for(;;){pollInputs();vTaskDelayUntil(&wake,period);if(xTaskGetTickCount()-wake>period)wake=xTaskGetTickCount();}}
static void readTouch(lv_indev_drv_t*,lv_indev_data_t* data){
  dragon::DialTouch event;
  if(xQueueReceive(touchQueue,&event,0)!=pdTRUE){portENTER_CRITICAL(&inputMux);event=touch;portEXIT_CRITICAL(&inputMux);}
  data->point.x=event.x;data->point.y=event.y;data->state=event.pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
  data->continue_reading=uxQueueMessagesWaiting(touchQueue)>0;
}
void boardBegin(){
  pinMode(DIAL_HOLD,OUTPUT);digitalWrite(DIAL_HOLD,HIGH);
  pinMode(DIAL_BUTTON,INPUT_PULLUP);pinMode(DIAL_ENCODER_A,INPUT_PULLUP);pinMode(DIAL_ENCODER_B,INPUT_PULLUP);
  i2cMutex=xSemaphoreCreateMutex();touchQueue=xQueueCreate(16,sizeof(dragon::DialTouch));keyQueue=xQueueCreate(8,sizeof(dragon::LitePress));if(!i2cMutex||!touchQueue||!keyQueue){Serial.println("FATAL Dial I2C mutex");abort();}
  encoder.reset((digitalRead(DIAL_ENCODER_A)<<1)|digitalRead(DIAL_ENCODER_B));
  attachInterrupt(digitalPinToInterrupt(DIAL_ENCODER_A),encoderChanged,CHANGE);
  attachInterrupt(digitalPinToInterrupt(DIAL_ENCODER_B),encoderChanged,CHANGE);
}
void boardLoop(){
  int steps;portENTER_CRITICAL(&inputMux);steps=pendingSteps;pendingSteps=0;portEXIT_CRITICAL(&inputMux);
  const uint32_t now=millis();navigation.push(steps,now);if(int step=navigation.take(now))dialRotate(step);
  if(!inputTaskReady)pollInputs();dragon::LitePress event;
  for(int n=0;n<8&&xQueueReceive(keyQueue,&event,0)==pdTRUE;++n)liteButtonEvent(event);
  static uint32_t clockCheckAt=0;
  if(!wallTime()&&uint32_t(now-clockCheckAt)>=2000){clockCheckAt=now;uint32_t epoch=0;if(boardReadRtc(epoch))setWallTime(epoch);}
}
void dialInputStatus(){
  dragon::DialTouch event;portENTER_CRITICAL(&inputMux);event=touch;portEXIT_CRITICAL(&inputMux);
  Serial.printf("TOUCH_STATUS ready=%u pressed=%u xy=%d,%d presses=%lu errors=%lu drops=%lu buffered=%u\n",touchReady.load(),event.pressed,event.x,event.y,touchPresses.load(),touchErrors.load(),inputDrops.load(),inputTaskReady);
  Serial.printf("ROTARY_STATUS edges=%lu button=%u hold=%u\n",uint32_t(encoderEdges),digitalRead(DIAL_BUTTON)==LOW,digitalRead(DIAL_HOLD));
}
void set_brightness(uint8_t value){ledcWrite(DIAL_BACKLIGHT,unsigned(constrain(value,0,100))*255/100);}
void scr_lvgl_init(){
  lcd.begin();lcd.setRotation(0);lcd.fillScreen(TFT_BLACK);ledcAttach(DIAL_BACKLIGHT,5000,8);set_brightness(100);
  // LCD reset also resets the RFID device on this shared I2C bus.
  Wire.begin(DIAL_SDA,DIAL_SCL,400000);Wire.setTimeOut(20);
  Wire.beginTransmission(0x38);touchReady=Wire.endTransmission()==0;
  lv_init();auto* pixels=(lv_color_t*)heap_caps_malloc(240*24*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("FATAL Dial display tile");abort();}
  lv_disp_draw_buf_init(&drawBuffer,pixels,nullptr,240*24);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=240;displayDriver.ver_res=240;displayDriver.draw_buf=&drawBuffer;
  displayDriver.flush_cb=[](lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels){
    lcd.startWrite();lcd.setAddrWindow(area->x1,area->y1,area->x2-area->x1+1,area->y2-area->y1+1);
    lcd.pushColors(reinterpret_cast<uint16_t*>(pixels),size_t(area->x2-area->x1+1)*(area->y2-area->y1+1),false);
    lcd.endWrite();lv_disp_flush_ready(driver);
  };
  lv_disp_drv_register(&displayDriver);
  static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=readTouch;lv_indev_drv_register(&input);
  inputTaskReady=xTaskCreatePinnedToCore(inputTask,"dial-touch",3072,nullptr,2,nullptr,ARDUINO_RUNNING_CORE)==pdPASS;
  Serial.printf("BOARD M5DIAL GC9A01=240x240 FT3267=%u PSRAM=none\n",touchReady.load());
}
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}

static bool rtcRead(uint8_t reg,uint8_t* data,size_t size){
  if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  Wire.beginTransmission(0x51);Wire.write(reg);bool ok=Wire.endTransmission(false)==0;
  if(ok)ok=Wire.requestFrom(uint8_t(0x51),uint8_t(size))==size;
  if(ok)for(size_t i=0;i<size;++i)data[i]=Wire.read();else while(Wire.available())Wire.read();
  xSemaphoreGive(i2cMutex);return ok;
}
static bool rtcWrite(uint8_t reg,const uint8_t* data,size_t size){
  if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  Wire.beginTransmission(0x51);Wire.write(reg);Wire.write(data,size);bool ok=Wire.endTransmission()==0;xSemaphoreGive(i2cMutex);return ok;
}
void boardSensorsBegin(){
  uint8_t control=0;rtcAvailable=false;
  for(unsigned attempt=0;attempt<3&&!rtcAvailable;++attempt){rtcAvailable=rtcRead(0,&control,1);if(!rtcAvailable)delay(10);}
}
static int decodeBcd(uint8_t value){return (value&15)<=9&&(value>>4)<=9?(value>>4)*10+(value&15):-1;}
bool boardReadRtc(uint32_t& epoch){
  if(!rtcAvailable)boardSensorsBegin();
  uint8_t control=0,b[7]{};if(!rtcAvailable||!rtcRead(0,&control,1)||(control&0xa8)||!rtcRead(2,b,7)||(b[0]&0x80))return false;
  dragon::Calendar date{2000+decodeBcd(b[6]),decodeBcd(b[5]&31),decodeBcd(b[3]&63),decodeBcd(b[2]&63),decodeBcd(b[1]&127),decodeBcd(b[0]&127)};
  epoch=dragon::dateEpoch(date);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
  if(!rtcAvailable)boardSensorsBegin();
  if(!rtcAvailable||!dragon::validEpoch(epoch))return false;time_t value=epoch;struct tm date{};gmtime_r(&value,&date);
  auto bcd=[](int v){return uint8_t((v/10)*16+v%10);};uint8_t control=0;
  if(!rtcRead(0,&control,1))return false;
  // Normal mode: TEST1/TESTC and reserved bits zero; stop only for the write.
  control=0x20;if(!rtcWrite(0,&control,1))return false;
  uint8_t b[]={bcd(date.tm_sec),bcd(date.tm_min),bcd(date.tm_hour),bcd(date.tm_mday),uint8_t(date.tm_wday),bcd(date.tm_mon+1),bcd(date.tm_year%100)};
  bool written=rtcWrite(2,b,7);control&=~0x20;bool restarted=rtcWrite(0,&control,1);
  uint32_t readback=0;return written&&restarted&&boardReadRtc(readback)&&readback>=epoch&&readback-epoch<=1;
}
void boardRtcDiagnostics(){
  uint8_t control=0,seconds=0;bool ok=rtcRead(0,&control,1)&&rtcRead(2,&seconds,1);
  Serial.printf("CLOCK_DIAGNOSTICS chip=BM8563 io=%u control=%02x lost=%u\n",ok,control,(seconds&0x80)!=0);
}

#endif
