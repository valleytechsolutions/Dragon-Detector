#pragma once
#include <cstdint>

namespace dragon::p4 {
// extconn carries the classic ESP32 descriptor. The P4 SDK header omits its
// noise-floor byte and consequently reads the primary channel one byte early.
inline uint8_t wifiPrimaryChannel(const void* rxControl) {
  return static_cast<const uint8_t*>(rxControl)[10]&0x0f;
}
}
