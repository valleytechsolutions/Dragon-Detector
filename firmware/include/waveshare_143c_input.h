#pragma once
#include <stddef.h>
#include <stdint.h>
#include "waveshare_185b_button.h"

// Pure input logic for the Waveshare ESP32-S3-Touch-AMOLED-1.43C, kept free of
// Arduino types so tools/test_waveshare143c.py can compile it on a workstation.
namespace waveshare143c {
constexpr uint16_t Resolution = 466;

// BOOT is the only key the ESP32 can read; PWR drives the board's power latch.
// That is the 1.85B's arrangement, so its click recognizer and its navigation
// are reused unchanged: a short press steps Radar > Main menu > Utilities >
// Badge > Clock, a hold opens the main menu, and on the timer one click is
// Start/Pause and two are Lap/Finish (running) or Reset (stopped).
using KeyAction = waveshare185b::BootAction;
using KeyEvent = waveshare185b::BootEvent;
using KeyClicks = waveshare185b::BootClicks;
constexpr uint32_t KeyDebounceMs = 25;
// A second step waits one UI pass (see endsDrain), and a pass that redraws the
// radar and writes history has been measured at almost half a second.
constexpr uint32_t KeyMaxAgeMs = 1000;

// Debounces the raw BOOT level for the click recognizer.
struct KeyDebounce {
  bool raw = false, stable = false;
  uint32_t changedAt = 0;
  bool update(bool down, uint32_t now) {
    if (down != raw) { raw = down; changedAt = now; }
    if (raw != stable && uint32_t(now - changedAt) >= KeyDebounceMs) stable = raw;
    return stable;
  }
};

// What a recognized event does once the UI task sees it. A timer click that
// arrives after touch has left the timer, a navigation click that arrives on
// it, and anything that waited longer than KeyMaxAgeMs are dropped instead of
// acting on a screen the user was not looking at when they pressed.
enum class KeyRoute : uint8_t { Drop, Home, Timer, Cycle };
inline KeyRoute routeKey(const KeyEvent& event, bool timerVisible, uint32_t now) {
  if (event.action == KeyAction::None || uint32_t(now - event.queuedAt) > KeyMaxAgeMs) return KeyRoute::Drop;
  if (event.action == KeyAction::Home) return KeyRoute::Home;
  if (event.timer) return timerVisible ? KeyRoute::Timer : KeyRoute::Drop;
  return timerVisible ? KeyRoute::Drop : KeyRoute::Cycle;
}
// A navigation step takes effect when the UI builds the page later in the same
// pass, and until then the UI ignores another step. The driver stops taking
// events after the first one, so a second quick press waits for the next pass
// instead of being taken off the queue and lost.
inline bool endsDrain(KeyRoute route) { return route == KeyRoute::Home || route == KeyRoute::Cycle; }
// BOOT answers "Later" on the SET YOUR CLOCK prompt, but not before the prompt
// has been up this long. A queued press can be KeyMaxAgeMs old, and one made
// as the prompt appeared was meant for the screen before it.
constexpr uint32_t ClockPromptGuardMs = KeyMaxAgeMs + 500;
inline bool clockPromptAnswers(uint32_t openedAt, uint32_t now) { return uint32_t(now - openedAt) >= ClockPromptGuardMs; }

// A held finger survives this long of bad samples before it releases.
constexpr uint32_t TouchHoldMs = 120;

// CST820 registers 0x02..0x06: finger count, X high/low, Y high/low. Waveshare
// mounts the panel rotated 180 degrees (MADCTL 0xC0), so both touch axes are
// mirrored to match the picture, exactly as the factory firmware does.
// A stale or malformed packet must never manufacture a press at a clamped edge.
struct TouchState {
  bool pressed = false, failing = false;
  uint16_t x = 0, y = 0;
  uint32_t failedAt = 0;
  // Returns true when the packet was a usable sample.
  bool update(const uint8_t* packet, size_t length, bool ioOk, uint32_t now) {
    const bool valid = ioOk && packet && length == 5 && packet[0] <= 2;
    if (valid && packet[0] == 0) { pressed = failing = false; return true; }
    if (valid) {
      const uint16_t rawX = uint16_t((packet[1] & 0x0f) << 8) | packet[2];
      const uint16_t rawY = uint16_t((packet[3] & 0x0f) << 8) | packet[4];
      if (rawX < Resolution && rawY < Resolution) {
        x = uint16_t(Resolution - 1 - rawX);
        y = uint16_t(Resolution - 1 - rawY);
        pressed = true; failing = false; return true;
      }
    }
    // A momentary bus error does not split a held touch into repeated taps,
    // but a lost release or a vanished controller cannot hold a press forever.
    // The time runs from the first bad sample, not from the last good one: the
    // controller is not read while the UI is busy elsewhere, and one bad read
    // after such a gap is not an outage.
    if (pressed) {
      if (!failing) { failing = true; failedAt = now; }
      else if (uint32_t(now - failedAt) >= TouchHoldMs) pressed = false;
    }
    return false;
  }
};

// The controller is read at most once per TouchPollMs. A read that fails
// slowly is a stuck bus, not a sleeping controller's NACK; it is retried four
// times a second so the bus timeout is not paid on every display transfer.
constexpr uint32_t TouchPollMs = 10, TouchSlowFailMs = 5, TouchRetryMs = 250;
struct TouchCadence {
  bool started = false;
  uint32_t at = 0, wait = TouchPollMs;
  bool due(uint32_t now) const { return !started || uint32_t(now - at) >= wait; }
  void done(uint32_t now, bool ioOk, uint32_t tookMs) {
    started = true; at = now;
    wait = (!ioOk && tookMs >= TouchSlowFailMs) ? TouchRetryMs : TouchPollMs;
  }
};

// An edge older than this is not replayed: the screen may have changed since.
constexpr uint32_t TouchMaxAgeMs = 1000;
// What LVGL is told next. `more` makes LVGL read again at once, `cancel` asks
// the driver to end LVGL's press without a click because input was discarded.
struct TouchStep {
  bool pressed = false, more = false, cancel = false;
  uint16_t x = 0, y = 0;
};

// LVGL reads the pointer once per UI pass, and a pass that redraws the whole
// radar takes about 0.2 s on this board, so a quick tap would begin and end
// between two reads. The driver samples the controller while display transfers
// are in flight and this keeps the edges in order, so LVGL gets the press and
// then the release, as it does from the 1.85B's input queue. Only edges are
// kept: while a finger stays down LVGL gets its newest position.
class TouchInput {
 public:
  static constexpr size_t Capacity = 4;
  // Every controller sample, after TouchState::update(). `valid` is false for
  // a state that was only assumed because the read failed.
  void sample(bool pressed, uint16_t x, uint16_t y, bool valid, uint32_t now) {
    // The controller is read only while the UI runs. An edge found after a
    // pause happened at some time in the pause, so it is as old as the pause,
    // and after more than TouchMaxAgeMs without a sample too old to replay.
    // Such a pause lasts until a sample says something: a failed read after
    // it does not make the next edge look fresh.
    uint32_t pause = sampled ? uint32_t(now - sampledAt) : 0;
    if (pause > TouchMaxAgeMs) unknown = true;
    if (unknown) pause = TouchMaxAgeMs + 1;
    const bool resumed = unknown && valid;
    if (resumed) unknown = false;
    // A finger found down on both sides of such a pause may have lifted and
    // pressed again in it. LVGL's press is not trusted to be the same touch.
    const bool heldAcross = resumed && pressed && down;
    sampled = true; sampledAt = now;
    if (pressed || down) upSince = now;
    down = pressed;
    // Only a lift ends the wait that follows discarded input. A release made
    // up by TouchState's timeout is not one, unless nothing has been felt for
    // TouchMaxAgeMs: a controller that answers only while touched never
    // reports a lift.
    lifted = !pressed && (valid || uint32_t(now - upSince) > TouchMaxAgeMs);
    if (lifted) waitForRelease = false;
    if (heldAcross) waitForRelease = true;
    // Until then the finger is passed on as up, so it makes no edges.
    if (waitForRelease) pressed = false;
    latest = {pressed, x, y, now};
    const bool tail = count ? queue[(head + count - 1) % Capacity].pressed : reported;
    if (pressed == tail) return;
    // More edges than fit: forget the newest press/release pair. The order of
    // the rest and the final state stay right.
    if (count == Capacity) count -= 2;
    queue[(head + count++) % Capacity] = {pressed, x, y, now - pause};
  }
  // `discard` drops everything sampled so far, as if it were stale: the driver
  // asks for it when what was on the panel then is not what LVGL holds now.
  TouchStep next(uint32_t now, bool discard = false) {
    TouchStep step;
    const bool drop = discard && (count || reported || latest.pressed);
    Event event = latest;
    event.at = now;
    if (!drop && count) {
      event = queue[head];
      // LVGL takes movement only from pressed reads. A release away from the
      // point it was last given is passed on as a press there first, so a
      // flick that fell between two reads scrolls instead of clicking the row
      // it started on. The release stays queued for the next read.
      if (!event.pressed && reported && (event.x != shownX || event.y != shownY)) event.pressed = true;
      else { head = (head + 1) % Capacity; --count; }
    }
    // Signed, so an edge stamped just after the caller read the time is new.
    if (drop || int32_t(now - event.at) > int32_t(TouchMaxAgeMs)) {
      // Drop the whole history. A finger that has not lifted must do so before
      // it can click what is on the screen now (the 1.85B's rule).
      count = 0;
      waitForRelease = !lifted;
      latest.pressed = false;
      event = latest;
      step.cancel = true;
    }
    reported = event.pressed;
    shownX = event.x; shownY = event.y;
    step.pressed = event.pressed;
    step.x = event.x; step.y = event.y;
    step.more = count != 0;
    return step;
  }
 private:
  struct Event {
    bool pressed = false;
    uint16_t x = 0, y = 0;
    uint32_t at = 0;
  };
  Event queue[Capacity], latest;
  size_t head = 0, count = 0;
  uint32_t sampledAt = 0, upSince = 0;
  // `down` is the finger as sampled; `latest` and the queue hold what LVGL may see.
  bool sampled = false, down = false, lifted = true, unknown = false;
  bool reported = false, waitForRelease = false;
  // Where LVGL was last told the finger is.
  uint16_t shownX = 0, shownY = 0;
};

// The charger's STAT line is low while charging and toggles when no cell is
// fitted. The reported state follows it only after four equal samples in a row.
struct ChargeFilter {
  uint8_t history = 0;
  bool charging = false;
  bool update(bool statLow) {
    history = uint8_t(history << 1) | (statLow ? 1 : 0);
    if ((history & 0x0f) == 0x0f) charging = true;
    else if ((history & 0x0f) == 0) charging = false;
    return charging;
  }
};

// The board has no fuel gauge: GPIO4 sees the cell through a 1:2 divider.
// This is a resting-voltage estimate for a single 3.7 V lithium cell, so it
// reads high while charging and sags under load. Below 2.5 V there is no cell
// worth reporting (-1 hides the indicator).
inline int batteryPercentFromMillivolts(uint32_t millivolts) {
  struct Point { uint16_t mv; uint8_t percent; };
  static constexpr Point curve[] = {
    {3300, 0}, {3500, 5}, {3600, 10}, {3700, 20}, {3750, 30}, {3790, 40},
    {3830, 50}, {3870, 60}, {3920, 70}, {3970, 80}, {4050, 90}, {4150, 100}};
  constexpr size_t count = sizeof(curve) / sizeof(curve[0]);
  if (millivolts < 2500) return -1;
  if (millivolts <= curve[0].mv) return 0;
  for (size_t i = 1; i < count; ++i) {
    if (millivolts <= curve[i].mv) {
      const uint32_t span = curve[i].mv - curve[i - 1].mv;
      const uint32_t rise = curve[i].percent - curve[i - 1].percent;
      return int(curve[i - 1].percent + (millivolts - curve[i - 1].mv) * rise / span);
    }
  }
  return 100;
}
}
