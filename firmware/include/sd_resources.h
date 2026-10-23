#pragma once
#include "board_pins.h"
#include <stdint.h>
#if BOARD_HAS_SD
void sdResourcesBegin();
void sdResourcesStatus();
const char* sdResourceVendor(const uint8_t*,bool,uint8_t);
#else
inline void sdResourcesBegin(){}
inline const char* sdResourceVendor(const uint8_t*,bool,uint8_t){return nullptr;}
#endif
