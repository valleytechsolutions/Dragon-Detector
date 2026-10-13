// Waveshare ESP32-S3-Touch-AMOLED-1.75C: CO5300 QSPI AMOLED, CST9217 touch, AXP2101 PMU.
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
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

static esp_lcd_panel_io_handle_t panelIo=nullptr;
static esp_lcd_panel_handle_t panel=nullptr;
static TouchDrvCST92xx touch;
static XPowersPMU power;
static bool touchReady=false,powerReady=false;
static volatile bool touchIrq=false;
static void IRAM_ATTR onTouch(){touchIrq=true;}
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

extern void uiTouch(bool pressed,int x,int y);

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
    if(touch.getPoint(x,y,1)>0){pressed=true;lastReport=millis();lastX=constrain(x[0],0,SCREEN_RES-1);lastY=constrain(y[0],0,SCREEN_RES-1);}
    else pressed=false;
  }else if(pressed && millis()-lastReport>200)pressed=false;
  if(pressed!=was){was=pressed;if(pressed)Serial.printf("TOUCH %d %d\n",lastX,lastY);}
  data->point.x=lastX;data->point.y=lastY;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
  uiTouch(pressed,lastX,lastY);
}
// 输入值为0-100
void set_brightness(uint8_t bri){if(panelIo){uint8_t value=uint16_t(constrain(bri,0,100))*255/100;esp_lcd_panel_io_tx_param(panelIo,0x02005100,&value,1);}}
int batteryPercent(){
  static uint32_t at=0;static int value=-1;
  if(!powerReady)return -1;
  if(!at || millis()-at>5000){at=millis();value=power.isBatteryConnect()?power.getBatteryPercent():-1;}
  return value;
}
bool batteryCharging(){return powerReady && power.isCharging();}
void scr_lvgl_init(){
  Wire.begin(IIC_SDA,IIC_SCL);Wire.setClock(400000);
  powerReady=power.begin(Wire,AXP2101_SLAVE_ADDRESS,IIC_SDA,IIC_SCL);
  if(powerReady){power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);power.clearIrqStatus();power.enableBattDetection();power.enableBattVoltageMeasure();power.enableVbusVoltageMeasure();}
  Serial.printf("PMU: %s\n",powerReady?"AXP2101 ready":"AXP2101 not found");

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
