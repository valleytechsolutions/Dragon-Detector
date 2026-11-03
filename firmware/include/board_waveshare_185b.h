#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef BOARD_WAVESHARE_185B
// The touch controller, RTC, fuel gauge and audio codecs share Wire (11/10).
// Lock directly only when calling a codec library that performs its own Wire I/O;
// the bounded register helpers below acquire this lock themselves.
bool waveshare185bI2cLock(uint32_t timeoutMs = 20);
void waveshare185bI2cUnlock();
bool waveshare185bI2cRead(uint8_t address, uint8_t reg, uint8_t* data, size_t count);
bool waveshare185bI2cWrite(uint8_t address, uint8_t reg, const uint8_t* data, size_t count);
void waveshare185bDiagnostics();
// UI owns navigation state; the driver supplies only fresh physical key events.
void ui185bCycle();
void ui185bHome();
bool ui185bTimerVisible();
#endif
