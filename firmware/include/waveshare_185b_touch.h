#pragma once
#include <stddef.h>
#include <stdint.h>

namespace waveshare185b {
constexpr uint32_t InputMaxAgeMs = 500;
inline bool inputFresh(uint32_t queuedAt, uint32_t now) {
  return uint32_t(now - queuedAt) <= InputMaxAgeMs;
}
enum class TouchDecision { Accept, Suppress, Reset };
struct TouchAgeGuard {
  bool waitForRelease = false;
  TouchDecision check(bool pressed, uint32_t queuedAt, uint32_t now, bool resync = false, bool validSample = true) {
    if (resync || !inputFresh(queuedAt, now)) {
      waitForRelease = true;
      return TouchDecision::Reset;
    }
    if (waitForRelease) {
      // After discarding history, a finger already down cannot become a new
      // click on whatever screen is now visible. Require a fresh release.
      if (!pressed && validSample) waitForRelease = false;
      return TouchDecision::Suppress;
    }
    return TouchDecision::Accept;
  }
};
// CST816S register 0x01: gesture, finger count, X high/low, Y high/low.
// A stale or malformed packet must never manufacture a press at a clamped edge.
struct TouchState {
  bool pressed = false;
  uint16_t x = 0, y = 0;
  uint32_t lastGoodAt = 0;
  bool update(const uint8_t* packet, size_t length, bool ioOk, uint32_t now) {
    const bool valid = ioOk && packet && length == 6 && packet[1] <= 1;
    if (valid && packet[1] == 0) { pressed = false; lastGoodAt = now; return true; }
    if (valid) {
      const uint16_t nextX = ((packet[2] & 0x0f) << 8) | packet[3];
      const uint16_t nextY = ((packet[4] & 0x0f) << 8) | packet[5];
      if (nextX < 360 && nextY < 360) {
        x = nextX; y = nextY; pressed = true; lastGoodAt = now; return true;
      }
    }
    // A momentary busy bus does not split a held touch into repeated clicks.
    // Recover release if an IRQ/packet is lost or the controller disappears.
    if (pressed && uint32_t(now - lastGoodAt) >= 120) pressed = false;
    return false;
  }
};
}
