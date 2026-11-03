#include <Arduino.h>
#ifdef BOARD_GUITION_185_BETA
#include "guition_display.h"
#else
#include "vendor_display.h"
#endif
#include "app.h"
// No fuel gauge on this board.
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}
void boardSensorsBegin(){rtcAvailable=false;}
bool boardReadRtc(uint32_t&){return false;}
bool boardWriteRtc(uint32_t){return false;}
