// Seeed XIAO ESP32-S3 + Round Display: GC9A01, CHSC6X, PCF8563/BM8563 RTC and SPI microSD.
// Pin/protocol reference: Seeed_Arduino_RoundDisplay (MIT); see CREDITS.md.
#include "app.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <time.h>

static TFT_eSPI lcd;
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;
static SemaphoreHandle_t i2cMutex=nullptr;
struct TouchEvent {bool pressed;int16_t x,y;};
struct KeyEvent {uint64_t at;};
static QueueHandle_t touchQueue=nullptr,keyQueue=nullptr;
static portMUX_TYPE inputMux=portMUX_INITIALIZER_UNLOCKED;
static TouchEvent latestTouch{};
static bool taskReady=false;
static std::atomic<bool> touchReady{false};
static std::atomic<uint32_t> inputDrops{0},touchPresses{0},touchErrors{0};
extern bool uiTouch(bool,int,int);

static void pollInputs(){
  TouchEvent next;portENTER_CRITICAL(&inputMux);next=latestTouch;portEXIT_CRITICAL(&inputMux);
  static uint32_t lastValidTouch=0;
  bool was=next.pressed;next.pressed=false;
  // A short bus contention/read failure must not synthesize a release and a
  // second press. Bound retention so a missing controller cannot hold a tap.
  if(digitalRead(XIAO_TOUCH_INT)==LOW){
    next.pressed=was&&uint32_t(millis()-lastValidTouch)<60;
    if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(5))==pdTRUE){
    uint8_t data[5]{};size_t received=Wire.requestFrom(uint8_t(0x2e),uint8_t(5));
    if(received==5){Wire.readBytes(data,5);touchReady=true;next.pressed=false;if(data[0]==1&&data[2]<240&&data[4]<240){next={true,int16_t(data[2]),int16_t(data[4])};lastValidTouch=millis();}}
    else {while(Wire.available())Wire.read();++touchErrors;}
    xSemaphoreGive(i2cMutex);
    }
  }
  if(next.pressed&&!was)++touchPresses;
  portENTER_CRITICAL(&inputMux);latestTouch=next;portEXIT_CRITICAL(&inputMux);
  if(next.pressed!=was&&xQueueSend(touchQueue,&next,0)!=pdTRUE){TouchEvent old;xQueueReceive(touchQueue,&old,0);xQueueSend(touchQueue,&next,0);++inputDrops;}
  static bool raw=false,stable=false;static uint32_t changed=0;
  bool pressed=digitalRead(BTN_PIN)==LOW;uint32_t now=millis();
  if(pressed!=raw){raw=pressed;changed=now;}
  if(raw!=stable&&now-changed>=25){stable=raw;if(stable){KeyEvent event{stopwatchNow()};if(xQueueSend(keyQueue,&event,0)!=pdTRUE)++inputDrops;}}
}
static void inputTask(void*){TickType_t wake=xTaskGetTickCount(),period=pdMS_TO_TICKS(10);for(;;){pollInputs();vTaskDelayUntil(&wake,period);if(xTaskGetTickCount()-wake>period)wake=xTaskGetTickCount();}}
static void readTouch(lv_indev_drv_t*,lv_indev_data_t* data){
  TouchEvent event;if(xQueueReceive(touchQueue,&event,0)!=pdTRUE){portENTER_CRITICAL(&inputMux);event=latestTouch;portEXIT_CRITICAL(&inputMux);}
  data->point.x=event.x;data->point.y=event.y;data->state=event.pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
  data->continue_reading=uxQueueMessagesWaiting(touchQueue)>0;
  if(!uiTouch(event.pressed,event.x,event.y))data->state=LV_INDEV_STATE_REL;
}
void boardBegin(){
  pinMode(BTN_PIN,INPUT_PULLUP);pinMode(XIAO_LCD_CS,OUTPUT);digitalWrite(XIAO_LCD_CS,HIGH);
  pinMode(XIAO_SD_CS,OUTPUT);digitalWrite(XIAO_SD_CS,HIGH);pinMode(XIAO_TOUCH_INT,INPUT_PULLUP);
  i2cMutex=xSemaphoreCreateMutex();touchQueue=xQueueCreate(16,sizeof(TouchEvent));keyQueue=xQueueCreate(8,sizeof(KeyEvent));
  if(!i2cMutex||!touchQueue||!keyQueue){Serial.println("FATAL XIAO input allocation");abort();}
  Wire.begin(XIAO_SDA,XIAO_SCL);Wire.setClock(400000);Wire.setTimeOut(10);
  Wire.beginTransmission(0x2e);touchReady=Wire.endTransmission()==0;
}
void set_brightness(uint8_t value){ledcWrite(XIAO_BACKLIGHT,unsigned(constrain(value,0,100))*255/100);}
void scr_lvgl_init(){
  lcd.begin();lcd.setRotation(0);lcd.fillScreen(TFT_BLACK);
  // The Round Display's BL switch must connect D6 for PWM dimming to work.
  ledcAttach(XIAO_BACKLIGHT,5000,8);set_brightness(100);
  lv_init();auto* pixels=(lv_color_t*)heap_caps_malloc(240*240*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("FATAL XIAO frame allocation");abort();}
  lv_disp_draw_buf_init(&drawBuffer,pixels,nullptr,240*240);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=240;displayDriver.ver_res=240;displayDriver.draw_buf=&drawBuffer;
  displayDriver.flush_cb=[](lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels){
    // One synchronous SPI transaction: release the shared bus before SD I/O.
    lcd.startWrite();lcd.setAddrWindow(area->x1,area->y1,area->x2-area->x1+1,area->y2-area->y1+1);
    lcd.pushColors(reinterpret_cast<uint16_t*>(pixels),size_t(area->x2-area->x1+1)*(area->y2-area->y1+1),false);
    lcd.endWrite();lv_disp_flush_ready(driver);
  };
  lv_disp_drv_register(&displayDriver);
  static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=readTouch;lv_indev_drv_register(&input);
  taskReady=xTaskCreatePinnedToCore(inputTask,"xiao-input",3072,nullptr,2,nullptr,ARDUINO_RUNNING_CORE)==pdPASS;
  Serial.printf("BOARD XIAO GC9A01=240x240 CHSC6X=%u buffered=%u\n",touchReady.load(),taskReady);
}
bool boardMountCard(){
  // Reuse the exact TFT SPI instance and its transaction mutex, no second bus.
  return SD.begin(XIAO_SD_CS,TFT_eSPI::getSPIinstance(),10000000,"/sdcard",8,false);
}
void boardLoop(){
  if(!taskReady)pollInputs();KeyEvent event;
  for(int n=0;n<8&&xQueueReceive(keyQueue,&event,0)==pdTRUE;++n)if(!uiStopwatchKey(true,event.at))uiShow("menu");
}
void xiaoTouchStatus(){
  TouchEvent event;portENTER_CRITICAL(&inputMux);event=latestTouch;portEXIT_CRITICAL(&inputMux);
  Serial.printf("TOUCH_STATUS ready=%u pressed=%u xy=%d,%d presses=%lu errors=%lu drops=%lu irq=%d\n",touchReady.load(),event.pressed,event.x,event.y,touchPresses.load(),touchErrors.load(),inputDrops.load(),digitalRead(XIAO_TOUCH_INT));Serial.println();
}
// Voltage-only charge estimation cannot establish presence/charging on this
// board. Keep the shared UI's brightness percentage instead of a false gauge.
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
  uint8_t control=0,b[7]{};if(!rtcAvailable||!rtcRead(0,&control,1)||(control&0xa8)||!rtcRead(2,b,7)||(b[0]&0x80))return false;
  dragon::Calendar date{2000+decodeBcd(b[6]),decodeBcd(b[5]&31),decodeBcd(b[3]&63),decodeBcd(b[2]&63),decodeBcd(b[1]&127),decodeBcd(b[0]&127)};
  epoch=dragon::dateEpoch(date);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
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
