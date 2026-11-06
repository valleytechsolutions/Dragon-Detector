#pragma once
#include <stddef.h>
#include <stdint.h>

namespace waveshare185b {
constexpr uint32_t InputMaxAgeMs = 500;
inline bool inputFresh(uint32_t queuedAt, uint32_t now) {
  return uint32_t(now - queuedAt) <= InputMaxAgeMs;
}
// CST816S register 0x01: gesture, finger count, X high/low, Y high/low.
// A stale or malformed packet must never manufacture a press at a clamped edge.
inline bool supportedTouchChip(uint8_t chip) {
  return chip == 0xb4 || chip == 0xb5 || chip == 0xb6; // CST816S, T and D
}
struct TouchState {
  bool pressed = false;
  uint16_t x = 0, y = 0;
  uint32_t lastGoodAt = 0;
  bool update(const uint8_t* packet, size_t length, bool ioOk, uint32_t now) {
    const bool complete = ioOk && packet && length == 6;
    const unsigned fingers = complete ? packet[1] & 0x0f : 15;
    const unsigned event = complete ? packet[2] >> 6 : 3;
    // Some firmware revisions leave the count at one for a lift report.
    // Release at the last valid position; the lift coordinates may be unused.
    if (fingers == 0 || (fingers == 1 && event == 1)) {
      pressed = false; lastGoodAt = now; return true;
    }
    if (fingers == 1 && (event == 0 || event == 2)) {
      const uint16_t nextX = ((packet[2] & 0x0f) << 8) | packet[3];
      const uint16_t nextY = ((packet[4] & 0x0f) << 8) | packet[5];
      if (nextX < 360 && nextY < 360) {
        x = nextX; y = nextY; pressed = true; lastGoodAt = now; return true;
      }
    }
    // A momentary busy bus does not split a held touch into repeated clicks.
    // On a prolonged fault the consumer cancels instead of clicking on lift.
    if (pressed && uint32_t(now - lastGoodAt) >= 120) pressed = false;
    return false;
  }
};
}
