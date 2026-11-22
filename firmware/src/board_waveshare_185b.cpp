// Dragon Detector integration for Waveshare ESP32-S3-Touch-LCD-1.85B Rev1.1.
// ST77916 initialization data retain their Waveshare Apache-2.0 provenance.
#include "board_pins.h"
#ifdef BOARD_WAVESHARE_185B
#include "app.h"
#include "board_waveshare_185b.h"
#include "waveshare_185b_lcd_init.h"
#include "waveshare_185b_touch.h"
#include "waveshare_185b_pointer.h"
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
constexpr size_t DmaTilePixels = SCREEN_RES * 40;
lv_color_t* dmaTile = nullptr;
size_t dmaTilePixels = DmaTilePixels;
bool stagedDisplay = false;
uint32_t displayRegions = 0, displayRefreshes = 0;
uint32_t refreshLastMs = 0, refreshMaxMs = 0, refreshTotalMs = 0, refreshPixels = 0;
lv_indev_t* touchDevice = nullptr;
uint8_t panelId[4]{}, panelVersion = 0;
bool inputTaskReady = false, backlightReady = false;
std::atomic<bool> touchReady{false}, reportIrqMode{false};
std::atomic<uint8_t> touchChip{0};
TaskHandle_t touchTaskHandle = nullptr;
std::atomic<uint32_t> irqWakeups{0}, timedWakeups{0}, touchAttempts{0}, touchInitFailures{0};
std::atomic<uint32_t> touchRecoveries{0}, touchGoodAt{0};
std::atomic<uint32_t> i2cErrors{0}, touchSamples{0}, touchErrors{0};
std::atomic<uint32_t> touchPresses{0}, inputDrops{0}, buttonPresses{0};
std::atomic<bool> timerInputMode{false};
volatile uint32_t flushes = 0, flushErrors = 0;
struct TouchEvent { bool pressed; uint16_t x, y; uint32_t queuedAt; bool validSample; };
using KeyEvent = waveshare185b::BootEvent;
QueueHandle_t keyQueue = nullptr;
portMUX_TYPE inputMux = portMUX_INITIALIZER_UNLOCKED;
TouchEvent latestTouch{};
waveshare185b::TouchState physicalTouch;
waveshare185b::PointerBuffer pointerBuffer;
uint32_t deliveredPresses = 0, deliveredMoves = 0, priorityReads = 0;
waveshare185b::BootClicks bootClicks;
int battery = -1;
uint16_t batteryMv = 0;
bool charging = false, gaugePresent = false;
uint32_t batteryGoodAt = 0, batteryAttemptAt = 0;

void IRAM_ATTR onTouch() {
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(touchTaskHandle, &woken);
  if (woken) portYIELD_FROM_ISR();
}
bool IRAM_ATTR flushComplete(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  ++flushes;
  // Direct mode lends LVGL's buffer to DMA. In staged mode the buffers are
  // separate and the UI thread releases the original PSRAM region itself.
  if (!stagedDisplay) lv_disp_flush_ready(&displayDriver);
  return false;
}
esp_err_t lcdCommand(uint8_t command, const void* data, size_t size) {
  return esp_lcd_panel_io_tx_param(panelIo, (0x02u << 24) | (uint32_t(command) << 8), data, size);
}
// DISPLAY_FLUSH_BEGIN: exercised with asynchronous DMA ownership in the host test.
esp_err_t setPanelRange(uint8_t command, int first, int last) {
  const uint8_t bytes[] = {uint8_t(first >> 8), uint8_t(first), uint8_t(last >> 8), uint8_t(last)};
  return lcdCommand(command, bytes, sizeof(bytes));
}
void flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* pixels) {
  ++displayRegions;
  const int width = area->x2 - area->x1 + 1;
  const int height = area->y2 - area->y1 + 1;
  if (width <= 0 || height <= 0 || area->x1 < 0 || area->x2 >= SCREEN_RES ||
      area->y1 < 0 || area->y2 >= SCREEN_RES || (!stagedDisplay && size_t(width) * height > dmaTilePixels)) {
    ++flushErrors;
    lv_disp_flush_ready(disp);
    return;
  }
  const int rowsPerTransfer = int(dmaTilePixels / size_t(width));
  esp_err_t result = setPanelRange(0x2a, area->x1, area->x2);
  for (int row = area->y1; result == ESP_OK && row <= area->y2; ) {
    const int rows = std::min(rowsPerTransfer, int(area->y2) - row + 1);
    const size_t count = size_t(rows) * width;
    // tx_param waits for any earlier queued color DMA, including the previous
    // region's final tile. Only copy over dmaTile after the window write succeeds.
    result = setPanelRange(0x2b, row, row + rows - 1);
    if (result != ESP_OK) break;
    if (stagedDisplay) memcpy(dmaTile, pixels, count * sizeof(*pixels));
    result = esp_lcd_panel_io_tx_color(panelIo, 0x32002c00,
        stagedDisplay ? dmaTile : pixels, count * sizeof(*pixels));
    pixels += count;
    row += rows;
  }
  if (result != ESP_OK) ++flushErrors;
  if (stagedDisplay || result != ESP_OK) lv_disp_flush_ready(disp);
}
// DISPLAY_FLUSH_END
void monitorRefresh(lv_disp_drv_t*, uint32_t durationMs, uint32_t pixels) {
  ++displayRefreshes;
  refreshLastMs = durationMs;
  refreshTotalMs += durationMs;
  refreshMaxMs = std::max(refreshMaxMs, durationMs);
  refreshPixels = pixels;
}
void publishTouchSample(bool pressed, uint16_t x, uint16_t y, bool validSample, uint32_t now) {
  portENTER_CRITICAL(&inputMux);
  if (validSample && pressed && !latestTouch.pressed) ++touchPresses;
  latestTouch = {pressed, x, y, now, validSample};
  pointerBuffer.sample(pressed, x, y, validSample, now);
  portEXIT_CRITICAL(&inputMux);
}
void queueKey(const KeyEvent& event) {
  if (xQueueSend(keyQueue, &event, 0) != pdTRUE) {
    xQueueReset(keyQueue);
    ++inputDrops;
  }
}
enum class TouchPhase { Start, ResetLow, Settling, Online, Backoff };
TouchPhase touchPhase = TouchPhase::Start;
uint32_t touchPhaseAt = 0, firstBadAt = 0;
unsigned badSamples = 0;

bool configureTouch() {
  uint8_t chip = 0;
  bool found = waveshare185bI2cRead(TP_I2C_ADDR, 0xa7, &chip, 1);
  touchChip.store(chip);
  const uint8_t awake = 1;
  if (!found || !waveshare185b::supportedTouchChip(chip) ||
      !waveshare185bI2cWrite(TP_I2C_ADDR, 0xfe, &awake, 1)) return false;

  // IRQ_CTL EnTouch/EnChange send a pulse on contact and movement. Preserve
  // the board's other IRQ settings. Polling also works on controllers that
  // cannot read/configure this optional register.
  uint8_t mode = 0;
  bool configured = waveshare185bI2cRead(TP_I2C_ADDR, 0xfa, &mode, 1) && mode != 0xff;
  if (configured && (mode & 0x60) != 0x60) {
    mode |= 0x60;
    configured = waveshare185bI2cWrite(TP_I2C_ADDR, 0xfa, &mode, 1) &&
                 waveshare185bI2cRead(TP_I2C_ADDR, 0xfa, &mode, 1) &&
                 mode != 0xff && (mode & 0x60) == 0x60;
  }
  reportIrqMode.store(configured);
  return true;
}

void pollTouch(uint32_t now) {
  // Reset only the CST816. The RTC, gauge and codec share Wire. Each reset
  // phase yields to the scheduler, including in the task-allocation fallback.
  switch (touchPhase) {
    case TouchPhase::Backoff:
      if (uint32_t(now - touchPhaseAt) < 2000) return;
      [[fallthrough]];
    case TouchPhase::Start:
      ++touchAttempts;
      reportIrqMode.store(false);
      digitalWrite(TP_RESET, LOW);
      touchPhaseAt = now; touchPhase = TouchPhase::ResetLow;
      return;
    case TouchPhase::ResetLow:
      if (uint32_t(now - touchPhaseAt) < 10) return;
      digitalWrite(TP_RESET, HIGH);
      touchPhaseAt = now; touchPhase = TouchPhase::Settling;
      return;
    case TouchPhase::Settling:
      if (uint32_t(now - touchPhaseAt) < 50) return;
      if (configureTouch()) {
        badSamples = 0; touchPhase = TouchPhase::Online; touchReady.store(true);
      } else {
        ++touchInitFailures; touchPhase = TouchPhase::Backoff; touchPhaseAt = now;
      }
      return;
    case TouchPhase::Online:
      break;
  }

  uint8_t packet[6]{};
  const bool ok = waveshare185bI2cRead(TP_I2C_ADDR, 0x01, packet, sizeof(packet));
  now = millis();
  const bool valid = physicalTouch.update(packet, sizeof(packet), ok, now);
  publishTouchSample(physicalTouch.pressed, physicalTouch.x, physicalTouch.y, valid, now);
  if (valid) {
    ++touchSamples; touchGoodAt.store(now); badSamples = 0;
    return;
  }
  ++touchErrors;
  if (badSamples++ == 0) firstBadAt = now;
  if (badSamples >= 3 && uint32_t(now - firstBadAt) >= 120) {
    ++touchRecoveries; touchReady.store(false);
    touchPhase = TouchPhase::Backoff; touchPhaseAt = now;
  }
}

void touchTask(void*) {
  for (;;) {
    pollTouch(millis());
    // Bound reads even on a noisy IRQ. The task notification remains pending
    // during the minimum interval. A timed read also catches an unreported lift.
    vTaskDelay(1);
    uint32_t pulses = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(9));
    if (pulses) irqWakeups.fetch_add(pulses);
    else ++timedWakeups;
  }
}

void pollInputs() {
  const uint32_t now = millis();
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
  using waveshare185b::PointerKind;
  portENTER_CRITICAL(&inputMux);
  const auto event = pointerBuffer.read(millis());
  const bool more = pointerBuffer.hasWork();
  portEXIT_CRITICAL(&inputMux);
  // Deliver PRESS_LOST so a canceled button also clears its pressed style.
  if (event.kind == PointerKind::Cancel && touchDevice) lv_indev_wait_release(touchDevice);
  data->point.x = event.x;
  data->point.y = event.y;
  const bool allow = uiTouch(event.pressed(), event.x, event.y);
  data->state = allow && event.pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (allow && event.kind == PointerKind::Down) ++deliveredPresses;
  if (allow && event.kind == PointerKind::Move) ++deliveredMoves;
  // A release can request a page change. Let the app build it before another
  // complete gesture can select a control on the previous view.
  data->continue_reading = more && event.kind != PointerKind::Up;
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

bool waveshare185bUsesStagedDisplay() { return stagedDisplay; }

void waveshare185bResetPointer() {
  portENTER_CRITICAL(&inputMux);
  pointerBuffer.resetView();
  portEXIT_CRITICAL(&inputMux);
}

bool waveshare185bServicePointer() {
  // Called only by the UI thread. A tap can run as soon as the main loop is
  // available even when the periodic LVGL input timer isn't due yet.
  if (!touchDevice) return false;
  portENTER_CRITICAL(&inputMux);
  const bool pending = pointerBuffer.hasWork();
  portEXIT_CRITICAL(&inputMux);
  if (!pending) return false;
  ++priorityReads;
  // A priority read can precede the display refresh that normally lays out
  // freshly created controls. Resolve their hit boxes before hit testing.
  lv_obj_update_layout(lv_scr_act());
  lv_obj_update_layout(lv_layer_top());
  lv_timer_t* timer = touchDevice->driver->read_timer;
  lv_indev_read_timer_cb(timer);
  lv_timer_reset(timer);
  return true;
}

void print185bPointerDiagnostics() {
  portENTER_CRITICAL(&inputMux);
  const auto stats = pointerBuffer.stats;
  portEXIT_CRITICAL(&inputMux);
  Serial.printf("TOUCH_UI presses=%lu moves=%lu priority_reads=%lu lag_ms=%lu max_lag_ms=%lu peak_queue=%u coalesced=%lu overflow=%lu expired=%lu faults=%lu cancels=%lu\n",
      deliveredPresses, deliveredMoves, priorityReads, stats.lastLagMs, stats.maxLagMs,
      stats.peakDepth, stats.coalesced, stats.overflow, stats.expired, stats.faults, stats.cancels);
}

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
  keyQueue = xQueueCreate(8, sizeof(KeyEvent));
  if (!busMutex || !keyQueue) abort();
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
  bus.max_transfer_sz = DmaTilePixels * sizeof(lv_color_t);
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
  pinMode(TP_INT, INPUT_PULLUP);
  lv_init();
  // Keep the existing internal DMA budget. A full PSRAM draw surface lets
  // LVGL render the radar once per invalid region instead of once per tile.
  while (!dmaTile && dmaTilePixels >= SCREEN_RES) {
    dmaTile = static_cast<lv_color_t*>(heap_caps_malloc(dmaTilePixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!dmaTile) dmaTilePixels /= 2;
  }
  if (!dmaTile) abort();
  constexpr size_t framePixels = SCREEN_RES * SCREEN_RES;
  auto* frame = static_cast<lv_color_t*>(heap_caps_malloc(framePixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  stagedDisplay = frame != nullptr;
  lv_disp_draw_buf_init(&drawBuffer, stagedDisplay ? frame : dmaTile, nullptr,
      stagedDisplay ? framePixels : dmaTilePixels);
  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = SCREEN_RES; displayDriver.ver_res = SCREEN_RES;
  displayDriver.draw_buf = &drawBuffer; displayDriver.flush_cb = flush;
  displayDriver.monitor_cb = monitorRefresh;
  lv_disp_drv_register(&displayDriver);
  static lv_indev_drv_t input;
  lv_indev_drv_init(&input);
  input.type = LV_INDEV_TYPE_POINTER; input.read_cb = touchRead;
  touchDevice = lv_indev_drv_register(&input);
  if (touchDevice) lv_timer_set_period(touchDevice->driver->read_timer, 10);
  inputTaskReady = xTaskCreatePinnedToCore(inputTask, "185b-input", 3072, nullptr, 2, nullptr, ARDUINO_RUNNING_CORE) == pdPASS;
  TaskHandle_t createdTouchTask = nullptr;
  if (xTaskCreatePinnedToCore(touchTask, "185b-touch", 3072, nullptr, 2, &createdTouchTask, ARDUINO_RUNNING_CORE) == pdPASS) {
    touchTaskHandle = createdTouchTask;
    attachInterrupt(digitalPinToInterrupt(TP_INT), onTouch, FALLING);
  }
}
void boardLoop() {
  timerInputMode.store(ui185bTimerVisible());
  if (!inputTaskReady) pollInputs();
  if (!touchTaskHandle) {
    static uint32_t lastFallback = 0;
    const uint32_t now = millis();
    if (uint32_t(now - lastFallback) >= 10) {lastFallback = now; pollTouch(now);}
  }
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
  print185bPointerDiagnostics();
  Serial.printf("185B_RENDER mode=%s tile_pixels=%u regions=%lu refreshes=%lu last_ms=%lu max_ms=%lu total_ms=%lu pixels=%lu\n",
      stagedDisplay ? "psram-frame" : "internal-tiles", unsigned(dmaTilePixels), displayRegions, displayRefreshes,
      refreshLastMs, refreshMaxMs, refreshTotalMs, refreshPixels);
  TouchEvent touch;
  portENTER_CRITICAL(&inputMux);
  touch = latestTouch;
  const uint32_t discarded = pointerBuffer.stats.overflow + pointerBuffer.stats.expired + pointerBuffer.stats.faults;
  portEXIT_CRITICAL(&inputMux);
  Serial.printf("185B_STATUS flash=%u psram=%u internal=%u sd=%u rtc=%u panel=%u id=%02x%02x%02x%02x flushes=%lu flush_errors=%lu\n",
      ESP.getFlashChipSize(), ESP.getPsramSize(), ESP.getFreeHeap(), sdReady, rtcAvailable,
      panelVersion, panelId[0], panelId[1], panelId[2], panelId[3], flushes, flushErrors);
  Serial.printf("TOUCH_STATUS enabled=%u task=%u chip=%02x irq=%d samples=%lu presses=%lu errors=%lu i2c_errors=%lu pressed=%u xy=%u,%u drops=%lu keys=%lu\n",
      touchReady.load(), touchTaskHandle != nullptr, touchChip.load(), digitalRead(TP_INT), touchSamples.load(), touchPresses.load(), touchErrors.load(),
      i2cErrors.load(), touch.pressed, touch.x, touch.y, inputDrops.load() + discarded, buttonPresses.load());
  const uint32_t validAge = millis() - touchGoodAt.load();
  Serial.printf("TOUCH_ACQUISITION healthy=%u last_good_age_ms=%lu irq_mode=%u irq_pulses=%lu timed_polls=%lu attempts=%lu init_failures=%lu recoveries=%lu key_task=%u\n",
      touchReady.load() && touchSamples.load() && validAge < 150, validAge, reportIrqMode.load(), irqWakeups.load(),
      timedWakeups.load(), touchAttempts.load(), touchInitFailures.load(), touchRecoveries.load(), inputTaskReady);
  Serial.printf("BATTERY_STATUS gauge=%u percent=%d mv=%u charging=%u power_key=hardware\n", gaugePresent, battery, batteryMv, charging);
}
#endif
