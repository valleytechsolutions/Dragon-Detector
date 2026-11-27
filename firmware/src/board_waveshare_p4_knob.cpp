// Original Dragon Detector integration for the Waveshare ESP32-P4-Knob-Touch-AMOLED-1.75.
// The CO5300 command list and DSI timing, the knob debounce, the haptic click
// patterns and the SYSTIMER repair follow Waveshare's board files (Apache-2.0).
// The ES8389 startup table and the ES7210 microphone register values follow
// Waveshare's KnobBoardAudio (Apache-2.0, adapted from Espressif
// esp_codec_dev). See CREDITS.md.
#ifdef BOARD_WAVESHARE_P4_KNOB
#include "app.h"
#include "missions.h"
#include "amoled175c_button.h"
#include "rotary_input.h"
#include "rotary_ui.h"
#include "recon.h"
#include "wardrive.h"
#include "node_mode.h"
#include "voice_recorder.h"
#include "usb_drive.h"
#include <Wire.h>
#include <USB.h>
#include <Preferences.h>
#include <lvgl.h>
#include <atomic>
#include <math.h>
#include <time.h>
#include <esp_heap_caps.h>
#include <esp_ldo_regulator.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_cpu.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <hal/systimer_ll.h>
#include <soc/systimer_struct.h>
#include <soc/lp_system_reg.h>
#include <soc/soc.h>
#include <TouchDrvCSTXXX.hpp>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

#include "p4_pointer.h"
extern void uiP4ReadPointer(lv_indev_data_t*,p4ui::PointerFrame (*)());
bool p4CoprocSleep(); // p4_coproc.cpp: holds the radio chip in reset

namespace {
// ---------------------------------------------------------------- display
esp_ldo_channel_handle_t dsiPower=nullptr,ioPower=nullptr;
esp_lcd_dsi_bus_handle_t dsiBus=nullptr;
esp_lcd_panel_io_handle_t panelIo=nullptr; // DBI: commands and brightness
esp_lcd_panel_handle_t panel=nullptr;      // DPI: the video stream from PSRAM
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
volatile uint32_t flushes=0,flushTotalUs=0,flushMaxUs=0,flushErrors=0,flushStarted=0;
uint8_t panelId[3]{};
struct PanelCommand {uint8_t command,length;uint8_t data[4];uint16_t delayMs;};
// CO5300 MIPI start-up list (Waveshare/Espressif esp_lcd_co5300 2.0.2). CASET
// starts at column 6: the glass sits six columns into the controller's RAM.
constexpr PanelCommand panelStartup[]={
  {0x36,1,{0x00},0},{0x3A,1,{0x55},0},
  {0xFE,1,{0x20},0},{0xF4,1,{0x5A},0},{0xF5,1,{0x59},0},{0xFE,1,{0x80},0},{0x03,1,{0x00},0},{0xFE,1,{0x00},0},
  {0x35,1,{0x00},0},{0x53,1,{0x20},0},{0x51,1,{0x00},0},{0x63,1,{0xAB},0},
  {0x2A,4,{0x00,LCD_COL_OFFSET,uint8_t((LCD_COL_OFFSET+SCREEN_RES-1)>>8),uint8_t((LCD_COL_OFFSET+SCREEN_RES-1)&0xff)},0},
  {0x2B,4,{0x00,0x00,uint8_t((SCREEN_RES-1)>>8),uint8_t((SCREEN_RES-1)&0xff)},0},
  {0x11,0,{},400},{0x29,0,{},0},
};
// LVGL renders byte-swapped RGB565 for the SPI panels; the DPI frame buffer
// takes native little-endian RGB565. Swap in place, two pixels per word.
void swapPixels(lv_color_t* pixels,size_t count){
  if((uintptr_t(pixels)&3)==0){
    auto* word=reinterpret_cast<uint32_t*>(pixels);
    for(size_t i=0,n=count/2;i<n;++i){uint32_t v=word[i];word[i]=((v&0x00ff00ffu)<<8)|((v>>8)&0x00ff00ffu);}
    if(count&1){auto* last=reinterpret_cast<uint16_t*>(pixels)+count-1;*last=__builtin_bswap16(*last);}
  }else{auto* half=reinterpret_cast<uint16_t*>(pixels);for(size_t i=0;i<count;++i)half[i]=__builtin_bswap16(half[i]);}
}
bool IRAM_ATTR copyDone(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*){
  const uint32_t us=uint32_t(esp_timer_get_time())-flushStarted;
  ++flushes;flushTotalUs+=us;if(us>flushMaxUs)flushMaxUs=us;
  lv_disp_flush_ready(&displayDriver);return false;
}
void flush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels){
  swapPixels(pixels,size_t(area->x2-area->x1+1)*size_t(area->y2-area->y1+1));
  flushStarted=uint32_t(esp_timer_get_time());
  // DMA2D copies the tile into the frame buffer; copyDone releases LVGL's buffer.
  if(esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels)!=ESP_OK){++flushErrors;lv_disp_flush_ready(driver);}
}
void flushNowhere(lv_disp_drv_t* driver,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(driver);}
bool panelCommand(uint8_t command,const uint8_t* data,size_t length){
  return panelIo&&esp_lcd_panel_io_tx_param(panelIo,command,data,length)==ESP_OK;
}
bool displayBegin(){
  esp_ldo_channel_config_t ldo{};ldo.chan_id=LCD_DSI_LDO_CHANNEL;ldo.voltage_mv=LCD_DSI_LDO_MV;
  if(esp_ldo_acquire_channel(&ldo,&dsiPower)!=ESP_OK)return false;
  esp_lcd_dsi_bus_config_t bus{};bus.bus_id=0;bus.num_data_lanes=1;bus.phy_clk_src=MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT;bus.lane_bit_rate_mbps=480;
  if(esp_lcd_new_dsi_bus(&bus,&dsiBus)!=ESP_OK)return false;
  esp_lcd_dbi_io_config_t dbi{};dbi.virtual_channel=0;dbi.lcd_cmd_bits=8;dbi.lcd_param_bits=8;
  if(esp_lcd_new_panel_io_dbi(dsiBus,&dbi,&panelIo)!=ESP_OK)return false;
  esp_lcd_dpi_panel_config_t dpi{};
  dpi.virtual_channel=0;dpi.dpi_clk_src=MIPI_DSI_DPI_CLK_SRC_DEFAULT;dpi.dpi_clock_freq_mhz=16;
  dpi.pixel_format=LCD_COLOR_PIXEL_FORMAT_RGB565;dpi.in_color_format=LCD_COLOR_FMT_RGB565;dpi.out_color_format=LCD_COLOR_FMT_RGB565;
  dpi.num_fbs=1;dpi.flags.use_dma2d=1;
  dpi.video_timing.h_size=SCREEN_RES;dpi.video_timing.v_size=SCREEN_RES;
  dpi.video_timing.hsync_pulse_width=4;dpi.video_timing.hsync_back_porch=32;dpi.video_timing.hsync_front_porch=32;
  dpi.video_timing.vsync_pulse_width=4;dpi.video_timing.vsync_back_porch=12;dpi.video_timing.vsync_front_porch=18;
  if(esp_lcd_new_panel_dpi(dsiBus,&dpi,&panel)!=ESP_OK)return false;
  // Hardware reset, then the command list over DBI, then start the video stream.
  gpio_config_t reset{};reset.pin_bit_mask=1ULL<<LCD_RESET;reset.mode=GPIO_MODE_OUTPUT;gpio_config(&reset);
  gpio_set_level(gpio_num_t(LCD_RESET),1);delay(5);gpio_set_level(gpio_num_t(LCD_RESET),0);delay(10);gpio_set_level(gpio_num_t(LCD_RESET),1);delay(120);
  esp_lcd_panel_io_rx_param(panelIo,0x04,panelId,3);
  for(const auto& c:panelStartup){if(!panelCommand(c.command,c.length?c.data:nullptr,c.length))return false;if(c.delayMs)delay(c.delayMs);}
  esp_lcd_dpi_panel_event_callbacks_t callbacks{};callbacks.on_color_trans_done=copyDone;
  if(esp_lcd_dpi_panel_register_event_callbacks(panel,&callbacks,nullptr)!=ESP_OK)return false;
  return esp_lcd_panel_init(panel)==ESP_OK;
}

// ---------------------------------------------------------------- touch, PMU, buttons
TouchDrvCST92xx touch;
XPowersPMU power;
bool touchReady=false,powerReady=false,displayReady=false;
volatile bool touchIrq=false;
volatile uint32_t touchIrqs=0;
uint32_t touchReads=0,touchDowns=0,touchUps=0,touchTimeouts=0,touchMaxReadUs=0;
int lastX=0,lastY=0;
bool injected=false,injectedPress=false;int injectedX=0,injectedY=0;
void IRAM_ATTR onTouch(){touchIrq=true;++touchIrqs;}
p4ui::PointerFrame readPhysicalTouch(){
  static int16_t x[2],y[2];static bool pressed=false,was=false;static uint32_t lastReport=0;
  // The CST9217 sleeps between touches and NACKs polling: read only after INT.
  if(touchIrq){
    touchIrq=false;const uint32_t started=micros();++touchReads;
    if(touch.getPoint(x,y,1)>0){pressed=true;lastReport=millis();lastX=constrain(x[0],0,SCREEN_RES-1);lastY=constrain(y[0],0,SCREEN_RES-1);}
    else pressed=false;
    touchMaxReadUs=std::max(touchMaxReadUs,uint32_t(micros()-started));
  }else if(pressed&&millis()-lastReport>200){pressed=false;++touchTimeouts;}
  bool down=pressed;int px=lastX,py=lastY;
  if(injected){down=injectedPress;px=injectedX;py=injectedY;if(!injectedPress)injected=false;}
  if(down!=was){was=down;if(down)++touchDowns;else ++touchUps;}
  rotaryUiTouch(down);
  return {down,{px,py}};
}
void readTouch(lv_indev_drv_t*,lv_indev_data_t* data){
  uiP4ReadPointer(data,readPhysicalTouch);
}

// ---------------------------------------------------------------- haptic motor
// Spinning the motor disturbs this board enough that both SYSTIMER counters now
// and then jump back by seconds, which stalls the tick and trips the interrupt
// watchdog. While the motor runs (and for 200 ms after), a 500 us GPTimer
// interrupt writes any counter that went backwards forward again from the CPU
// cycle count. The same interrupt cuts the motor after 500 ms on.
gptimer_handle_t guardTimer=nullptr;
volatile bool guardRunning=false,motorOn=false;
volatile uint32_t guardRepairs=0,motorTicks=0,motorCutoffs=0;
uint64_t guardLast[2]{};uint32_t guardCycles=0;bool guardPrimed=false;uint32_t cpuMhz=400;
constexpr uint32_t MotorLimitTicks=1000; // 500 ms in 500 us steps
inline void IRAM_ATTR motorPins(bool in1,bool in2){gpio_set_level(gpio_num_t(HAPTIC_IN1_PIN),in1);gpio_set_level(gpio_num_t(HAPTIC_IN2_PIN),in2);}
uint64_t IRAM_ATTR systimerRead(uint32_t id){
  systimer_ll_counter_snapshot(&SYSTIMER,id);
  while(!systimer_ll_is_counter_value_valid(&SYSTIMER,id)){}
  uint32_t lo=systimer_ll_get_counter_value_low(&SYSTIMER,id),hi=systimer_ll_get_counter_value_high(&SYSTIMER,id);
  return (uint64_t(hi)<<32)|lo;
}
bool IRAM_ATTR guardTick(gptimer_handle_t,const gptimer_alarm_event_data_t*,void*){
  const uint32_t cycles=esp_cpu_get_cycle_count();
  const uint64_t elapsed=uint64_t(uint32_t(cycles-guardCycles))*16/cpuMhz; // SYSTIMER runs at 16 MHz
  guardCycles=cycles;
  for(uint32_t id=0;id<2;++id){
    uint64_t value=systimerRead(id);
    if(guardPrimed&&value<guardLast[id]){
      value=guardLast[id]+elapsed;
      systimer_ll_set_counter_value(&SYSTIMER,id,value);systimer_ll_apply_counter_value(&SYSTIMER,id);++guardRepairs;
    }
    guardLast[id]=value;
  }
  guardPrimed=true;
  if(motorOn&&++motorTicks>MotorLimitTicks){motorOn=false;motorPins(false,false);++motorCutoffs;}
  return false;
}
void guardStart(){
  if(!guardTimer||guardRunning)return;
  guardPrimed=false;guardCycles=esp_cpu_get_cycle_count();
  if(gptimer_start(guardTimer)==ESP_OK)guardRunning=true;
}
void guardStop(){if(guardTimer&&guardRunning&&gptimer_stop(guardTimer)==ESP_OK)guardRunning=false;}
struct HapticStep {uint16_t onMs,brakeMs,pauseMs;};
struct HapticPattern {uint8_t count;HapticStep steps[3];};
QueueHandle_t hapticQueue=nullptr;
std::atomic<bool> hapticBusy{false};
uint32_t hapticPlays=0;
constexpr HapticStep knobClicks[]={{22,12,0},{35,15,0},{50,18,0}}; // light, medium, strong
uint8_t knobClickLevel=2; // 0 off, 1..3
Preferences hapticPrefs;
void hapticTask(void*){
  HapticPattern pattern;
  for(;;){
    if(xQueueReceive(hapticQueue,&pattern,portMAX_DELAY)!=pdTRUE)continue;
    guardStart();
    do{
      hapticBusy=true;++hapticPlays;
      for(unsigned i=0;i<pattern.count&&i<3;++i){
        const auto& s=pattern.steps[i];
        motorTicks=0;motorOn=true;motorPins(true,false);vTaskDelay(pdMS_TO_TICKS(std::min<uint16_t>(s.onMs,500)));
        motorOn=false;if(s.brakeMs){motorPins(true,true);vTaskDelay(pdMS_TO_TICKS(s.brakeMs));}
        motorPins(false,false);if(s.pauseMs)vTaskDelay(pdMS_TO_TICKS(s.pauseMs));
      }
      hapticBusy=false;
    }while(xQueueReceive(hapticQueue,&pattern,pdMS_TO_TICKS(200))==pdTRUE); // the guard stays up for a 200 ms tail
    guardStop();
  }
}
void hapticPlay(const HapticPattern& pattern){if(hapticQueue&&!hapticBusy)xQueueSend(hapticQueue,&pattern,0);}
void hapticBegin(){
  gpio_config_t out{};out.pin_bit_mask=(1ULL<<HAPTIC_IN1_PIN)|(1ULL<<HAPTIC_IN2_PIN);out.mode=GPIO_MODE_OUTPUT;gpio_config(&out);motorPins(false,false);
  cpuMhz=std::max<uint32_t>(1,getCpuFrequencyMhz());
  gptimer_config_t timer{};timer.clk_src=GPTIMER_CLK_SRC_DEFAULT;timer.direction=GPTIMER_COUNT_UP;timer.resolution_hz=1000000;
  if(gptimer_new_timer(&timer,&guardTimer)==ESP_OK){
    gptimer_event_callbacks_t callbacks{};callbacks.on_alarm=guardTick;
    gptimer_alarm_config_t alarm{};alarm.alarm_count=500;alarm.reload_count=0;alarm.flags.auto_reload_on_alarm=true;
    if(gptimer_register_event_callbacks(guardTimer,&callbacks,nullptr)!=ESP_OK||gptimer_set_alarm_action(guardTimer,&alarm)!=ESP_OK||gptimer_enable(guardTimer)!=ESP_OK){gptimer_del_timer(guardTimer);guardTimer=nullptr;}
  }
  hapticPrefs.begin("dd-haptic",false);knobClickLevel=std::min<uint8_t>(hapticPrefs.getUChar("knob",2),3);
  hapticQueue=xQueueCreate(2,sizeof(HapticPattern));
  // Without the guard the motor would risk a watchdog reset: keep it still.
  if(!guardTimer||!hapticQueue||xTaskCreatePinnedToCore(hapticTask,"dd-haptic",2560,nullptr,5,nullptr,0)!=pdPASS){if(hapticQueue)vQueueDelete(hapticQueue);hapticQueue=nullptr;}
}

// ---------------------------------------------------------------- knob
// Two independent active-low switches, one pulse per detent. A level that
// holds for 2 ms is real; a step counts once, on the change to a confirmed LOW.
constexpr uint8_t knobPins[2]={KNOB_CW_PIN,KNOB_CCW_PIN};
volatile uint32_t knobEdgeUs[2]{}; // low 32 bits of esp_timer time
TaskHandle_t knobTaskHandle=nullptr;
portMUX_TYPE knobMux=portMUX_INITIALIZER_UNLOCKED;
volatile int knobSteps=0;volatile uint32_t knobEdges=0,knobDetents=0,knobLastAt=0;
int knobPosition=0;
void ARDUINO_ISR_ATTR knobEdge(void* arg){
  knobEdgeUs[uintptr_t(arg)]=uint32_t(esp_timer_get_time());++knobEdges;
  BaseType_t woken=pdFALSE;if(knobTaskHandle)vTaskNotifyGiveFromISR(knobTaskHandle,&woken);portYIELD_FROM_ISR(woken);
}
void knobTask(void*){
  bool low[2]={false,false};
  for(;;){
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    for(;;){
      const uint32_t now=uint32_t(esp_timer_get_time());int32_t wait=0;int steps=0;
      for(unsigned i=0;i<2;++i){
        const int32_t held=int32_t(now-knobEdgeUs[i]);
        if(held<2000){wait=std::max<int32_t>(wait,held<0?2000:2000-held);continue;}
        const bool pressed=gpio_get_level(gpio_num_t(knobPins[i]))==0;
        if(pressed&&!low[i])steps+=i?-1:1;
        low[i]=pressed;
      }
      if(steps){
        portENTER_CRITICAL(&knobMux);knobSteps=constrain(knobSteps+steps,-48,48);knobDetents+=abs(steps);knobLastAt=millis();portEXIT_CRITICAL(&knobMux);
        if(knobClickLevel){HapticPattern click{1,{knobClicks[knobClickLevel-1]}};hapticPlay(click);}
      }
      if(!wait)break;
      ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(uint32_t(wait+999)/1000));
    }
  }
}
void knobBegin(){
  for(unsigned i=0;i<2;++i){pinMode(knobPins[i],INPUT_PULLUP);knobEdgeUs[i]=uint32_t(esp_timer_get_time())-10000;}
  if(xTaskCreatePinnedToCore(knobTask,"dd-knob",3072,nullptr,4,&knobTaskHandle,0)!=pdPASS)return;
  for(unsigned i=0;i<2;++i)attachInterruptArg(knobPins[i],knobEdge,(void*)uintptr_t(i),CHANGE);
}

// ---------------------------------------------------------------- RTC (PCF85063, UTC, 24 h)
bool rtcRead(uint8_t reg,uint8_t* data,size_t count){
  Wire.beginTransmission(0x51);Wire.write(reg);if(Wire.endTransmission(false)!=0)return false;
  if(Wire.requestFrom(uint8_t(0x51),uint8_t(count))!=count)return false;
  for(size_t i=0;i<count;++i)data[i]=Wire.read();return true;
}
bool rtcWrite(uint8_t reg,const uint8_t* data,size_t count){Wire.beginTransmission(0x51);Wire.write(reg);Wire.write(data,count);return Wire.endTransmission()==0;}
int decodeBcd(uint8_t v){return (v&15)<=9&&(v>>4)<=9?(v>>4)*10+(v&15):-1;}

// ---------------------------------------------------------------- ES8389 DAC (I2C 0x10)
constexpr uint8_t CodecAddress=0x10;
struct CodecWrite {uint8_t reg,value;};
// esp_codec_dev's ES8389 start-up, 16-bit I2S slave with MCLK from the P4 (256 fs).
constexpr CodecWrite codecStartup[]={
  {0xF3,0x00},{0x00,0x7E},{0xF3,0x38},{0x24,0x64},{0x25,0x04},{0x45,0x03},{0x60,0x2A},{0x61,0xC9},{0x62,0x4F},{0x63,0x06},
  {0x6B,0x00},{0x6D,0x16},{0x6E,0xAA},{0x6F,0x66},{0x70,0x99},{0x23,0x00},{0x72,0x10},{0x73,0x10},{0x10,0xC4},{0x01,0x08},
  {0xF1,0x00},{0x12,0x01},{0x13,0x01},{0x14,0x01},{0x15,0x01},{0x16,0x35},{0x17,0x09},{0x18,0x91},{0x19,0x28},{0x1A,0x01},
  {0x1B,0x01},{0x1C,0x11},{0x2A,0x00},{0x20,0x60},{0x40,0x60},{0xF0,0x01},{0x02,0x00},{0x04,0x00},{0x05,0x10},{0x06,0x00},
  {0x07,0xC0},{0x08,0x00},{0x09,0xC0},{0x0A,0x80},{0x0B,0x04},{0x0C,0x01},{0x0D,0x00},{0x0F,0x10},{0x21,0x1F},{0x22,0x7F},
  {0x2F,0xC0},{0x30,0xF4},{0x31,0x00},{0x44,0x00},{0x41,0x7F},{0x42,0x7F},{0x43,0x10},{0x49,0x0F},{0x4C,0xC0},{0x00,0x00},
  {0x03,0xC1},{0x00,0x01},{0x4D,0x00},{0x26,0xBF},{0x27,0xBF},{0x28,0xBF},{0x46,0xBF},{0x47,0xBF},{0x48,0xBE},
};
bool codecWrite(uint8_t reg,uint8_t value){Wire.beginTransmission(CodecAddress);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0;}
bool codecRead(uint8_t reg,uint8_t& value){
  Wire.beginTransmission(CodecAddress);Wire.write(reg);
  if(Wire.endTransmission(false)!=0||Wire.requestFrom(CodecAddress,uint8_t(1))!=1)return false;
  value=Wire.read();return true;
}
bool codecUpdate(uint8_t reg,uint8_t mask,uint8_t value){uint8_t v=0;return codecRead(reg,v)&&codecWrite(reg,(v&~mask)|(value&mask));}
bool codecReady=false;

// ---------------------------------------------------------------- ES7210 microphones (I2C 0x40)
// I2S slave, 16-bit Philips, MCLK 4.096 MHz from the P4 (256 fs at 16 kHz),
// MIC1 left and MIC2 right on SDOUT1, 24 dB PGA, high-pass filters on. The
// knob has no MIC3/MIC4: their power and inputs stay off.
constexpr uint8_t MicAddress=0x40;
constexpr CodecWrite micStartup[]={
  {0x00,0xFF},{0x00,0x41},{0x01,0x3F},{0x09,0x30},{0x0A,0x30},{0x23,0x2A},{0x22,0x0A},{0x20,0x0A},{0x21,0x2A},
  {0x40,0x43},{0x41,0x70},{0x42,0x70},{0x07,0x20},{0x02,0xC1},{0x01,0x34},{0x4B,0x00},{0x4C,0xFF},
  {0x43,0x18},{0x44,0x18},{0x45,0x00},{0x46,0x00},{0x12,0x00},{0x11,0x60},
};
// Power up: clocks for ADC 1/2, analog on, per-microphone power, then a state-machine restart.
constexpr CodecWrite micRun[]={
  {0x01,0x34},{0x06,0x00},{0x40,0x43},{0x47,0x08},{0x48,0x08},{0x49,0x08},{0x4A,0x08},{0x4B,0x00},{0x4C,0xFF},
  {0x43,0x18},{0x44,0x18},{0x40,0x43},{0x00,0x71},{0x00,0x41},
};
// Everything off: microphone power, bias and analog, all clocks.
constexpr CodecWrite micOff[]={
  {0x47,0xFF},{0x48,0xFF},{0x49,0xFF},{0x4A,0xFF},{0x4B,0xFF},{0x4C,0xFF},{0x40,0xC0},{0x01,0x7F},{0x06,0x07},
};
bool micWrite(uint8_t reg,uint8_t value){Wire.beginTransmission(MicAddress);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0;}
bool micRead(uint8_t reg,uint8_t& value){
  Wire.beginTransmission(MicAddress);Wire.write(reg);
  if(Wire.endTransmission(false)!=0||Wire.requestFrom(MicAddress,uint8_t(1))!=1)return false;
  value=Wire.read();return true;
}
bool micReady=false;uint32_t micStarts=0,micFailures=0;

// ---------------------------------------------------------------- battery and keys
int battery=-1;uint32_t batteryMv=0;bool charging=false;
dragon::RotaryButton bootKey;
dragon::Amoled175cButton pwrKey;
uint32_t bootClicks=0,bootHolds=0,pwrTaps=0,pwrHolds=0;
uint32_t lastAlert=0,lastAlertBuzz=0;
}

// ---------------------------------------------------------------- public hooks
void set_brightness(uint8_t value){
  if(!displayReady)return;uint8_t level=uint16_t(constrain(value,0,100))*255/100;
  panelCommand(0x51,&level,1);
}
void scr_lvgl_init(){
  displayReady=displayBegin();
  Serial.printf("DISPLAY: CO5300 DSI %dx%d %s id=%02x%02x%02x\n",SCREEN_RES,SCREEN_RES,displayReady?"ready":"FAILED",panelId[0],panelId[1],panelId[2]);
  // Keep a contiguous 32 KiB DMA region available for the SDIO receiver.
  // LVGL can render twelve rows into one tile while DMA2D copies the other.
  constexpr size_t rows=12,pixels=SCREEN_RES*rows;
  auto* first=(lv_color_t*)heap_caps_aligned_alloc(64,pixels*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  auto* second=(lv_color_t*)heap_caps_aligned_alloc(64,pixels*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(!first){Serial.println("FATAL display buffer");abort();}
  lv_init();lv_disp_draw_buf_init(&drawBuffer,first,second,pixels);
  lv_disp_drv_init(&displayDriver);displayDriver.hor_res=SCREEN_RES;displayDriver.ver_res=SCREEN_RES;
  displayDriver.flush_cb=displayReady?flush:flushNowhere;
  displayDriver.draw_buf=&drawBuffer;lv_disp_drv_register(&displayDriver);
  touch.setPins(TP_RESET,TP_INT);
  touchReady=touch.begin(Wire,TP_I2C_ADDR,IIC_SDA,IIC_SCL);
  if(touchReady){touch.setMaxCoordinates(SCREEN_RES,SCREEN_RES);touch.setMirrorXY(true,true);pinMode(TP_INT,INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(TP_INT),onTouch,FALLING);}
  Serial.printf("TOUCH: %s\n",touchReady?"CST9217 ready":"CST9217 not found; the knob and keys still work");
  static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=readTouch;lv_indev_drv_register(&input);
  rotaryUiBegin();
}
void p4KnobUsbBegin();
void boardBegin(){
  // The web flasher's reset over USB starts download mode (see lineState).
  p4KnobUsbBegin();
  // Pins held through deep sleep stay held after the wake until released here.
  for(int pin:{HAPTIC_IN1_PIN,HAPTIC_IN2_PIN,AUDIO_MUTE_PIN,COPROC_EN_PIN})gpio_hold_dis(gpio_num_t(pin));
  // The P4's own IO supply for the TF card and the radio chip's SDIO link. The
  // board owns it, so neither the card driver nor the radio link acquires it.
  esp_ldo_channel_config_t io{};io.chan_id=SD_IO_LDO_CHANNEL;io.voltage_mv=3300;
  if(esp_ldo_acquire_channel(&io,&ioPower)!=ESP_OK)Serial.println("BOARD: LDO4 unavailable; TF card and radio chip may not start");
  pinMode(AUDIO_MUTE_PIN,OUTPUT);digitalWrite(AUDIO_MUTE_PIN,LOW);
  pinMode(BTN_PIN,INPUT_PULLUP);pinMode(PWR_KEY_PIN,INPUT);pinMode(RTC_INT_PIN,INPUT_PULLUP);
  Wire.begin(IIC_SDA,IIC_SCL,400000);Wire.setTimeOut(20);
  // Read-only PMU use: IRQ and ADC set-up, never a rail.
  powerReady=power.begin(Wire,AXP2101_SLAVE_ADDRESS,IIC_SDA,IIC_SCL);
  if(powerReady){power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);power.clearIrqStatus();power.enableBattDetection();power.enableBattVoltageMeasure();power.enableVbusVoltageMeasure();}
  hapticBegin();knobBegin();
  Serial.printf("BOARD Waveshare P4 Knob 466x466 / P4 rev %u / flash %u / psram %u / pmu=%u haptic=%u wake=%d\n",
    unsigned(ESP.getChipRevision()),ESP.getFlashChipSize(),ESP.getPsramSize(),powerReady,hapticQueue!=nullptr,int(esp_sleep_get_wakeup_cause()));
}
void boardLoop(){
  const uint32_t now=millis();
  portENTER_CRITICAL(&knobMux);
  int steps=knobSteps;
  const uint32_t age=now-knobLastAt;
  knobSteps=0;
  portEXIT_CRITICAL(&knobMux);
  if(steps && age>350 && !uiP4ClockOwnsDial())steps=0;
  knobPosition+=steps;
  bool click=false,back=false;
  switch(bootKey.update(digitalRead(BTN_PIN)==LOW,now)){
    case dragon::RotaryPress::Click:click=true;++bootClicks;break;
    case dragon::RotaryPress::Back:back=true;++bootHolds;break;
    default:break;
  }
  rotaryUiInput(steps,click,back);
  voiceLoop(); // microphone power follows the recorder; a stop says why
  switch(pwrKey.update(digitalRead(PWR_KEY_PIN)==HIGH,now)){
    case dragon::Amoled175cButtonEvent::Tap:++pwrTaps;uiAmoled175cKey(true);break;
    case dragon::Amoled175cButtonEvent::Sleep:++pwrHolds;uiAmoled175cSleep();break;
    default:break;
  }
  // A new alert buzzes twice, at most every 10 s, unless quiet mode is on.
  if(alertTotal!=lastAlert){lastAlert=alertTotal;
    if(alertVibration&&!detector.quiet&&(!lastAlertBuzz||now-lastAlertBuzz>10000)){lastAlertBuzz=now;HapticPattern buzz{2,{{120,20,90},{120,20,0}}};hapticPlay(buzz);}
  }
  static uint32_t batteryAt=0;
  if(powerReady&&(!batteryAt||now-batteryAt>=5000)){batteryAt=now;
    const bool present=power.isBatteryConnect();batteryMv=present?power.getBattVoltage():0;
    battery=present?power.getBatteryPercent():-1;charging=present&&power.isCharging();
  }
}
int batteryPercent(){return battery;}
bool batteryCharging(){return charging;}
void testTouch(int x,int y,bool down){injected=true;injectedX=x;injectedY=y;injectedPress=down;}
// Settings: knob click strength, 0 off .. 3 strong.
uint8_t p4KnobClickLevel(){return knobClickLevel;}
void p4KnobSetClickLevel(uint8_t level){knobClickLevel=std::min<uint8_t>(level,3);hapticPrefs.putUChar("knob",knobClickLevel);
  if(knobClickLevel){HapticPattern click{1,{knobClicks[knobClickLevel-1]}};hapticPlay(click);}}
const char* p4KnobClickName(){static const char* names[]={"Off","Light","Medium","Strong"};return names[knobClickLevel];}
void p4KnobBuzz(){HapticPattern buzz{1,{{120,20,0}}};hapticPlay(buzz);}

bool p4KnobCodecBegin(){
  // I2S (tone_output.cpp) already drives MCLK, so the codec can lock to it.
  codecReady=false;
  for(const auto& w:codecStartup)if(!codecWrite(w.reg,w.value))return false;
  // External MCLK, I2S slave timing, no internal DAC loopback (capture is the ES7210).
  if(!codecUpdate(0x01,0x01,0x00)||!codecUpdate(0x02,0xC3,0x00)||!codecUpdate(0x23,0x80,0x00)||!codecWrite(0xF0,0x12))return false;
  // DAC volume: -6 dB at the speaker, after the 3.3 V DAC / 5 V amplifier ratio.
  const float codecDb=-6.0f-20.0f*log10f(3.3f/5.0f);const uint8_t raw=uint8_t(constrain(int((codecDb+95.5f)*255.0f/127.5f),0,255));
  if(!codecWrite(0x46,raw)||!codecWrite(0x47,raw))return false;
  // Bias on and unmute; the amplifier enable (GPIO53) is left to tone_output.cpp.
  codecReady=codecWrite(0x4D,0x00)&&codecUpdate(0x69,0x20,0x20)&&codecWrite(0x61,0xD9)&&codecWrite(0x64,0x8F)&&codecWrite(0x10,0xE4)&&
    codecWrite(0x00,0x01)&&codecWrite(0x03,0xC3)&&codecWrite(0x24,0x6A)&&codecWrite(0x25,0x0A)&&codecUpdate(0x40,0x03,0x00);
  return codecReady;
}
// Main loop only: the ES7210 shares I2C with touch, the PMU and the RTC. MCLK,
// BCLK and WS already run (tone_output.cpp keeps the transmitter on).
bool p4KnobMicBegin(){
  micReady=false;++micStarts;
  for(const auto& w:micStartup)if(!micWrite(w.reg,w.value)){++micFailures;return false;}
  uint8_t mode=0;if(!micRead(0x08,mode)||!micWrite(0x08,mode&~0x01)){++micFailures;return false;} // bit 0 clear: I2S slave
  for(const auto& w:micRun)if(!micWrite(w.reg,w.value)){++micFailures;return false;}
  micReady=true;return true;
}
void p4KnobMicEnd(){for(const auto& w:micOff)micWrite(w.reg,w.value);micReady=false;}
static void codecQuiet(){if(codecReady){codecUpdate(0x40,0x03,0x03);codecUpdate(0x61,0x20,0x00);codecWrite(0x10,0xD4);}digitalWrite(AUDIO_MUTE_PIN,LOW);}

static void panelSleep(){if(displayReady){panelCommand(0x28,nullptr,0);delay(20);panelCommand(0x10,nullptr,0);delay(120);}}
static void panelWake(){if(displayReady){panelCommand(0x11,nullptr,0);delay(120);panelCommand(0x29,nullptr,0);}}
bool p4KnobSleep(unsigned seconds){
  // PWR reaches GPIO2 through an inverter: high while pressed. Wait for release.
  // Wait for radio initialization and for the PC to release mounted storage.
  if(p4CoprocBusy()||reconRecording()||reconStorageBusy()||wardriveBusy()||nodeBusy()||voiceRecording()||usbDriveOn()||digitalRead(PWR_KEY_PIN)==HIGH)return false;
  missionStop();if(missionActive())return false;
  if(!voiceShutdown(1000))return false; // a playing file closes first
    audioSave();
  extern bool watcherCloseStorage();if(!watcherCloseStorage())return false;
  if(esp_sleep_enable_ext1_wakeup_io(1ULL<<PWR_KEY_PIN,ESP_EXT1_WAKEUP_ANY_HIGH)!=ESP_OK)return false;
  if(seconds&&esp_sleep_enable_timer_wakeup(uint64_t(seconds)*1000000)!=ESP_OK){esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);return false;}
  if(!p4CoprocSleep()){esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);return false;}
  audioStop();codecQuiet();
  if(touchReady){detachInterrupt(TP_INT);touch.sleep();}
  for(uint8_t pin:knobPins)detachInterrupt(pin);
  motorPins(false,false);gpio_hold_en(gpio_num_t(HAPTIC_IN1_PIN));gpio_hold_en(gpio_num_t(HAPTIC_IN2_PIN));
  gpio_hold_en(gpio_num_t(AUDIO_MUTE_PIN));

  panelSleep();
  visitsPersist(); // devices still here after a short sleep do not alert again
  Serial.printf("P4KNOB_SLEEP epoch=%lu timer=%u\n",wallTime(),seconds);Serial.flush();delay(50);
  esp_deep_sleep_start();return true;
}
bool p4KnobPowerOff(){
  if(!powerReady||p4CoprocBusy()||usbDriveOn())return false;   // not mid radio-chip set-up; the PC may have the card
  missionStop();if(missionActive())return false;
  extern bool watcherCloseStorage();bool wasLogging=loggingEnabled;
  if(!voiceShutdown(3000))return false; // a recording saves its last samples and its final sizes
    audioSave();
  if(!watcherCloseStorage())return false;
  if(!p4CoprocSleep())return false;
  audioStop();codecQuiet();panelSleep();
  Serial.println("P4KNOB_POWER_OFF");Serial.flush();delay(50);
  power.shutdown();
  delay(1000); // USB power can keep the board running
  // USB power kept the board alive. Restart Dragon to reinitialize the radio.
  (void)wasLogging;Serial.println("P4KNOB_POWER_STILL_ON USB power keeps the knob on; restarting");Serial.flush();delay(50);
  esp_restart();return false;
}
// The web flasher and esptool use DTR/RTS to ask for download mode. The P4's
// USB-OTG port has no auto-reset circuit, so this firmware restarts into the
// ROM loader itself (the same request as holding BOOT at power-on).
void p4KnobDownloadMode(){
  Serial.println("P4KNOB_DOWNLOAD_MODE");Serial.flush();delay(50);
  REG_SET_BIT(LP_SYSTEM_REG_SYS_CTRL_REG,LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT);esp_restart();
}
static void lineState(void*,esp_event_base_t,int32_t id,void* data){
  static uint8_t stage=0;
  if(id==ARDUINO_USB_CDC_LINE_CODING_EVENT){auto* e=static_cast<arduino_usb_cdc_event_data_t*>(data);if(e->line_coding.bit_rate==1200)p4KnobDownloadMode();return;}
  if(id!=ARDUINO_USB_CDC_LINE_STATE_EVENT)return;
  auto* e=static_cast<arduino_usb_cdc_event_data_t*>(data);const bool dtr=e->line_state.dtr,rts=e->line_state.rts;
  // esptool's classic reset: (DTR0,RTS1) -> (DTR1,RTS1) -> (DTR1,RTS0) -> (DTR0,RTS0).
  if(!dtr&&rts)stage=1;
  else if(dtr&&rts)stage=stage==1?2:0;
  else if(dtr&&!rts)stage=stage==2?3:0;
  else {if(stage==3)p4KnobDownloadMode();stage=0;}
}
// USB drive mode (usb_drive.cpp): 1 while USB-C has power, 0 without, -1 unknown.
int p4KnobUsbPower(){return powerReady?int(power.isVbusIn()):-1;}
void p4KnobUsbBegin(){Serial.enableReboot(false);Serial.onEvent(ARDUINO_USB_CDC_LINE_STATE_EVENT,lineState);Serial.onEvent(ARDUINO_USB_CDC_LINE_CODING_EVENT,lineState);}

void p4KnobStatus(){
  Serial.printf("P4KNOB_STATUS display=%u touch=%u pmu=%u codec=%u ldo4=%u sd=%u rtc=%u battery=%d battery_mv=%lu charging=%u psram_free=%u internal_free=%u mic=%u mic_starts=%lu mic_failures=%lu\n",
    displayReady,touchReady,powerReady,codecReady,ioPower!=nullptr,sdReady,rtcAvailable,battery,(unsigned long)batteryMv,charging,ESP.getFreePsram(),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),micReady,(unsigned long)micStarts,(unsigned long)micFailures);
  Serial.printf("ROTARY_STATUS position=%d edges=%lu detents=%lu boot_clicks=%lu boot_holds=%lu pwr_taps=%lu pwr_holds=%lu\n",
    knobPosition,(unsigned long)knobEdges,(unsigned long)knobDetents,(unsigned long)bootClicks,(unsigned long)bootHolds,(unsigned long)pwrTaps,(unsigned long)pwrHolds);
  Serial.printf("TOUCH_STATUS enabled=%u irq=%lu reads=%lu downs=%lu ups=%lu timeouts=%lu read_max_us=%lu xy=%d,%d\n",
    touchReady,(unsigned long)touchIrqs,(unsigned long)touchReads,(unsigned long)touchDowns,(unsigned long)touchUps,(unsigned long)touchTimeouts,(unsigned long)touchMaxReadUs,lastX,lastY);
  Serial.printf("HAPTIC_STATUS ready=%u level=%s plays=%lu guard_repairs=%lu cutoffs=%lu\n",hapticQueue!=nullptr,p4KnobClickName(),(unsigned long)hapticPlays,(unsigned long)guardRepairs,(unsigned long)motorCutoffs);
  Serial.printf("DISPLAY_STATUS frames=%lu flush_total_us=%lu flush_max_us=%lu errors=%lu resolution=%ux%u\n",(unsigned long)flushes,(unsigned long)flushTotalUs,(unsigned long)flushMaxUs,(unsigned long)flushErrors,SCREEN_RES,SCREEN_RES);
}

void boardSensorsBegin(){
  uint8_t control=0;rtcAvailable=false;
  for(unsigned attempt=0;attempt<3&&!rtcAvailable;++attempt){rtcAvailable=rtcRead(0,&control,1);if(!rtcAvailable)delay(5);}
}
bool boardReadRtc(uint32_t& epoch){
  uint8_t control=0,b[7]{};if(!rtcAvailable||!rtcRead(0,&control,1)||(control&0xa2)||!rtcRead(4,b,7)||(b[0]&0x80))return false;
  dragon::Calendar d{2000+decodeBcd(b[6]),decodeBcd(b[5]&31),decodeBcd(b[3]&63),decodeBcd(b[2]&63),decodeBcd(b[1]&127),decodeBcd(b[0]&127)};
  epoch=dragon::dateEpoch(d);return epoch!=0;
}
bool boardWriteRtc(uint32_t epoch){
  if(!rtcAvailable||!dragon::validEpoch(epoch))return false;time_t value=epoch;struct tm t{};gmtime_r(&value,&t);
  auto bcd=[](int v){return uint8_t((v/10)*16+v%10);};
  // Leave external-clock test and 12 h modes; keep the oscillator load setting.
  uint8_t control=0;if(!rtcRead(0,&control,1))return false;control=(control&0x05)|0x20;
  if(!rtcWrite(0,&control,1))return false;
  uint8_t b[]={bcd(t.tm_sec),bcd(t.tm_min),bcd(t.tm_hour),bcd(t.tm_mday),uint8_t(t.tm_wday),bcd(t.tm_mon+1),bcd(t.tm_year%100)};
  bool written=rtcWrite(4,b,7);control&=~0x20;bool restarted=rtcWrite(0,&control,1);
  uint32_t readback=0;return written&&restarted&&boardReadRtc(readback)&&readback>=epoch&&readback-epoch<=1;
}
void boardRtcDiagnostics(){
  uint8_t control=0,seconds=0;bool ok=rtcRead(0,&control,1)&&rtcRead(4,&seconds,1);
  Serial.printf("CLOCK_DIAGNOSTICS chip=PCF85063 io=%u control=%02x lost=%u\n",ok,control,(seconds&0x80)!=0);
}
bool p4RadioRailReady(){return ioPower!=nullptr;}
#endif
