// Dragon Detector integration for Waveshare ESP32-S3-Touch-AMOLED-1.43C / 1.43C-EN.
// CO5300 QSPI panel through the bundled esp_lcd driver with Waveshare's 1.43C
// initialization values, CST820 touch read directly over I2C, one-button BOOT
// navigation, ES8311 speaker supply and a voltage-based battery estimate.
// Wiring: docs/WAVESHARE-AMOLED-143C.md.
#include "board_pins.h"
#ifdef BOARD_WAVESHARE_143C
#include "app.h"
#include "waveshare_143c_input.h"
#include <lvgl.h>
#include <Wire.h>
#include <esp32-hal-i2c.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_co5300.h>

static esp_lcd_panel_io_handle_t panelIo=nullptr;
static esp_lcd_panel_handle_t panel=nullptr;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static waveshare143c::TouchState touch;
static waveshare143c::TouchCadence touchCadence;
static waveshare143c::TouchInput touchInput;
static lv_indev_t* touchDevice=nullptr;
static bool touchReady=false,touchAwake=false;
static uint8_t touchChip=0;
static uint32_t touchSamples=0,touchErrors=0,touchDowns=0,touchReplayed=0,touchStale=0,touchDropped=0,touchProbedAt=0;
// Set when what LVGL holds is no longer what was on the panel when the
// finger was sampled (see waveshare143cPictureChanged and set_brightness).
static bool touchDiscard=false,touchRedrawn=true,panelDark=false;
static uint32_t refreshes=0,touchPictureAt=0;
static uint32_t flushes=0;
static volatile bool displayRegistered=false;
static uint32_t flushErrors=0;
static int battery=-1;
static uint16_t batteryMv=0;
static waveshare143c::ChargeFilter charger;
static uint32_t batteryAt=0;
static waveshare143c::KeyDebounce keyDebounce;
static waveshare143c::KeyClicks keyClicks;
static QueueHandle_t keyQueue=nullptr;
static StaticQueue_t keyQueueControl;
static uint8_t keyQueueStorage[8*sizeof(waveshare143c::KeyEvent)];
static bool keySampler=false;
static std::atomic<bool> timerInputMode{false};
static std::atomic<uint32_t> keyPresses{0},keyDrops{0};
static uint32_t keyCycles=0,keyHomes=0,keyTimerClicks=0;

extern bool uiTouch(bool pressed,int x,int y);
// One-button navigation in ui.cpp, shared with the 1.85B.
void ui185bCycle();
void ui185bHome();
bool ui185bTimerVisible();

// Waveshare's 1.43C sequence. The driver sends MADCTL and COLMOD first; 0x36
// then selects the factory orientation. Brightness stays at zero until the
// frame memory has been cleared, so power-on noise is never shown.
static const co5300_lcd_init_cmd_t panelInit[]={
  {0xFE,(uint8_t[]){0x00},1,0},                   // user command page
  {0xC4,(uint8_t[]){0x80},1,0},                   // QSPI interface
  {0x35,(uint8_t[]){0x00},1,0},                   // tearing-effect line on
  {0x53,(uint8_t[]){0x20},1,10},                  // brightness control on
  {0x51,(uint8_t[]){0x00},1,10},                  // brightness 0
  {0x36,(uint8_t[]){0xC0},1,0},                   // factory orientation (180 degrees)
  {0x63,(uint8_t[]){0xFF},1,0},                   // high-brightness-mode level
  {0x2A,(uint8_t[]){0x00,0x06,0x01,0xD7},4,0},    // 466 visible columns
  {0x2B,(uint8_t[]){0x00,0x00,0x01,0xD1},4,0},    // 466 rows
  {0x11,(uint8_t[]){0x00},0,120},                 // sleep out
};

// CO5300 only accepts windows that start on even and end on odd coordinates.
static void rounder(lv_disp_drv_t*,lv_area_t* area){
  area->x1&=~1;area->y1&=~1;area->x2|=1;area->y2|=1;
}
static void flush(lv_disp_drv_t* disp,const lv_area_t* area,lv_color_t* color_p){
  if(lv_disp_flush_is_last(disp))++refreshes;
  // The completion callback releases LVGL's buffer only after DMA is finished.
  if(esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,color_p)!=ESP_OK){++flushErrors;lv_disp_flush_ready(disp);}
}
// Runs in the SPI interrupt. Transfers made before LVGL owns the display (the
// startup blanking below) have no draw buffer to release.
static bool IRAM_ATTR flushComplete(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void* context){
  if(displayRegistered){++flushes;lv_disp_flush_ready((lv_disp_drv_t*)context);}
  return false;
}

// The codec driver uses the same HAL calls on this bus. They take the bus lock
// themselves and, unlike Wire, stay silent when a sleeping controller NACKs.
static bool busRead(uint8_t address,uint8_t reg,uint8_t* data,size_t size){
  size_t count=0;
  return i2cWriteReadNonStop(0,address,&reg,1,data,size,20,&count)==ESP_OK&&count==size;
}
static bool busWrite(uint8_t address,uint8_t reg,uint8_t value){
  const uint8_t frame[]={reg,value};
  return i2cWrite(0,address,frame,sizeof(frame),20)==ESP_OK;
}
// 0xA7 is the chip ID; a non-zero 0xFE keeps the controller out of auto-sleep
// so it keeps answering polls between touches.
static bool touchProbe(){
  uint8_t id=0;
  if(!busRead(TP_I2C_ADDR,0xA7,&id,1))return false;
  touchChip=id;touchAwake=busWrite(TP_I2C_ADDR,0xFE,0xFF);return true;
}
// LVGL reads the pointer once per UI pass, about every 0.2 s while the radar
// redraws, so a quick tap would fall between two reads. The controller is
// therefore also read while each display transfer is in flight (flushWait,
// about every 14 ms on the radar), in the UI task and without a task of its own.
static void touchPoll(uint32_t now){
  if(!touchReady||!touchCadence.due(now))return;
  uint8_t packet[5]{};
  const bool ok=busRead(TP_I2C_ADDR,0x02,packet,sizeof(packet));
  const uint32_t after=millis();
  touchCadence.done(after,ok,after-now);
  const bool valid=touch.update(packet,sizeof(packet),ok,after);
  if(valid)++touchSamples;else ++touchErrors;
  // A controller that could not be kept awake stops answering when the finger
  // lifts, so its silence is the lift. From an awake one, silence is a fault.
  touchInput.sample(touch.pressed,touch.x,touch.y,valid||(!touchAwake&&!touch.pressed),after);
}
static void flushWait(lv_disp_drv_t*){touchPoll(millis());}
static void touchRead(lv_indev_drv_t*,lv_indev_data_t* data){
  static bool was=false;
  if(touchReady)touchPoll(millis());
  else if(uint32_t(millis()-touchProbedAt)>=2000){
    // A controller that missed the startup probe is retried without blocking.
    touchProbedAt=millis();touchReady=touchProbe();
    if(touchReady)Serial.printf("TOUCH: CST820 ready chip=%02x (late)\n",touchChip);
  }
  // Input sampled before the picture changed, or while the new picture was
  // still being sent to the panel, was meant for the old one.
  bool discard=touchDiscard;touchDiscard=false;
  if(!touchRedrawn&&refreshes!=touchPictureAt){touchRedrawn=true;discard=true;}
  // A tap that ended before this read arrives as a press with its release
  // waiting behind it; LVGL then reads again at once.
  const waveshare143c::TouchStep step=touchInput.next(millis(),discard);
  // Input too old to replay or meant for another picture: LVGL's press ends
  // without a click. PRESS_LOST lets a button, wheel or list settle.
  if(step.cancel){++(discard?touchDropped:touchStale);if(touchDevice)lv_indev_wait_release(touchDevice);}
  if(step.pressed!=was){was=step.pressed;if(was){++touchDowns;if(step.more)++touchReplayed;Serial.printf("TOUCH %u %u\n",unsigned(step.x),unsigned(step.y));}}
  data->point.x=step.x;data->point.y=step.y;data->state=step.pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
  data->continue_reading=step.more;
  if(!uiTouch(step.pressed,step.x,step.y))data->state=LV_INDEV_STATE_RELEASED;
}

// The UI calls this when it builds a page and when a notice or dialog appears
// or goes away. A tap sampled before then was made on the old picture, and so
// was one sampled while the change was still being sent to the panel, which
// takes up to a radar pass. Neither is replayed onto the new picture.
// Removed objects are gone at once, added ones are hit only once drawn.
void waveshare143cPictureChanged(bool removed){
  if(removed)touchDiscard=true;
  touchRedrawn=false;touchPictureAt=refreshes;
}

// 输入值为0-100
// A tap on the dark panel was made on a blank screen. The UI lights the panel
// a pass after the waking touch, and taps sampled until then are dropped.
void set_brightness(uint8_t bri){
  if(!bri)panelDark=true;else if(panelDark){panelDark=false;touchDiscard=true;}
  if(panelIo){uint8_t value=uint16_t(constrain(bri,0,100))*255/100;esp_lcd_panel_io_tx_param(panelIo,0x02005100,&value,1);}}

static void sampleBattery(){
  const uint32_t now=millis();
  if(batteryAt&&uint32_t(now-batteryAt)<5000)return;
  batteryAt=now?now:1;
  uint32_t sum=0;
  for(unsigned i=0;i<8;++i)sum+=analogReadMilliVolts(BATTERY_ADC_PIN);
  batteryMv=uint16_t(sum/8*2);
  battery=waveshare143c::batteryPercentFromMillivolts(batteryMv);
}
// The CHG LED hangs on GPIO5 (low = lit), not on the charger, so the firmware
// mirrors the ETA6098 STAT line. A charger without a cell toggles STAT; only a
// steady second of either level changes the reported state.
static void pollCharger(uint32_t now){
  static uint32_t polledAt=0;
  if(polledAt&&uint32_t(now-polledAt)<250)return;
  polledAt=now?now:1;
  digitalWrite(BOARD_LED_PIN,charger.update(digitalRead(CHARGE_STATUS_PIN)==LOW)?LOW:HIGH);
}
int batteryPercent(){sampleBattery();return battery;}
bool batteryCharging(){return charger.charging;}

// BOOT is sampled every 10 ms from the system timer task, so a quick tap is
// seen and a timer click keeps the time of the press even while the UI is busy
// drawing. A task of its own would cost about 3.5 KiB of the internal heap that
// Wi-Fi needs on this board; the timer task already exists.
static void pollKey(){
  const uint32_t now=millis();
  const bool was=keyDebounce.stable,down=keyDebounce.update(digitalRead(BTN_PIN)==LOW,now);
  if(down&&!was)++keyPresses;
  const waveshare143c::KeyEvent event=keyClicks.update(down,timerInputMode.load(),now,stopwatchNow());
  if(event.action!=waveshare143c::KeyAction::None&&xQueueSend(keyQueue,&event,0)!=pdTRUE){xQueueReset(keyQueue);++keyDrops;}
}
static void keyTick(void*){pollKey();}

void scr_lvgl_init(){
  Wire.begin(IIC_SDA,IIC_SCL);Wire.setClock(400000);Wire.setTimeOut(20);

  // 32 rows, not the 40 of the 1.75 boards. This buffer and the equally sized
  // rotation buffer in wearable.cpp are internal DMA memory, so 8 rows are
  // 15 KB: with 40 the lab portal's 35,000-byte floor fails after a Wi-Fi join.
  const size_t rows=32;
  auto* buffer=(lv_color_t*)heap_caps_calloc(rows*SCREEN_RES,sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(!buffer){Serial.println("Display buffer allocation failed");abort();}

  spi_bus_config_t bus{};bus.sclk_io_num=LCD_SCLK;bus.data0_io_num=LCD_SDIO0;bus.data1_io_num=LCD_SDIO1;
  bus.data2_io_num=LCD_SDIO2;bus.data3_io_num=LCD_SDIO3;bus.max_transfer_sz=SCREEN_RES*rows*2;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t io{};io.cs_gpio_num=LCD_CS;io.dc_gpio_num=-1;io.spi_mode=0;io.pclk_hz=40000000;
  io.trans_queue_depth=1;io.lcd_cmd_bits=32;io.lcd_param_bits=8;io.flags.quad_mode=true;
  io.user_ctx=&disp_drv;io.on_color_trans_done=flushComplete;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&io,&panelIo));
  static co5300_vendor_config_t vendor{};vendor.init_cmds=panelInit;vendor.init_cmds_size=sizeof(panelInit)/sizeof(panelInit[0]);
  esp_lcd_panel_dev_config_t cfg{};cfg.reset_gpio_num=LCD_RESET;cfg.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB;cfg.bits_per_pixel=16;cfg.vendor_config=&vendor;
  ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panelIo,&cfg,&panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel,LCD_COL_OFFSET,0));
  // Blank the frame memory before the panel is switched on. Each command waits
  // for the previous colour transfer, so the zeroed buffer can be reused.
  for(int y=0;y<SCREEN_RES;y+=int(rows))ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,0,y,SCREEN_RES,std::min(y+int(rows),SCREEN_RES),buffer));
  // DISPON also waits for the last blanking transfer before LVGL takes over.
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
  set_brightness(50);
  Serial.printf("DISPLAY: CO5300 %dx%d 1.43C\n",SCREEN_RES,SCREEN_RES);

  pinMode(TP_INT,INPUT_PULLUP);
  pinMode(TP_RESET,OUTPUT);digitalWrite(TP_RESET,HIGH);delay(10);
  digitalWrite(TP_RESET,LOW);delay(20);digitalWrite(TP_RESET,HIGH);delay(200);
  for(unsigned attempt=0;attempt<5&&!touchReady;++attempt){touchReady=touchProbe();if(!touchReady)delay(20);}
  touchProbedAt=millis();
  if(touchReady)Serial.printf("TOUCH: CST820 ready chip=%02x\n",touchChip);
  else Serial.println("TOUCH: CST820 not found; BOOT steps through the screens, dialogs need touch");

  lv_init();
  lv_disp_draw_buf_init(&draw_buf,buffer,NULL,SCREEN_RES*rows);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res=SCREEN_RES;disp_drv.ver_res=SCREEN_RES;
  disp_drv.flush_cb=flush;disp_drv.rounder_cb=rounder;disp_drv.wait_cb=flushWait;disp_drv.draw_buf=&draw_buf;
  lv_disp_drv_register(&disp_drv);
  displayRegistered=true;

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type=LV_INDEV_TYPE_POINTER;indev_drv.read_cb=touchRead;
  touchDevice=lv_indev_drv_register(&indev_drv);
}

void boardBegin(){
  pinMode(BTN_PIN,INPUT_PULLUP);
  pinMode(CHARGE_STATUS_PIN,INPUT_PULLUP);
  pinMode(BOARD_LED_PIN,OUTPUT);digitalWrite(BOARD_LED_PIN,HIGH);
  // The codec's 3.3 V regulator is enabled by this line (already pulled high
  // on the board). Drive it well before the audio module configures the ES8311.
  pinMode(AUDIO_LDO_EN,OUTPUT);digitalWrite(AUDIO_LDO_EN,HIGH);
  // Keep the speaker amplifier off until the tone output has queued silence.
  pinMode(AUDIO_MUTE_PIN,OUTPUT);digitalWrite(AUDIO_MUTE_PIN,LOW);
  keyQueue=xQueueCreateStatic(8,sizeof(waveshare143c::KeyEvent),keyQueueStorage,&keyQueueControl);
  esp_timer_create_args_t sampler{};
  sampler.callback=keyTick;sampler.dispatch_method=ESP_TIMER_TASK;sampler.name="143c-key";sampler.skip_unhandled_events=true;
  esp_timer_handle_t timer=nullptr;
  // Without the timer the key is still polled from the main loop.
  keySampler=esp_timer_create(&sampler,&timer)==ESP_OK&&esp_timer_start_periodic(timer,10000)==ESP_OK;
}
// BOOT is the only firmware button. PWR belongs to the board's power latch:
// a short press switches on and a long press switches off, without firmware.
void boardLoop(){
  timerInputMode.store(ui185bTimerVisible());
  if(!keySampler)pollKey();
  waveshare143c::KeyEvent key;
  // One navigation step per pass: the UI builds the new page after this
  // function returns and ignores a second step until it has.
  bool navigated=false;
  for(unsigned n=0;n<8&&!navigated&&xQueueReceive(keyQueue,&key,0)==pdTRUE;++n){
    const waveshare143c::KeyRoute route=waveshare143c::routeKey(key,ui185bTimerVisible(),millis());
    switch(route){
      case waveshare143c::KeyRoute::Home:++keyHomes;ui185bHome();break;
      // The first press time, not the recognition time, is what gets recorded.
      case waveshare143c::KeyRoute::Timer:++keyTimerClicks;uiStopwatchKey(key.action==waveshare143c::KeyAction::Single,key.at);break;
      case waveshare143c::KeyRoute::Cycle:++keyCycles;ui185bCycle();break;
      default:++keyDrops;break;
    }
    navigated=waveshare143c::endsDrain(route);
  }
  timerInputMode.store(ui185bTimerVisible());
  pollCharger(millis());
}
// No RTC is fitted. The clock comes from USB or saved Wi-Fi and is kept by the
// ESP32 while it stays powered.
void boardSensorsBegin(){rtcAvailable=false;}
bool boardReadRtc(uint32_t&){return false;}
bool boardWriteRtc(uint32_t){return false;}

void waveshare143cHardwareStatus(){
  sampleBattery();
  Serial.printf("143C_STATUS flash=%u psram=%u heap=%u min_heap=%u panel=CO5300 clock_hz=40000000 column_offset=%d flushes=%lu flush_errors=%lu\n",
    unsigned(ESP.getFlashChipSize()),unsigned(ESP.getPsramSize()),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),LCD_COL_OFFSET,(unsigned long)flushes,(unsigned long)flushErrors);
  Serial.printf("TOUCH_STATUS ready=%u chip=%02x awake=%u irq=%d samples=%lu errors=%lu downs=%lu replayed=%lu stale=%lu dropped=%lu pressed=%u xy=%u,%u\n",
    touchReady,touchChip,touchAwake,digitalRead(TP_INT),(unsigned long)touchSamples,(unsigned long)touchErrors,(unsigned long)touchDowns,(unsigned long)touchReplayed,(unsigned long)touchStale,(unsigned long)touchDropped,touch.pressed,unsigned(touch.x),unsigned(touch.y));
  Serial.printf("KEY_STATUS boot=%d sampler=%u presses=%lu cycles=%lu homes=%lu timer_clicks=%lu drops=%lu timer_mode=%u\n",
    digitalRead(BTN_PIN),keySampler,(unsigned long)keyPresses.load(),(unsigned long)keyCycles,(unsigned long)keyHomes,(unsigned long)keyTimerClicks,(unsigned long)keyDrops.load(),timerInputMode.load());
  Serial.printf("BATTERY_STATUS percent=%d mv=%u charging=%u stat=%d gauge=voltage power_key=hardware\n",battery,batteryMv,charger.charging,digitalRead(CHARGE_STATUS_PIN));
}
#endif
