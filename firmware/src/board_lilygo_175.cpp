// LILYGO 1.75 H0175Y003AM. Working display/touch baseline preserved.
#include "board_pins.h"
#ifdef BOARD_LILYGO_175
#include "app.h"
#include "board_pins.h"
#include <lvgl.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_co5300.h>
#include <TouchDrvCSTXXX.hpp>
#include <SPI.h>
#include <time.h>

static esp_lcd_panel_io_handle_t panelIo=nullptr;
static esp_lcd_panel_handle_t panel=nullptr;
static TouchDrvCST92xx touch;
static bool touchReady=false;
static SPIClass cardSpi(HSPI);
static SemaphoreHandle_t i2cMutex=nullptr;
static volatile bool touchIrq=false;
static void IRAM_ATTR onTouch(){touchIrq=true;}
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

extern bool uiTouch(bool pressed,int x,int y);

// CO5300 only accepts windows that start on even and end on odd coordinates.
static void rounder(lv_disp_drv_t*,lv_area_t* area){
  area->x1&=~1;area->y1&=~1;area->x2|=1;area->y2|=1;
}
static void flush(lv_disp_drv_t* disp,const lv_area_t* area,lv_color_t* color_p){
  // The completion callback releases LVGL's buffer only after DMA is finished.
  if(esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,color_p)!=ESP_OK)lv_disp_flush_ready(disp);
}
static void touchRead(lv_indev_drv_t*,lv_indev_data_t* data){
  static int16_t x[2],y[2];static lv_coord_t lastX=0,lastY=0;static bool was=false,pressed=false;static uint32_t lastReport=0;
  // The CST9217 sleeps between touches and NACKs polling, so only read after
  // its interrupt. A missed release report is covered by the report timeout.
  if(touchIrq){
    touchIrq=false;
    int count=0;if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(15))==pdTRUE){count=touch.getPoint(x,y,1);xSemaphoreGive(i2cMutex);}
    if(count>0){pressed=true;lastReport=millis();lastX=constrain(x[0],0,SCREEN_RES-1);lastY=constrain(y[0],0,SCREEN_RES-1);}
    else pressed=false;
  }else if(pressed && millis()-lastReport>200)pressed=false;
  if(pressed!=was){was=pressed;if(pressed)Serial.printf("TOUCH %d %d\n",lastX,lastY);}
  data->point.x=lastX;data->point.y=lastY;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
  if(!uiTouch(pressed,lastX,lastY))data->state=LV_INDEV_STATE_RELEASED;
}
// 输入值为0-100
void set_brightness(uint8_t bri){if(panelIo){uint8_t value=uint16_t(constrain(bri,0,100))*255/100;esp_lcd_panel_io_tx_param(panelIo,0x02005100,&value,1);}}
// SY6970 has no reliable state-of-charge gauge. Keep brightness in the UI
// instead of inventing a battery percentage from a floating USB-only reading.
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}
void scr_lvgl_init(){
  Wire.begin(IIC_SDA,IIC_SCL);Wire.setClock(400000);
  Wire.setTimeOut(15);

  spi_bus_config_t bus{};bus.sclk_io_num=LCD_SCLK;bus.data0_io_num=LCD_SDIO0;bus.data1_io_num=LCD_SDIO1;
  bus.data2_io_num=LCD_SDIO2;bus.data3_io_num=LCD_SDIO3;bus.max_transfer_sz=SCREEN_RES*40*2;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t io{};io.cs_gpio_num=LCD_CS;io.dc_gpio_num=-1;io.spi_mode=0;io.pclk_hz=40000000;
  io.trans_queue_depth=1;io.lcd_cmd_bits=32;io.lcd_param_bits=8;io.flags.quad_mode=true;io.user_ctx=&disp_drv;
  io.on_color_trans_done=[](esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void* ctx){lv_disp_flush_ready((lv_disp_drv_t*)ctx);return false;};
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&io,&panelIo));
  esp_lcd_panel_dev_config_t cfg{};cfg.reset_gpio_num=LCD_RESET;cfg.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB;cfg.bits_per_pixel=16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panelIo,&cfg,&panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel,LCD_COL_OFFSET,0));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,false));ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));set_brightness(50);
  Serial.printf("DISPLAY: CO5300 %dx%d\n",SCREEN_RES,SCREEN_RES);

  touch.setPins(TP_RESET,TP_INT);
  touchReady=touch.begin(Wire,TP_I2C_ADDR,IIC_SDA,IIC_SCL);
  if(touchReady){touch.setMaxCoordinates(SCREEN_RES,SCREEN_RES);touch.setMirrorXY(true,true);pinMode(TP_INT,INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(TP_INT),onTouch,FALLING);Serial.printf("TOUCH: %s ready\n",touch.getModelName());}
  else Serial.println("TOUCH: CST9217 not found; BOOT button still switches radar / tools");

  size_t rows=40;
  auto* buffer=(lv_color_t*)heap_caps_malloc(rows*SCREEN_RES*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(!buffer){Serial.println("Display buffer allocation failed");abort();}
  lv_init();
  lv_disp_draw_buf_init(&draw_buf,buffer,NULL,SCREEN_RES*rows);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res=SCREEN_RES;disp_drv.ver_res=SCREEN_RES;
  disp_drv.flush_cb=flush;disp_drv.rounder_cb=rounder;disp_drv.draw_buf=&draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type=LV_INDEV_TYPE_POINTER;indev_drv.read_cb=touchRead;
  lv_indev_drv_register(&indev_drv);
}

void boardBegin(){
  pinMode(BTN_PIN,INPUT_PULLUP);pinMode(LCD_ENABLE,OUTPUT);digitalWrite(LCD_ENABLE,HIGH);delay(20);
  pinMode(LILY_SD_CS,OUTPUT);digitalWrite(LILY_SD_CS,HIGH);
  i2cMutex=xSemaphoreCreateMutex();if(!i2cMutex)abort();
}
bool boardMountCard(){
  // Separate SPI3 bus: SD writes cannot reconfigure the QSPI display bus.
  cardSpi.begin(LILY_SD_SCLK,LILY_SD_MISO,LILY_SD_MOSI,LILY_SD_CS);
  return SD.begin(LILY_SD_CS,cardSpi,10000000,"/sdcard",8,false);
}
void boardLoop(){
  static bool raw=false,stable=false;static uint32_t changed=0;
  uint32_t now=millis();bool down=digitalRead(BTN_PIN)==LOW;
  if(down!=raw){raw=down;changed=now;}
  if(raw!=stable&&now-changed>=25){stable=raw;if(stable&&!uiStopwatchKey(true))uiShow("menu");}
}
// PCF8563: calendar starts at 0x02, voltage-loss flag is seconds bit 7.
// Its diode-isolated backup supply is passive; never reset it at startup.
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
  Serial.printf("CLOCK_DIAGNOSTICS chip=PCF8563 io=%u control=%02x lost=%u\n",ok,control,(seconds&0x80)!=0);
}

#endif
