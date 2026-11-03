#pragma once
#include <stdint.h>

namespace waveshare185b {
// A radar redraw takes longer than 100 ms on this panel. During capture leave
// non-rendering turns for receive processing and SD drains; input is still
// serviced on every application loop.
inline constexpr uint32_t uiRefreshIntervalMs(bool timer, bool radar, bool capturing) {
  return timer ? 50 : radar && capturing ? 250 : 100;
}
// The existing 3 ms elapsed-time bound still limits each SD drain. Permit more
// small records to fit within it when the card can write them quickly.
inline constexpr unsigned SdWriteBatchRecords = 32;
}
