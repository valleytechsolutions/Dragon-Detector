#include <Arduino.h>
#include "vendor_display.h"
// No fuel gauge on this board.
int batteryPercent(){return -1;}
bool batteryCharging(){return false;}
