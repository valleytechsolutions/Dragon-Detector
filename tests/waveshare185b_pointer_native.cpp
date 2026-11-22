// Linked by tools/test_waveshare185b_pointer.py against the project's LVGL.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <lvgl.h>
#include "waveshare_185b_pointer.h"

// The native test is single threaded. Only platform I/O, the clock and the
// sensor samples are simulated; input publication, wake/gesture dispatch,
// scroll detection, and object click handling run their real implementations.
#define portENTER_CRITICAL(lock) ((void)0)
#define portEXIT_CRITICAL(lock) ((void)0)
static int outputBrightness = 100;
static uint32_t now, lastInteraction, touchPresses, deliveredPresses, deliveredMoves, priorityReads;
static uint32_t millis() { return now; }
struct TouchEvent { bool pressed; uint16_t x, y; uint32_t queuedAt; bool validSample; };
static TouchEvent latestTouch{};
static waveshare185b::PointerBuffer pointerBuffer;
static lv_indev_t* touchDevice;
#include "production.inc"

static unsigned clicks, rowClicks, lost, holds;
static void advance(unsigned ms) { now += ms; lv_tick_inc(ms); }
static void sample(bool down, int x = 105, int y = 85, bool valid = true) {
  advance(5);
  publishTouchSample(down, x, y, valid, now);
}
static void tap() { sample(true); sample(false); }
static void flush(lv_disp_drv_t* d, const lv_area_t*, lv_color_t*) { lv_disp_flush_ready(d); }
static lv_obj_t* button(lv_obj_t* parent, int x, int y, int w, int h, unsigned* count) {
  auto* b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_add_event_cb(b, [](lv_event_t* e) {
    ++*static_cast<unsigned*>(lv_event_get_user_data(e));
  }, LV_EVENT_CLICKED, count);
  lv_obj_add_event_cb(b, [](lv_event_t*) { ++lost; }, LV_EVENT_PRESS_LOST, nullptr);
  lv_obj_add_event_cb(b, [](lv_event_t*) { ++holds; }, LV_EVENT_LONG_PRESSED, nullptr);
  return b;
}

int main() {
  lv_init();
  static lv_color_t pixels[360 * 32];
  lv_disp_draw_buf_t buffer;
  lv_disp_draw_buf_init(&buffer, pixels, nullptr, 360 * 32);
  lv_disp_drv_t display;
  lv_disp_drv_init(&display);
  display.hor_res = display.ver_res = 360; display.draw_buf = &buffer; display.flush_cb = flush;
  lv_disp_drv_register(&display);
  lv_indev_drv_t driver;
  lv_indev_drv_init(&driver);
  driver.type = LV_INDEV_TYPE_POINTER; driver.read_cb = touchRead;
  touchDevice = lv_indev_drv_register(&driver);
  lv_timer_set_period(touchDevice->driver->read_timer, 10);
  auto* screen = lv_scr_act();
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  auto* target = button(screen, 70, 60, 150, 50, &clicks);
  auto* list = lv_obj_create(screen);
  lv_obj_remove_style_all(list);
  lv_obj_set_pos(list, 25, 140); lv_obj_set_size(list, 300, 180);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  for (int i = 0; i < 9; ++i) button(list, 5, i * 50, 260, 46, &rowClicks);
  lv_obj_update_layout(screen); advance(40); lv_timer_handler();

  // A complete brief tap is delivered on demand, ahead of the timer.
  tap(); assert(waveshare185bServicePointer() && clicks == 1);
  assert(!waveshare185bServicePointer());
  // End each gesture at UP so a pending view change can run between taps.
  tap(); tap();
  assert(waveshare185bServicePointer() && clicks == 2);
  assert(waveshare185bServicePointer() && clicks == 3);
  // The wake tap is consumed until release despite brightness coming back.
  outputBrightness = 0; sample(true); waveshare185bServicePointer();
  outputBrightness = 100; sample(false); waveshare185bServicePointer();
  assert(clicks == 3); tap(); waveshare185bServicePointer(); assert(clicks == 4);
  // A fresh tap near the end of a long idle/UI stall still reaches its target.
  advance(900); tap(); waveshare185bServicePointer(); assert(clicks == 5);
  tap(); advance(550); tap(); waveshare185bServicePointer();
  assert(clicks == 6 && pointerBuffer.stats.expired != 0);
  // A delivered press held across a blocked UI cannot be committed by a
  // fresh release. The next independent tap is preserved.
  sample(true); waveshare185bServicePointer(); advance(550);
  sample(false); tap(); waveshare185bServicePointer(); assert(clicks == 7);
  // A short sensor error during hold leaves the gesture intact.
  sample(true); waveshare185bServicePointer(); assert(lv_obj_has_state(target, LV_STATE_PRESSED));
  sample(true, 105, 85, false); sample(false); waveshare185bServicePointer(); assert(clicks == 8);
  // A prolonged failure produces PRESS_LOST, rearming only on a valid lift.
  sample(true); waveshare185bServicePointer(); unsigned priorLost = lost;
  sample(false, 105, 85, false); waveshare185bServicePointer();
  assert(clicks == 8 && lost == priorLost + 1 && !lv_obj_has_state(target, LV_STATE_PRESSED));
  sample(true); sample(false); assert(!waveshare185bServicePointer());
  tap(); waveshare185bServicePointer(); assert(clicks == 9);
  // A page rebuild while down prevents a release hitting a replacement button.
  sample(true); waveshare185bServicePointer(); waveshare185bResetPointer();
  lv_obj_del(target); target = button(screen, 70, 60, 150, 50, &clicks);
  sample(true); sample(false); waveshare185bServicePointer(); assert(clicks == 9);
  tap(); waveshare185bServicePointer(); assert(clicks == 10);
  // Long press gets its regular LVGL timer reads while stationary.
  sample(true); waveshare185bServicePointer();
  for (unsigned i = 0; i < 60; ++i) { sample(true); advance(5); lv_timer_handler(); }
  assert(holds == 1);
  sample(false); waveshare185bServicePointer(); assert(clicks == 11);
  // The WHOLE swipe (including lift) happens before LVGL gets CPU time.
  sample(true, 100, 270);
  for (int y = 265; y >= 165; y -= 5) sample(true, 100, y);
  sample(false); waveshare185bServicePointer();
  assert(lv_obj_get_scroll_y(list) > 30 && rowClicks == 0);
  assert(pointerBuffer.stats.coalesced > 0);
  // Backtracking to the original coordinate still performed a real drag.
  lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
  sample(true, 100, 270); sample(true, 100, 175); sample(true, 100, 270);
  sample(false); waveshare185bServicePointer(); assert(rowClicks == 0);
  // Overflow cancels, even when a finger is down at the time of overflow.
  sample(true); waveshare185bServicePointer();
  for (unsigned i = 0; i < 40; ++i) sample(true, i % 2 ? 114 : 96, 85);
  sample(false); waveshare185bServicePointer(); assert(clicks == 11);
  assert(pointerBuffer.stats.overflow == 1);
  tap(); waveshare185bServicePointer(); assert(clicks == 12);
  assert(deliveredPresses && deliveredMoves && priorityReads);
  puts("PASS real LVGL 1.85B pointer: quick/batched taps, wake, faults, fresh/stale stalls, "
       "page rebuild, stationary hold, queued swipe, reversal, overflow and rearm");
}
