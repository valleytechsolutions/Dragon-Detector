#pragma once
#include <Arduino.h>

struct BadUsbStatus {
  bool connected, running, selected;
  unsigned line;
  const char* name;
  const char* message;
  const char* medium;
  bool keyboardMode; // Booted as a USB keyboard; normal boots use USB serial.
};
bool badusbSupported();
const char* badusbUnavailableReason();
void badusbBegin();
void badusbLoop();
bool badusbCommand(const String&);
void badusbRefresh();
unsigned badusbCount();
const char* badusbName(unsigned);
bool badusbSelect(unsigned);
// Normal mode: restarts as a USB keyboard with the selection kept. Keyboard
// mode: starts typing the selected script.
bool badusbRun();
void badusbStop();
// Frees the idle script buffer (not in keyboard mode, a run or an upload); the next
// refresh selects the same script again. A node start calls it.
void badusbRelease();
bool badusbKeyboardMode();
// Leave keyboard mode: restarts into the normal USB serial firmware.
void badusbExitKeyboard();
const char* badusbPreview();
BadUsbStatus badusbStatus();
