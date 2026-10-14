#include <Arduino.h>
#include "vendor_display.h"
#include "app.h"
// No fuel gauge on this board.
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}
void boardSensorsBegin(){rtcAvailable=false;imuAvailable=false;}
bool boardReadAcceleration(float&,float&,float&){return false;}
bool boardReadRtc(uint32_t&){return false;}
bool boardWriteRtc(uint32_t){return false;}
