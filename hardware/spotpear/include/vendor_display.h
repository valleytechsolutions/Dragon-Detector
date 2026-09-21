#ifndef _SCR_ST77916_H_
#define _SCR_ST77916_H_

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

#define USE_CUSTOM_INIT_CMD 0 // 是否用自定义的初始化代码

#if TOUCH_PIN_NUM_INT >= 0

IRAM_ATTR bool onTouchInterruptCallback(void *user_data)
{
  return false;
}

#endif



#define TFT_SPI_FREQ_HZ (50 * 1000 * 1000)

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  ESP_PanelLcd *lcd = (ESP_PanelLcd *)disp->user_data;
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  lcd->drawBitmap(offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, (const uint8_t *)color_p);
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

extern void uiTouch(bool pressed, int x, int y);
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
    uiTouch(true, point.x, point.y);
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

void scr_lvgl_init()
{
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_13_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 5000,
      .clk_cfg = LEDC_AUTO_CLK};
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {
      .gpio_num = (TFT_BLK),
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_0,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_0,
      .duty = 0,
      .hpoint = 0};

  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  backlight = new ESP_PanelBacklight(ledc_timer, ledc_channel);
  backlight->begin();
  backlight->off();

  ESP_PanelBus_I2C *touch_bus = new ESP_PanelBus_I2C(TOUCH_PIN_NUM_I2C_SCL, TOUCH_PIN_NUM_I2C_SDA, ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG());
  touch_bus->configI2cFreqHz(400000);
  touch_bus->begin();

  touch = new ESP_PanelTouch_CST816S(touch_bus, SCREEN_RES_HOR, SCREEN_RES_VER, TOUCH_PIN_NUM_RST, TOUCH_PIN_NUM_INT);

  touch->init();
  touch->begin();

#if TOUCH_PIN_NUM_INT >= 0
  touch->attachInterruptCallback(onTouchInterruptCallback, NULL);
#endif

  ESP_PanelBus_QSPI *panel_bus = new ESP_PanelBus_QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
  panel_bus->configQspiFreqHz(TFT_SPI_FREQ_HZ);
  panel_bus->begin();

  lcd = new ESP_PanelLcd_ST77916(panel_bus, 16, TFT_RST);
  // 注意，初始化代码的设置必须在INIT之前
  // lcd->configVendorCommands(lcd_init_cmd, sizeof(lcd_init_cmd) / sizeof(lcd_init_cmd[0]));
  lcd->init();
  lcd->reset();
  lcd->begin();

  lcd->invertColor(true);
  // setRotation(0);  //设置屏幕方向
  lcd->displayOn();

  screen_switch(true);
  backlight->setBrightness(75); // 设置亮度

  size_t lv_cache_rows = 24;

  disp_draw_buf = (lv_color_t *)heap_caps_malloc(lv_cache_rows * SCREEN_RES_HOR * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!disp_draw_buf) { Serial.println("Display buffer allocation failed"); abort(); }
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, SCREEN_RES_HOR * lv_cache_rows);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_RES_HOR;
  disp_drv.ver_res = SCREEN_RES_VER;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.user_data = (void *)lcd;
  lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

  if (lcd->getBus()->getType() != ESP_PANEL_BUS_TYPE_RGB)
  {
    lcd->attachRefreshFinishCallback(onRefreshFinishCallback, (void *)disp->driver);
  }
  indev_touchpad = indev_init(touch);
}

#endif