#pragma once
// USB drive mode (Waveshare ESP32-P4 Knob only). While it is on, the PC sees the
// TF card as a USB drive on the same USB-C port as the serial console, and
// Dragon leaves the card alone: logging and captures are closed, sdReady is
// false and the FAT volume is unmounted. Leaving the mode (Done, eject on the
// PC, unplug, or "usb drive off") mounts the card again as at startup.
// Other boards compile it out; see usb_drive.cpp.
#include <Arduino.h>
#if defined(BOARD_WAVESHARE_P4_KNOB)
#define DRAGON_USB_DRIVE 1
enum class UsbDriveState:uint8_t{Off,Waiting,Mounted,Busy};
// True from a successful start until the card is mounted for Dragon again.
bool usbDriveOn();
UsbDriveState usbDriveState();
// Hands the card to the PC. False (and nothing changed) when refused; why holds
// a screen-ready reason either way.
bool usbDriveStart(char* why,size_t length);
// Asks to give the card back; usbDriveLoop() finishes it from the main loop.
void usbDriveStop(const char* reason);
// Done on the screen: false when a second tap is needed to confirm because
// the PC still has the card open or wrote to it a moment ago.
bool usbDriveDone();
bool usbDriveConfirming();
// Screen text: the state line, and the read/written/card size line.
const char* usbDriveStateText();
void usbDriveDetail(char* out,size_t length);
void usbDriveLoop();
// "usb drive on|off|status", and the card commands refused while the PC has it.
bool usbDriveCommand(const String& c);
#else
inline bool usbDriveOn(){return false;}
inline void usbDriveLoop(){}
inline bool usbDriveCommand(const String&){return false;}
#endif
