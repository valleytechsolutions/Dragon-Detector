#pragma once
// One-shot USB keyboard mode. Normal boots keep the S3's hardware USB
// serial/JTAG (flashing, auto-reset and serial tools work as usual). A request
// survives exactly one software restart in RTC memory; any later reset or a
// power cycle returns to USB serial.
namespace dragon::badusb {
// Call first in setup(), before Serial.begin(): consumes a pending request and
// returns the USB pins to serial/JTAG on normal boots.
void bootMode();
bool keyboardMode();
const char* keyboardScript(); // Selection carried into keyboard mode.
[[noreturn]] void restartIntoKeyboard(const char* script);
[[noreturn]] void restartIntoSerial();
}
