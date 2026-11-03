// MIT: isolated Guition beta display path; stable Spotpear header is unchanged.
#ifndef _GUITION_BETA_DISPLAY_H_
#define _GUITION_BETA_DISPLAY_H_

#include "board_pins.h"
#include <lvgl.h>
#include <ESP_IOExpander.h>
#include <ESP_Panel_Library.h>

#define SCREEN_RES_HOR 360
#define SCREEN_RES_VER 360

static lv_color_t *disp_draw_buf;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_t *indev_touchpad;
static ESP_PanelBacklight *backlight = NULL;
static ESP_PanelLcd *lcd = NULL;
static ESP_PanelTouch *touch = NULL;

#include "guition_panel_init.h"

#if TOUCH_PIN_NUM_INT >= 0

IRAM_ATTR bool onTouchInterruptCallback(void *user_data)
{
  return false;
}

#endif



#define TFT_SPI_FREQ_HZ (20 * 1000 * 1000)

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  ESP_PanelLcd *lcd = (ESP_PanelLcd *)disp->user_data;
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  if(!lcd||!lcd->drawBitmap(offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, (const uint8_t *)color_p))lv_disp_flush_ready(disp);
}

IRAM_ATTR bool onRefreshFinishCallback(void *user_data)
{
  lv_disp_drv_t *drv = (lv_disp_drv_t *)user_data;
  lv_disp_flush_ready(drv);
  return false;
}

void setRotation(uint8_t rot)
{
  if (rot > 3)
    return;
  if (lcd == NULL || touch == NULL)
    return;

  switch (rot)
  {
  case 1: // 顺时针90度
    lcd->swapXY(true);
    lcd->mirrorX(true);
    lcd->mirrorY(false);
    touch->swapXY(true);
    touch->mirrorX(true);
    touch->mirrorY(false);
    break;
  case 2:
    lcd->swapXY(false);
    lcd->mirrorX(true);
    lcd->mirrorY(true);
    touch->swapXY(false);
    touch->mirrorX(true);
    touch->mirrorY(true);
    break;
  case 3:
    lcd->swapXY(true);
    lcd->mirrorX(false);
    lcd->mirrorY(true);
    touch->swapXY(true);
    touch->mirrorX(false);
    touch->mirrorY(true);
    break;
  default:
    lcd->swapXY(false);
    lcd->mirrorX(false);
    lcd->mirrorY(false);
    touch->swapXY(false);
    touch->mirrorX(false);
    touch->mirrorY(false);
    break;
  }
}

void screen_switch(bool on)
{
  if (NULL == backlight)
    return;
  if (on)
    backlight->on();
  else
    backlight->off();
}

// 输入值为0-100
void set_brightness(uint8_t bri)
{
  if (NULL == backlight)
    return;
  backlight->setBrightness(bri);
}

extern bool uiTouch(bool pressed, int x, int y);
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
  ESP_PanelTouch *tp = (ESP_PanelTouch *)indev_drv->user_data;
  ESP_PanelTouchPoint point;

  int read_touch_result = tp->readPoints(&point, 1);
  if (read_touch_result > 0)
  {
    data->point.x = point.x;
    data->point.y = point.y;
    data->state = LV_INDEV_STATE_PRESSED;
    if(!uiTouch(true, point.x, point.y))data->state=LV_INDEV_STATE_RELEASED;
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
    uiTouch(false, 0, 0);
  }
}

static lv_indev_t *indev_init(ESP_PanelTouch *tp)
{
  ESP_PANEL_CHECK_FALSE_RET(tp != nullptr, nullptr, "Invalid touch device");
  ESP_PANEL_CHECK_FALSE_RET(tp->getHandle() != nullptr, nullptr, "Touch device is not initialized");
  static lv_indev_drv_t indev_drv_tp;
  lv_indev_drv_init(&indev_drv_tp);
  indev_drv_tp.type = LV_INDEV_TYPE_POINTER;
  indev_drv_tp.read_cb = touchpad_read;
  indev_drv_tp.user_data = (void *)tp;
  return lv_indev_drv_register(&indev_drv_tp);
}

static void guitionTouchBegin(){
  i2c_config_t config=ESP_PANEL_HOST_I2C_CONFIG_DEFAULT(TOUCH_PIN_NUM_I2C_SCL,TOUCH_PIN_NUM_I2C_SDA);
  guitionTouchStage="i2c_config";
  if(i2c_param_config(I2C_NUM_0,&config)!=ESP_OK)return;
  guitionTouchStage="i2c_install";
  if(i2c_driver_install(I2C_NUM_0,config.mode,0,0,0)!=ESP_OK)return;
  esp_lcd_panel_io_i2c_config_t io=ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
  auto* bus=new(std::nothrow) ESP_PanelBus_I2C(io,I2C_NUM_0);
  guitionTouchStage="i2c_io";
  if(bus&&bus->begin()){
    guitionTouchStage="touch_init";
    touch=new(std::nothrow) ESP_PanelTouch_CST816S(bus,SCREEN_RES_HOR,SCREEN_RES_VER,TOUCH_PIN_NUM_RST,TOUCH_PIN_NUM_INT);
    if(touch&&touch->init()&&touch->begin()){
      guitionTouchStage="ready";
#if TOUCH_PIN_NUM_INT >= 0
      touch->attachInterruptCallback(onTouchInterruptCallback,nullptr);
#endif
      return;
    }
  }
  delete touch;touch=nullptr;delete bus;i2c_driver_delete(I2C_NUM_0);
}
void scr_lvgl_init(){
  // No retry loop or forced reboot: a failed panel leaves USB and the UI alive.
  static bool initialized=false;if(initialized)return;initialized=true;
  pinMode(TFT_BLK,OUTPUT);digitalWrite(TFT_BLK,LOW);
  backlight=new(std::nothrow) ESP_PanelBacklight(TFT_BLK,true,true);
  if(backlight&&!backlight->begin()){delete backlight;backlight=nullptr;}
  if(backlight)backlight->off();
  const bool panelReady=guitionPanelBegin();
  guitionTouchBegin();
  constexpr size_t rows=24;
  disp_draw_buf=(lv_color_t*)heap_caps_malloc(rows*SCREEN_RES_HOR*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  static lv_color_t emergencyRow[SCREEN_RES_HOR];
  size_t pixels=rows*SCREEN_RES_HOR;
  if(!disp_draw_buf){
    Serial.println("GUITION_DISPLAY error=dma_tile_allocation headless=1");
    guitionPanelStage="dma_tile_allocation";
    guitionPanelCleanup();disp_draw_buf=emergencyRow;pixels=SCREEN_RES_HOR;
  }
  lv_init();lv_disp_draw_buf_init(&draw_buf,disp_draw_buf,nullptr,pixels);
  lv_disp_drv_init(&disp_drv);disp_drv.hor_res=SCREEN_RES_HOR;disp_drv.ver_res=SCREEN_RES_VER;
  disp_drv.flush_cb=my_disp_flush;disp_drv.draw_buf=&draw_buf;disp_drv.user_data=lcd;
  lv_disp_t* disp=lv_disp_drv_register(&disp_drv);
  if(lcd&&disp&&!lcd->attachRefreshFinishCallback(onRefreshFinishCallback,disp->driver)){
    guitionPanelStage="refresh_callback";guitionPanelCleanup();disp->driver->user_data=nullptr;
  }
  if(touch)indev_touchpad=indev_init(touch);
  if(panelReady&&lcd){screen_switch(true);set_brightness(75);}
  guitionDisplayDiagnostics();
}
#endif

