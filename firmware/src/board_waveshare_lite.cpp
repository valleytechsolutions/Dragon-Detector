// Waveshare ESP32-S3-LCD-1.28 (non-touch), ESP32-S3R2 / 16MB flash / 2MB QSPI PSRAM.
// Pin reference: official Waveshare schematic; see docs/WAVESHARE-LITE.md.
#include "app.h"
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
static TFT_eSPI lcd;
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;
void boardBegin(){}
void boardLoop(){}
void set_brightness(uint8_t){digitalWrite(LITE_BACKLIGHT,HIGH);}
void scr_lvgl_init(){
  Serial.println("LITE_DISPLAY begin");Serial.flush();
  pinMode(LITE_BACKLIGHT,OUTPUT);digitalWrite(LITE_BACKLIGHT,LOW);
  lcd.begin();Serial.println("LITE_DISPLAY initialized");Serial.flush();
  lcd.setRotation(0);lcd.fillScreen(TFT_BLACK);Serial.println("LITE_DISPLAY cleared");Serial.flush();
  lv_init();
  // An 11.5KB internal-RAM tile avoids slow QSPI-PSRAM reads during software
  // rendering. Each SPI write covers at most 24 rows; the shared renderer holds
  // animation time and RSSI interpolation steady across all tiles of a frame.
  auto* pixels=(lv_color_t*)heap_caps_malloc(240*24*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("FATAL Lite display allocation");abort();}
  lv_disp_draw_buf_init(&drawBuffer,pixels,nullptr,240*24);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=240;displayDriver.ver_res=240;displayDriver.draw_buf=&drawBuffer;
  displayDriver.flush_cb=[](lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels){
    lcd.startWrite();lcd.setAddrWindow(area->x1,area->y1,area->x2-area->x1+1,area->y2-area->y1+1);
    lcd.pushColors(reinterpret_cast<uint16_t*>(pixels),size_t(area->x2-area->x1+1)*(area->y2-area->y1+1),false);
    lcd.endWrite();lv_disp_flush_ready(driver);
  };
  lv_disp_drv_register(&displayDriver);set_brightness(100);
  Serial.println("BOARD LITE GC9A01=240x240 PSRAM=QSPI2MB touch=0 audio=0 brightness=100");
}
// No calibrated fuel gauge. Never present USB/ADC voltage as battery percent.
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}
