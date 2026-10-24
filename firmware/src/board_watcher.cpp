// Original Dragon Detector integration; pin map from Seeed Watcher BSP (Apache-2.0).
// The display/touch drivers retain their separate Espressif Apache-2.0 notices.
#ifdef BOARD_WATCHER
#include "app.h"
#include "rotary_input.h"
#include "rotary_ui.h"
#include "wardrive.h"
#include <Wire.h>
#include <SPI.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <driver/spi_master.h>
#include <driver/i2c.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_io_interface.h>
#include <freertos/queue.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <esp_lcd_touch_spd2010.h>
#include <esp_log.h>
#include <hal/gpio_ll.h>
#include <soc/gpio_struct.h>
#include <time.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "recon.h"

extern bool uiTouch(bool,int,int);
namespace {
constexpr uint8_t Expander=0x21;
constexpr uint16_t Outputs=0xdf00, Inputs=0x20ff;
uint16_t outputs=0x8700;volatile uint16_t inputs=0xffff; // SD, LCD, system latch and battery ADC. AI/Grove off.
SemaphoreHandle_t busMutex=nullptr;
SPIClass cardSpi(FSPI); // SPI2; display exclusively owns SPI3.
esp_lcd_panel_handle_t panel=nullptr;
esp_lcd_panel_io_handle_t panelIo=nullptr;
esp_lcd_touch_handle_t touch=nullptr;
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
dragon::DetentDecoder decoder;
volatile bool wheelPressed=false;
volatile uint32_t wheelReleasedAt=0;
dragon::RotaryButton button;
portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED;
volatile int detents=0;
volatile uint32_t edges=0,flushes=0,lastEdgeAt=0;
struct InputEdge {bool down;uint16_t x,y;uint32_t at;};
struct ButtonEvent {dragon::RotaryPress value;uint32_t at;};
QueueHandle_t touchQueue=nullptr,buttonQueue=nullptr;
InputEdge physicalTouch{};
esp_lcd_panel_io_t touchTransport{};
uint32_t touchPolls=0;
TaskHandle_t inputTaskHandle=nullptr;
volatile uint32_t touchStatusRaw=0,touchPackets=0,rawContacts=0;
volatile uint16_t rawX=0,rawY=0,rawStrength=0;
bool readingStatus=false;
uint32_t clicks=0,holds=0,touchPresses=0,touchSamples=0,touchErrors=0,i2cErrors=0;
int position=0,lastX=0,lastY=0,battery=-1;
uint32_t batteryMv=0;
bool injected=false,injectedPress=false;
int injectedX=0,injectedY=0;
bool readRegs(uint8_t address,uint8_t reg,uint8_t* data,size_t count){
 if(xSemaphoreTake(busMutex,pdMS_TO_TICKS(20))!=pdTRUE)return false;
 Wire.beginTransmission(address);Wire.write(reg);bool ok=Wire.endTransmission(false)==0;
 if(ok)ok=Wire.requestFrom(address,count)==count;
 if(ok)for(size_t i=0;i<count;++i)data[i]=Wire.read();else {++i2cErrors;while(Wire.available())Wire.read();}
 xSemaphoreGive(busMutex);return ok;
}
bool writeRegs(uint8_t address,uint8_t reg,const uint8_t* data,size_t count){
 if(xSemaphoreTake(busMutex,pdMS_TO_TICKS(20))!=pdTRUE)return false;
 Wire.beginTransmission(address);Wire.write(reg);Wire.write(data,count);bool ok=Wire.endTransmission()==0;
 if(!ok)++i2cErrors;xSemaphoreGive(busMutex);return ok;
}
bool writeWord(uint8_t reg,uint16_t value){uint8_t b[]={uint8_t(value),uint8_t(value>>8)};return writeRegs(Expander,reg,b,2);}
bool sampleInputs(){uint8_t b[2];if(!readRegs(Expander,0,b,2))return false;inputs=b[0]|uint16_t(b[1])<<8;return true;}
void ARDUINO_ISR_ATTR encoderChanged(){
 uint8_t state=(gpio_ll_get_level(&GPIO,41)<<1)|gpio_ll_get_level(&GPIO,42);
 portENTER_CRITICAL_ISR(&encoderMux);++edges;lastEdgeAt=millis();int step=decoder.update(state);
 if(!wheelPressed&&uint32_t(millis()-wheelReleasedAt)>=100&&((step>0&&detents<48)||(step<0&&detents>-48)))detents+=step;
 portEXIT_CRITICAL_ISR(&encoderMux);
}
bool IRAM_ATTR flushComplete(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*){
 ++flushes;lv_disp_flush_ready(&displayDriver);return false;
}
// Use data-only transactions on Wire1 with a bounded timeout. No empty
// command-address write is inserted before the SPD2010 read transaction.
esp_err_t touchWrite(esp_lcd_panel_io_t*,int,const void* data,size_t size){
 if(!data||!size||size>128)return ESP_ERR_INVALID_ARG;
 readingStatus=size==2&&static_cast<const uint8_t*>(data)[0]==0x20&&static_cast<const uint8_t*>(data)[1]==0;
 Wire1.beginTransmission(0x53);Wire1.write(static_cast<const uint8_t*>(data),size);
 return Wire1.endTransmission(true)==0?ESP_OK:ESP_FAIL;
}
esp_err_t touchRead(esp_lcd_panel_io_t*,int,void* data,size_t size){
 if(!data||!size||size>128)return ESP_ERR_INVALID_ARG;
 if(Wire1.requestFrom(uint8_t(0x53),size)!=size){while(Wire1.available())Wire1.read();return ESP_FAIL;}
 auto* bytes=static_cast<uint8_t*>(data);for(size_t i=0;i<size;++i)bytes[i]=Wire1.read();
 if(readingStatus&&size==4){touchStatusRaw=uint32_t(bytes[0])|uint32_t(bytes[1])<<8|uint32_t(bytes[2])<<16|uint32_t(bytes[3])<<24;if(bytes[0]&1)++touchPackets;}
 return ESP_OK;
}
void inputTask(void*){
 dragon::TouchContactFilter contactFilter;
 bool wasDown=false;uint16_t contactX=0,contactY=0;TickType_t wake=xTaskGetTickCount();
 for(;;){
  uint32_t now=millis();
  if(sampleInputs()){
   bool pressed=!(inputs&(1u<<3));
   portENTER_CRITICAL(&encoderMux);
   if(pressed){detents=0;wheelReleasedAt=now;}
   wheelPressed=pressed;
   portEXIT_CRITICAL(&encoderMux);
   auto event=button.update(pressed,now);
   if(event!=dragon::RotaryPress::None){ButtonEvent e{event,now};xQueueSend(buttonQueue,&e,0);if(event==dragon::RotaryPress::Click)++clicks;else ++holds;}
  }
  uint16_t x=0,y=0,strength=0;uint8_t count=0;bool down=false;
  if(touch){++touchPolls;
   if(esp_lcd_touch_read_data(touch)==ESP_OK)down=esp_lcd_touch_get_coordinates(touch,&x,&y,&strength,&count,1)&&dragon::touchContactValid(count,x,y,strength,412);
   else ++touchErrors;
  }
  if(count){++rawContacts;rawX=x;rawY=y;rawStrength=strength;}
  if(down){contactX=x;contactY=y;}
  down=contactFilter.update(down,now);x=contactX;y=contactY;
  portENTER_CRITICAL(&encoderMux);if(!down){x=physicalTouch.x;y=physicalTouch.y;}physicalTouch={down,x,y,now};portEXIT_CRITICAL(&encoderMux);
  if(down){++touchSamples;if(!wasDown)++touchPresses;}
  if(down!=wasDown){InputEdge e{down,x,y,now};if(xQueueSend(touchQueue,&e,0)!=pdTRUE){xQueueReset(touchQueue);InputEdge released{false,x,y,now};xQueueSend(touchQueue,&released,0);if(down)xQueueSend(touchQueue,&e,0);}wasDown=down;}
  vTaskDelayUntil(&wake,pdMS_TO_TICKS(10));
 }
}
void readTouch(lv_indev_drv_t*,lv_indev_data_t* data){
 InputEdge point;
 portENTER_CRITICAL(&encoderMux);point=physicalTouch;portEXIT_CRITICAL(&encoderMux);
 InputEdge queued;while(xQueueReceive(touchQueue,&queued,0)==pdTRUE){if(uint32_t(millis()-queued.at)<=500){point=queued;break;}}
 if(injected){point={injectedPress,uint16_t(injectedX),uint16_t(injectedY),millis()};if(!injectedPress)injected=false;}
 rotaryUiTouch(point.down);
 lastX=point.x;lastY=point.y;data->point.x=lastX;data->point.y=lastY;
 data->state=point.down&&uiTouch(true,lastX,lastY)?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
 if(!point.down)uiTouch(false,lastX,lastY);
 data->continue_reading=uxQueueMessagesWaiting(touchQueue)>0;
}
}
void boardBegin(){
 esp_log_level_set("*",ESP_LOG_NONE);
 pinMode(8,OUTPUT);digitalWrite(8,LOW);
 busMutex=xSemaphoreCreateMutex();if(!busMutex)abort();
 Wire.begin(47,48,400000);Wire.setTimeOut(20);
 // Set the output latch BEFORE changing direction. Never pulse the system latch low.
 if(!writeWord(2,outputs)||!writeWord(6,Inputs)){
  Serial.println("FATAL Watcher power expander missing (0x21); check Grove wiring");while(true)delay(100);
 }
 delay(100);sampleInputs();
 pinMode(21,OUTPUT);digitalWrite(21,HIGH); // AI SPI select inactive; its firmware is untouched.
 pinMode(46,OUTPUT);digitalWrite(46,HIGH);
 pinMode(41,INPUT_PULLUP);pinMode(42,INPUT_PULLUP);
 decoder.reset((digitalRead(41)<<1)|digitalRead(42));
 attachInterrupt(41,encoderChanged,CHANGE);attachInterrupt(42,encoderChanged,CHANGE);
 // Arduino 3 requires an attached ADC channel before per-pin configuration.
 // Match the channel attenuation and the core's millivolt calibration.
 analogSetAttenuation(ADC_2_5db);analogRead(3);
 Serial.println("BOARD Watcher 412x412 / 32MB / rotary 41,42 + expander P03 / GPS RX19 TX20");
}
void watcherAudioPower(bool on){
 uint16_t next=on?outputs|(1u<<12):outputs&~(1u<<12);
 if(writeWord(2,next))outputs=next;
}
bool watcherPowerOff(){
 extern bool watcherCloseStorage();
 bool wasLogging=loggingEnabled;
 if(!watcherCloseStorage())return false;
 audioStop();watcherAudioPower(false);
 // Same PCA9535 system latch used by Seeed's bsp_system_shutdown().
 uint16_t previous=outputs;
 if(!writeWord(2,outputs&~(1u<<10))){loggingEnabled=wasLogging;watcherAudioPower(true);return false;}
 delay(500); // Battery-powered hardware should have cut power by now.
 // If external power kept the CPU alive, retain the latch and report failure.
 writeWord(2,previous);watcherAudioPower(true);loggingEnabled=wasLogging;return false;
}
bool watcherSleep(unsigned seconds){
 if(reconRecording()||reconStorageBusy()||wardriveBusy())return false;
 extern bool watcherCloseStorage();
 if(!watcherCloseStorage())return false;
 audioStop();
 // Stop the polling task outside its shared I2C transaction, then retain SYS.
 if(xSemaphoreTake(busMutex,pdMS_TO_TICKS(100))!=pdTRUE)return false;
 if(inputTaskHandle)vTaskSuspend(inputTaskHandle);
 xSemaphoreGive(busMutex);
 constexpr uint16_t peripheralPower=(1u<<8)|(1u<<9)|(1u<<11)|(1u<<12)|(1u<<14)|(1u<<15);
 if(!writeWord(2,outputs&~peripheralPower)){if(inputTaskHandle)vTaskResume(inputTaskHandle);return false;}
 ledcWrite(8,0);sampleInputs();
 esp_sleep_enable_ext0_wakeup(GPIO_NUM_2,0);rtc_gpio_pullup_en(GPIO_NUM_2);rtc_gpio_pulldown_dis(GPIO_NUM_2);
 if(seconds)esp_sleep_enable_timer_wakeup(uint64_t(seconds)*1000000);
 Serial.println("WATCHER_SLEEP");Serial.flush();esp_deep_sleep_start();return true;
}
void watcherAudioDiagnostics(){
 extern void watcherToneDiagnostics();watcherToneDiagnostics();
 uint8_t latch[2]{};bool ok=readRegs(Expander,2,latch,2);
 Serial.printf("WATCHER_AUDIO power_io=%u pa=%u",ok,bool(latch[1]&0x10));
 for(uint8_t reg:{uint8_t(0x00),uint8_t(0x01),uint8_t(0x0d),uint8_t(0x12),uint8_t(0x13),uint8_t(0x31),uint8_t(0x32)}){
  uint8_t value=0;bool read=readRegs(0x18,reg,&value,1);Serial.printf(" r%02x=%s%02x",reg,read?"":"ERR",value);
 }
 Serial.println();
}
void set_brightness(uint8_t value){ledcWrite(8,unsigned(constrain(value,0,100))*255/100);}
void scr_lvgl_init(){
 spi_bus_config_t bus{};bus.sclk_io_num=7;bus.data0_io_num=9;bus.data1_io_num=1;bus.data2_io_num=14;bus.data3_io_num=13;
 bus.max_transfer_sz=412*32*2;bus.flags=SPICOMMON_BUSFLAG_MASTER|SPICOMMON_BUSFLAG_QUAD;
 ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO));
 esp_lcd_panel_io_spi_config_t io{};io.cs_gpio_num=45;io.dc_gpio_num=-1;io.spi_mode=3;io.pclk_hz=40000000;
 io.trans_queue_depth=4;io.lcd_cmd_bits=32;io.lcd_param_bits=8;io.flags.quad_mode=true;
 io.on_color_trans_done=flushComplete;
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST,&io,&panelIo));
 spd2010_vendor_config_t vendor{};vendor.flags.use_qspi_interface=true;
 esp_lcd_panel_dev_config_t config{};config.reset_gpio_num=-1;config.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB;config.bits_per_pixel=16;config.vendor_config=&vendor;
 ESP_ERROR_CHECK(esp_lcd_new_panel_spd2010(panelIo,&config,&panel));
 ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
 ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
 Wire1.begin(39,38,100000);Wire1.setTimeOut(20);
 touchTransport.rx_param=touchRead;touchTransport.tx_param=touchWrite;
 touchTransport.del=[](esp_lcd_panel_io_t*){return ESP_OK;};
 esp_lcd_touch_config_t touchConfig{};touchConfig.x_max=412;touchConfig.y_max=412;touchConfig.rst_gpio_num=GPIO_NUM_NC;touchConfig.int_gpio_num=GPIO_NUM_NC;
 if(esp_lcd_touch_new_i2c_spd2010(&touchTransport,&touchConfig,&touch)==ESP_OK){
  for(int i=0;i<3;++i){esp_lcd_touch_read_data(touch);delay(50);}
 }
 touchQueue=xQueueCreate(8,sizeof(InputEdge));buttonQueue=xQueueCreate(4,sizeof(ButtonEvent));
 if(!touchQueue||!buttonQueue)abort();
 if(xTaskCreatePinnedToCore(inputTask,"watcher-input",4096,nullptr,3,&inputTaskHandle,0)!=pdPASS)abort();
 auto* pixels=(lv_color_t*)heap_caps_malloc(412*32*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
 if(!pixels){Serial.println("FATAL Watcher display DMA buffer");abort();}
 lv_init();lv_disp_draw_buf_init(&drawBuffer,pixels,nullptr,412*32);
 lv_disp_drv_init(&displayDriver);displayDriver.hor_res=412;displayDriver.ver_res=412;displayDriver.draw_buf=&drawBuffer;
 displayDriver.flush_cb=[](lv_disp_drv_t* drv,const lv_area_t* area,lv_color_t* data){
  if(esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,data)!=ESP_OK)lv_disp_flush_ready(drv);
 };
 lv_disp_drv_register(&displayDriver);
 static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=readTouch;lv_indev_drv_register(&input);
 rotaryUiBegin();ledcAttach(8,5000,8);set_brightness(50);
 Serial.printf("WATCHER_DISPLAY ready=1 touch=%u dma_bytes=%u\n",touch!=nullptr,412*32*2);
}
bool boardMountCard(){cardSpi.begin(4,6,5,46);return SD.begin(46,cardSpi,10000000,"/sdcard",12,false);}
void boardLoop(){
 uint32_t now=millis();
 portENTER_CRITICAL(&encoderMux);int steps=uint32_t(now-lastEdgeAt)<=350?detents:0;detents=0;portEXIT_CRITICAL(&encoderMux);
 position+=steps;bool click=false,back=false;static uint32_t batteryAt=0;
 ButtonEvent event;while(xQueueReceive(buttonQueue,&event,0)==pdTRUE){if(uint32_t(now-event.at)<=500){click=event.value==dragon::RotaryPress::Click;back=event.value==dragon::RotaryPress::Back;break;}}
 // Do not let wheel events overtake a queued touch gesture between UI frames.
 portENTER_CRITICAL(&encoderMux);bool touching=physicalTouch.down;portEXIT_CRITICAL(&encoderMux);
 if(touching||uxQueueMessagesWaiting(touchQueue)>0)rotaryUiTouch(true);
 rotaryUiInput(steps,click,back);
 if(!batteryAt||now-batteryAt>=5000){batteryAt=now;uint32_t sum=0;for(unsigned i=0;i<8;++i)sum+=analogReadMilliVolts(3);
  batteryMv=(sum*41)/(8*10);battery=(batteryMv<2500||batteryMv>4500)?-1:constrain((int(batteryMv)-3300)*100/900,0,100);
 }
}
void testTouch(int x,int y,bool down){injected=true;injectedX=x;injectedY=y;injectedPress=down;}
void watcherInputStatus(){
 Serial.printf("WATCHER_STATUS flash=%u psram=%u free_psram=%u internal=%u battery_mv=%lu battery_estimate=%d sd=%u rtc=%u\n",ESP.getFlashChipSize(),ESP.getPsramSize(),ESP.getFreePsram(),ESP.getFreeHeap(),batteryMv,battery,sdReady,rtcAvailable);
 Serial.printf("ROTARY_STATUS position=%d edges=%lu clicks=%lu holds=%lu inputs=%04x errors=%lu\n",position,edges,clicks,holds,inputs,i2cErrors);
 Serial.printf("TOUCH_STATUS enabled=%u presses=%lu samples=%lu xy=%d,%d errors=%lu\n",touch!=nullptr,touchPresses,touchSamples,lastX,lastY,touchErrors);
 Serial.printf("TOUCH_POLL count=%lu status=%08lx packets=%lu raw_contacts=%lu raw_xy=%u,%u strength=%u\n",touchPolls,touchStatusRaw,touchPackets,rawContacts,rawX,rawY,rawStrength);
 Serial.printf("DISPLAY_STATUS frames=%lu resolution=412x412\n",flushes);
}
int batteryPercent(){return battery;}
bool batteryCharging(){return battery>=0&&!(inputs&1);}
void boardSensorsBegin(){uint8_t control=0;rtcAvailable=readRegs(0x51,0,&control,1);}
static int bcd(uint8_t v){return (v&15)<=9&&(v>>4)<=9?(v>>4)*10+(v&15):-1;}
bool boardReadRtc(uint32_t& epoch){
 uint8_t control=0,b[7];if(!rtcAvailable||!readRegs(0x51,0,&control,1)||(control&0xa8)||!readRegs(0x51,2,b,7)||(b[0]&0x80))return false;
 dragon::Calendar date{2000+bcd(b[6]),bcd(b[5]&31),bcd(b[3]&63),bcd(b[2]&63),bcd(b[1]&127),bcd(b[0]&127)};
 epoch=dragon::dateEpoch(date);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
 if(!rtcAvailable||!dragon::validEpoch(epoch))return false;time_t value=epoch;struct tm t{};gmtime_r(&value,&t);
 auto encode=[](int v){return uint8_t((v/10)*16+v%10);};uint8_t control=0x20;
 if(!writeRegs(0x51,0,&control,1))return false;
 uint8_t b[]={encode(t.tm_sec),encode(t.tm_min),encode(t.tm_hour),encode(t.tm_mday),uint8_t(t.tm_wday),encode(t.tm_mon+1),encode(t.tm_year%100)};
 bool ok=writeRegs(0x51,2,b,7);control=0;bool restarted=writeRegs(0x51,0,&control,1);uint32_t readback=0;
 return ok&&restarted&&boardReadRtc(readback)&&readback>=epoch&&readback-epoch<=1;
}
void boardRtcDiagnostics(){uint8_t b[3]{};bool ok=readRegs(0x51,0,b,3);Serial.printf("CLOCK_DIAGNOSTICS chip=PCF8563 io=%u control=%02x lost=%u\n",ok,b[0],bool(b[2]&0x80));}
#endif
