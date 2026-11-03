#pragma once
#include <stdint.h>

namespace waveshare185b {
enum class BootAction : uint8_t { None, Single, Double, Home };
struct BootEvent {
  BootAction action = BootAction::None;
  uint64_t at = 0;       // Original first press: stopwatch elapsed-time capture.
  uint32_t queuedAt = 0; // Action recognition time: discard stale UI events.
  bool timer = false;
};

// Input is an already debounced level. Timer clicks wait for a possible second
// press; navigation clicks remain immediate. No UI or task APIs live here.
class BootClicks {
 public:
  static constexpr uint32_t DoubleMs = 250, HoldMs = 700;
  BootEvent update(bool down, bool timerVisible, uint32_t now, uint64_t monotonicUs) {
    BootEvent event;
    // Changing screens cancels an unfinished timer click, rather than applying
    // it to controls on the new screen after the recognition delay.
    if (pending && !timerVisible) { pending = false; second = false; }
    if (down != pressed) {
      pressed = down;
      if (down) {
        downAt = now; downUs = monotonicUs; held = false;
        second = pending && timerVisible && uint32_t(now - releasedAt) <= DoubleMs;
        if (pending && !second) {
          event = {BootAction::Single, firstUs, now, true};
          pending = false;
        }
        timerAtDown = timerVisible;
      } else if (!held && timerAtDown == timerVisible) {
        if (second && pending) {
          event = {BootAction::Double, firstUs, now, true};
          pending = false; second = false;
        } else if (timerAtDown) {
          pending = true; firstUs = downUs; releasedAt = now;
        } else {
          event = {BootAction::Single, downUs, now, false};
        }
      }
    }
    if (pressed && !held && uint32_t(now - downAt) >= HoldMs) {
      held = true; pending = false; second = false;
      event = {BootAction::Home, downUs, now, timerAtDown};
    } else if (pending && !pressed && uint32_t(now - releasedAt) >= DoubleMs) {
      pending = false;
      event = {BootAction::Single, firstUs, now, true};
    }
    return event;
  }
 private:
  bool pressed = false, held = false, pending = false, second = false, timerAtDown = false;
  uint32_t downAt = 0, releasedAt = 0;
  uint64_t downUs = 0, firstUs = 0;
};
}
