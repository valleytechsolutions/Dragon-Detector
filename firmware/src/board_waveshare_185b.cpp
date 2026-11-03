// Dragon Detector integration for Waveshare ESP32-S3-Touch-LCD-1.85B Rev1.1.
// ST77916 initialization data retain their Waveshare Apache-2.0 provenance.
#include "board_pins.h"
#ifdef BOARD_WAVESHARE_185B
#include "app.h"
#include "board_waveshare_185b.h"
#include "waveshare_185b_lcd_init.h"
#include "waveshare_185b_touch.h"
#include "waveshare_185b_button.h"
#include <Wire.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <freertos/queue.h>
#include <atomic>
#include <time.h>

extern bool uiTouch(bool, int, int);
namespace {
constexpr uint8_t RtcAddress = 0x51, GaugeAddress = 0x55;
constexpr int LcdTePin = 18;
SemaphoreHandle_t busMutex = nullptr;
esp_lcd_panel_io_handle_t panelIo = nullptr;
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_indev_t* touchDevice = nullptr;
uint8_t panelId[4]{}, panelVersion = 0, touchChip = 0;
bool touchReady = false, inputTaskReady = false, backlightReady = false;
volatile bool touchIrq = false;
std::atomic<uint32_t> i2cErrors{0}, touchSamples{0}, touchErrors{0};
std::atomic<uint32_t> touchPresses{0}, inputDrops{0}, buttonPresses{0};
std::atomic<bool> touchResync{false};
std::atomic<bool> timerInputMode{false};
volatile uint32_t flushes = 0, flushErrors = 0;
struct TouchEvent { bool pressed; uint16_t x, y; uint32_t queuedAt; bool validSample; };
using KeyEvent = waveshare185b::BootEvent;
QueueHandle_t touchQueue = nullptr, keyQueue = nullptr;
portMUX_TYPE inputMux = portMUX_INITIALIZER_UNLOCKED;
TouchEvent latestTouch{};
waveshare185b::TouchState physicalTouch;
waveshare185b::TouchAgeGuard touchAgeGuard;
waveshare185b::BootClicks bootClicks;
int battery = -1;
uint16_t batteryMv = 0;
bool charging = false, gaugePresent = false;
uint32_t batteryGoodAt = 0, batteryAttemptAt = 0;

void IRAM_ATTR onTouch() { touchIrq = true; }
bool IRAM_ATTR flushComplete(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  ++flushes;
  lv_disp_flush_ready(&displayDriver);
  return false;
}
esp_err_t lcdCommand(uint8_t command, const void* data, size_t size) {
  return esp_lcd_panel_io_tx_param(panelIo, (0x02u << 24) | (uint32_t(command) << 8), data, size);
}
void flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* pixels) {
  const uint8_t cols[] = {uint8_t(area->x1 >> 8), uint8_t(area->x1), uint8_t(area->x2 >> 8), uint8_t(area->x2)};
  const uint8_t rows[] = {uint8_t(area->y1 >> 8), uint8_t(area->y1), uint8_t(area->y2 >> 8), uint8_t(area->y2)};
  esp_err_t result = lcdCommand(0x2a, cols, sizeof(cols));
  if (result == ESP_OK) result = lcdCommand(0x2b, rows, sizeof(rows));
  // LV_COLOR_16_SWAP=1 already supplies big-endian RGB565. DMA owns this
  // buffer until flushComplete; do not swap it or release it early.
  if (result == ESP_OK) result = esp_lcd_panel_io_tx_color(panelIo, 0x32002c00, pixels,
      size_t(area->x2 - area->x1 + 1) * size_t(area->y2 - area->y1 + 1) * 2);
  if (result != ESP_OK) { ++flushErrors; lv_disp_flush_ready(disp); }
}
void queueTouch(const TouchEvent& event) {
  if (xQueueSend(touchQueue, &event, 0) != pdTRUE) {
    // A truncated press/release sequence is not safe to replay. The UI thread
    // cancels its current gesture and resynchronizes against a fresh release.
    touchResync.store(true);
    xQueueReset(touchQueue);
    ++inputDrops;
  }
}
void queueKey(const KeyEvent& event) {
  if (xQueueSend(keyQueue, &event, 0) != pdTRUE) {
    xQueueReset(keyQueue);
    ++inputDrops;
  }
}
void pollInputs() {
  const uint32_t now = millis();
  if (touchReady) {
    // Auto-sleep is disabled while the MCU is running. Polling also recovers a
    // missed edge; finger count, rather than gesture flags, defines release.
    uint8_t packet[6]{};
    touchIrq = false;
    bool ok = waveshare185bI2cRead(TP_I2C_ADDR, 0x01, packet, sizeof(packet));
    bool was = physicalTouch.pressed;
    const bool validSample = physicalTouch.update(packet, sizeof(packet), ok, now);
    if (validSample) ++touchSamples;
    else ++touchErrors;
    TouchEvent event{physicalTouch.pressed, physicalTouch.x, physicalTouch.y, now, validSample};
    portENTER_CRITICAL(&inputMux);
    latestTouch = event;
    portEXIT_CRITICAL(&inputMux);
    if (was != event.pressed) {
      if (event.pressed) ++touchPresses;
      queueTouch(event);
    }
  }
  // BOOT is the only MCU button. The adjacent PWR key belongs to the hardware
  // latch and is deliberately not assigned a second firmware function.
  static bool raw = false, stable = false;
  static uint32_t changed = 0;
  const bool down = digitalRead(BTN_PIN) == LOW;
  if (down != raw) { raw = down; changed = now; }
  if (raw != stable && uint32_t(now - changed) >= 25) {
    stable = raw;
    if (stable) ++buttonPresses;
  }
  const KeyEvent event = bootClicks.update(stable, timerInputMode.load(), now, stopwatchNow());
  if (event.action != waveshare185b::BootAction::None) queueKey(event);
}
void inputTask(void*) {
  TickType_t wake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(10);
  for (;;) {
    pollInputs();
    vTaskDelayUntil(&wake, period);
    if (xTaskGetTickCount() - wake > period) wake = xTaskGetTickCount();
  }
}
void touchRead(lv_indev_drv_t*, lv_indev_data_t* data) {
  static uint32_t lastReadAt = 0;
  static bool hasRead = false;
  const uint32_t now = millis();
  const bool stalled = hasRead && !waveshare185b::inputFresh(lastReadAt, now);
  lastReadAt = now; hasRead = true;
  TouchEvent event;
  if (xQueueReceive(touchQueue, &event, 0) != pdTRUE) {
    portENTER_CRITICAL(&inputMux);
    event = latestTouch;
    portEXIT_CRITICAL(&inputMux);
  }
  auto decision = touchAgeGuard.check(event.pressed, event.queuedAt, now, touchResync.exchange(false) || stalled, event.validSample);
  if (decision == waveshare185b::TouchDecision::Reset) {
    xQueueReset(touchQueue);
    portENTER_CRITICAL(&inputMux); event = latestTouch; portEXIT_CRITICAL(&inputMux);
    // All LVGL access stays on the consumer thread. Reset cancels the previous
    // press rather than converting a delayed release into a clicked event.
    if (touchDevice) lv_indev_reset(touchDevice, nullptr);
    touchAgeGuard.check(event.pressed, event.queuedAt, now, false, event.validSample);
    ++inputDrops;
  }
  if (decision != waveshare185b::TouchDecision::Accept) event.pressed = false;
  data->point.x = event.x;
  data->point.y = event.y;
  data->state = event.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->continue_reading = uxQueueMessagesWaiting(touchQueue) != 0;
  if (!uiTouch(event.pressed, event.x, event.y)) data->state = LV_INDEV_STATE_RELEASED;
}
bool gaugeWord(uint8_t reg, uint16_t& value) {
  uint8_t b[2]{};
  if (!waveshare185bI2cRead(GaugeAddress, reg, b, sizeof(b))) return false;
  value = uint16_t(b[0]) | uint16_t(b[1]) << 8;
  return true;
}
void sampleBattery() {
  uint32_t now = millis();
  if (batteryAttemptAt && uint32_t(now - batteryAttemptAt) < 5000) return;
  batteryAttemptAt = now;
  uint16_t status = 0, operation = 0, mv = 0, percent = 0, current = 0;
  bool ok = gaugeWord(0x0a, status) && gaugeWord(0x3a, operation);
  if (ok && !(status & (1u << 3))) {
    gaugePresent = true; battery = -1; batteryMv = 0; charging = false; return;
  }
  ok = ok && (operation & (1u << 5)) && !(operation & (1u << 10)) &&
      gaugeWord(0x08, mv) && gaugeWord(0x2c, percent) && gaugeWord(0x0c, current);
  if (ok && mv >= 2500 && mv <= 4500 && percent <= 100) {
    gaugePresent = true; battery = percent; batteryMv = mv;
    charging = int16_t(current) > 5; batteryGoodAt = now;
  } else if (!batteryGoodAt || uint32_t(now - batteryGoodAt) > 30000) {
    battery = -1; charging = false;
  }
  // Read-only: never unseal or change a battery's chemistry/capacity profile.
}
} // namespace

bool waveshare185bI2cLock(uint32_t timeoutMs) {
  return busMutex && xSemaphoreTake(busMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}
void waveshare185bI2cUnlock() { if (busMutex) xSemaphoreGive(busMutex); }
bool waveshare185bI2cRead(uint8_t address, uint8_t reg, uint8_t* data, size_t count) {
  if (!data || !count || count > 32 || !waveshare185bI2cLock()) return false;
  Wire.beginTransmission(address);
  Wire.write(reg);
  bool ok = Wire.endTransmission(false) == 0;
  if (ok) ok = Wire.requestFrom(address, count) == count;
  if (ok) for (size_t i = 0; i < count; ++i) data[i] = Wire.read();
  else { ++i2cErrors; while (Wire.available()) Wire.read(); }
  waveshare185bI2cUnlock();
  return ok;
}
bool waveshare185bI2cWrite(uint8_t address, uint8_t reg, const uint8_t* data, size_t count) {
  if (!data || !count || count > 31 || !waveshare185bI2cLock()) return false;
  Wire.beginTransmission(address);
  Wire.write(reg);
  bool ok = Wire.write(data, count) == count;
  const uint8_t result = Wire.endTransmission();
  ok = ok && result == 0;
  if (!ok) ++i2cErrors;
  waveshare185bI2cUnlock();
  return ok;
}

void boardBegin() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LcdTePin, INPUT);
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, LOW);
  busMutex = xSemaphoreCreateMutex();
  touchQueue = xQueueCreate(16, sizeof(TouchEvent));
  keyQueue = xQueueCreate(8, sizeof(KeyEvent));
  if (!busMutex || !touchQueue || !keyQueue) abort();
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(15);
  backlightReady = ledcAttach(LCD_BACKLIGHT, 20000, 10);
  if (backlightReady) ledcWrite(LCD_BACKLIGHT, 0);
}
void set_brightness(uint8_t brightness) {
  if (backlightReady) ledcWrite(LCD_BACKLIGHT, uint32_t(constrain(brightness, 0, 100)) * 1023 / 100);
}
int batteryPercent() { sampleBattery(); return battery; }
bool batteryCharging() { sampleBattery(); return charging; }

void scr_lvgl_init() {
  pinMode(LCD_RESET, OUTPUT);
  digitalWrite(LCD_RESET, LOW); delay(10);
  digitalWrite(LCD_RESET, HIGH); delay(50);
  spi_bus_config_t bus{};
  bus.sclk_io_num = LCD_SCLK;
  bus.data0_io_num = LCD_SDIO0; bus.data1_io_num = LCD_SDIO1;
  bus.data2_io_num = LCD_SDIO2; bus.data3_io_num = LCD_SDIO3;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  bus.max_transfer_sz = SCREEN_RES * 40 * 2;
  bus.flags = SPICOMMON_BUSFLAG_QUAD;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t io{};
  io.cs_gpio_num = LCD_CS; io.dc_gpio_num = -1; io.spi_mode = 0;
  io.pclk_hz = 3000000; io.trans_queue_depth = 1;
  io.lcd_cmd_bits = 32; io.lcd_param_bits = 8; io.flags.quad_mode = true;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &panelIo));
  for (unsigned attempt = 0; attempt < 3 && !panelVersion; ++attempt) {
    memset(panelId, 0, sizeof(panelId));
    esp_err_t idResult = esp_lcd_panel_io_rx_param(panelIo, 0x0b000400, panelId, sizeof(panelId));
    if (idResult == ESP_OK && panelId[0] == 0 && panelId[2] == 0x7f && panelId[3] == 0x7f) {
      if (panelId[1] == 0x7f) panelVersion = 1;
      else if (panelId[1] == 2) panelVersion = 2;
    }
    if (!panelVersion && attempt < 2) delay(5);
  }
  ESP_ERROR_CHECK(esp_lcd_panel_io_del(panelIo));
  panelIo = nullptr;
  // 40 MHz leaves electrical margin while still moving a full 360px frame
  // quickly over four data lines. The vendor qualifies up to 80 MHz.
  io.pclk_hz = 40000000;
  io.user_ctx = &displayDriver; io.on_color_trans_done = flushComplete;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &panelIo));
  uint8_t format = 0x55, direction = 0;
  ESP_ERROR_CHECK(lcdCommand(0x3a, &format, 1));
  ESP_ERROR_CHECK(lcdCommand(0x36, &direction, 1));
  const auto* commands = panelVersion == 2 ? waveshare185b::panelV2 : waveshare185b::panelV1;
  const size_t count = panelVersion == 2 ? sizeof(waveshare185b::panelV2) / sizeof(*commands) : sizeof(waveshare185b::panelV1) / sizeof(*commands);
  for (size_t i = 0; i < count; ++i) {
    ESP_ERROR_CHECK(lcdCommand(commands[i].command, commands[i].data, commands[i].size));
    if (commands[i].delayMs) delay(commands[i].delayMs);
  }
  Serial.printf("DISPLAY ST77916 360x360 id=%02x,%02x,%02x,%02x revision=%u clock=40MHz%s\n",
      panelId[0], panelId[1], panelId[2], panelId[3], panelVersion, panelVersion ? "" : " fallback=V1");
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW); delay(10);
  digitalWrite(TP_RESET, HIGH); delay(50);
  touchReady = waveshare185bI2cRead(TP_I2C_ADDR, 0xa7, &touchChip, 1);
  uint8_t disableSleep = 1;
  if (touchReady) touchReady = waveshare185bI2cWrite(TP_I2C_ADDR, 0xfe, &disableSleep, 1);
  pinMode(TP_INT, INPUT_PULLUP);
  if (touchReady) attachInterrupt(digitalPinToInterrupt(TP_INT), onTouch, FALLING);
  Serial.printf("TOUCH CST816S ready=%u chip=%02x\n", touchReady, touchChip);
  lv_init();
  auto* buffer = static_cast<lv_color_t*>(heap_caps_malloc(SCREEN_RES * 40 * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  if (!buffer) abort();
  lv_disp_draw_buf_init(&drawBuffer, buffer, nullptr, SCREEN_RES * 40);
  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = SCREEN_RES; displayDriver.ver_res = SCREEN_RES;
  displayDriver.draw_buf = &drawBuffer; displayDriver.flush_cb = flush;
  lv_disp_drv_register(&displayDriver);
  static lv_indev_drv_t input;
  lv_indev_drv_init(&input);
  input.type = LV_INDEV_TYPE_POINTER; input.read_cb = touchRead;
  touchDevice = lv_indev_drv_register(&input);
  inputTaskReady = xTaskCreatePinnedToCore(inputTask, "185b-input", 3072, nullptr, 2, nullptr, ARDUINO_RUNNING_CORE) == pdPASS;
}
void boardLoop() {
  timerInputMode.store(ui185bTimerVisible());
  if (!inputTaskReady) pollInputs();
  KeyEvent key;
  for (unsigned n = 0; n < 8 && xQueueReceive(keyQueue, &key, 0) == pdTRUE; ++n) {
    if (!waveshare185b::inputFresh(key.queuedAt, millis())) { ++inputDrops; continue; }
    if (key.action == waveshare185b::BootAction::Home) ui185bHome();
    else if (key.timer) {
      // A delayed timer click must never turn into navigation after a touch
      // switched screens. The original first-press timestamp stays intact.
      if (ui185bTimerVisible()) uiStopwatchKey(key.action == waveshare185b::BootAction::Single, key.at);
      else ++inputDrops;
    } else if (!ui185bTimerVisible()) ui185bCycle();
    else ++inputDrops;
  }
  timerInputMode.store(ui185bTimerVisible());
}

// RTC_IMPLEMENTATION_BEGIN: compiled directly by tools/test_waveshare185b.py.
static bool rtcRead(uint8_t reg, uint8_t* data, size_t count) {
  return waveshare185bI2cRead(0x51, reg, data, count);
}
static bool rtcWrite(uint8_t reg, const uint8_t* data, size_t count) {
  return waveshare185bI2cWrite(0x51, reg, data, count);
}
void boardSensorsBegin() {
  uint8_t control = 0;
  rtcAvailable = false;
  for (unsigned attempt = 0; attempt < 3 && !rtcAvailable; ++attempt) {
    rtcAvailable = rtcRead(0, &control, 1);
    if (!rtcAvailable) delay(10);
  }
  // Probe only. Do not reset, seed or clear oscillator-loss state on startup.
}
static int decodeBcd(uint8_t value) {
  return (value & 15) <= 9 && (value >> 4) <= 9 ? (value >> 4) * 10 + (value & 15) : -1;
}
bool boardReadRtc(uint32_t& epoch) {
  uint8_t control = 0, b[7]{};
  if (!rtcAvailable || !rtcRead(0, &control, 1) || (control & 0xa2) ||
      !rtcRead(4, b, sizeof(b)) || (b[0] & 0x80) || (b[4] & 7) > 6) return false;
  dragon::Calendar date{2000 + decodeBcd(b[6]), decodeBcd(b[5] & 31), decodeBcd(b[3] & 63),
      decodeBcd(b[2] & 63), decodeBcd(b[1] & 127), decodeBcd(b[0] & 127)};
  const uint32_t result = dragon::dateEpoch(date);
  if (!result) return false;
  epoch = result;
  return true;
}
bool boardWriteRtc(uint32_t epoch) {
  if (!rtcAvailable || !dragon::validEpoch(epoch)) return false;
  time_t value = epoch;
  struct tm date{};
  gmtime_r(&value, &date);
  auto bcd = [](int v) { return uint8_t((v / 10) * 16 + v % 10); };
  uint8_t control = 0;
  if (!rtcRead(0, &control, 1)) return false;
  // Preserve CAP_SEL and correction mode, clear test/12-hour/reset modes.
  // STOP permits an atomic calendar update; restart even if the write fails.
  control = (control & 0x05) | 0x20;
  if (!rtcWrite(0, &control, 1)) return false;
  const uint8_t b[] = {bcd(date.tm_sec), bcd(date.tm_min), bcd(date.tm_hour), bcd(date.tm_mday),
      uint8_t(date.tm_wday), bcd(date.tm_mon + 1), bcd(date.tm_year % 100)};
  const bool written = rtcWrite(4, b, sizeof(b));
  control &= ~0x20;
  const bool restarted = rtcWrite(0, &control, 1);
  uint32_t check = 0;
  return written && restarted && boardReadRtc(check) && check >= epoch && check - epoch <= 1;
}
void boardRtcDiagnostics() {
  uint8_t control = 0, seconds = 0;
  bool ok = rtcRead(0, &control, 1) && rtcRead(4, &seconds, 1);
  Serial.printf("CLOCK_DIAGNOSTICS chip=PCF85063 io=%u control=%02x lost=%u backup=battery\n", ok, control, bool(seconds & 0x80));
}
// RTC_IMPLEMENTATION_END

void waveshare185bDiagnostics() {
  TouchEvent touch;
  portENTER_CRITICAL(&inputMux); touch = latestTouch; portEXIT_CRITICAL(&inputMux);
  Serial.printf("185B_STATUS flash=%u psram=%u internal=%u sd=%u rtc=%u panel=%u id=%02x%02x%02x%02x flushes=%lu flush_errors=%lu\n",
      ESP.getFlashChipSize(), ESP.getPsramSize(), ESP.getFreeHeap(), sdReady, rtcAvailable,
      panelVersion, panelId[0], panelId[1], panelId[2], panelId[3], flushes, flushErrors);
  Serial.printf("TOUCH_STATUS enabled=%u task=%u chip=%02x irq=%d samples=%lu presses=%lu errors=%lu i2c_errors=%lu pressed=%u xy=%u,%u drops=%lu keys=%lu\n",
      touchReady, inputTaskReady, touchChip, digitalRead(TP_INT), touchSamples.load(), touchPresses.load(), touchErrors.load(),
      i2cErrors.load(), touch.pressed, touch.x, touch.y, inputDrops.load(), buttonPresses.load());
  Serial.printf("BATTERY_STATUS gauge=%u percent=%d mv=%u charging=%u power_key=hardware\n", gaugePresent, battery, batteryMv, charging);
}
#endif
