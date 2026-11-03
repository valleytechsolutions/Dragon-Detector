#pragma once
#include "board_pins.h"
#include <stdint.h>
#if BOARD_HAS_SD
void sdResourcesBegin();
void sdResourcesStatus();
const char* sdResourceVendor(const uint8_t*,bool,uint8_t);
bool sdResourcesReady();
uint32_t sdResourceVendorCount();
#ifdef BOARD_WAVESHARE_185B
const char* sdResourceBleLabel(uint8_t kind,uint16_t value);
uint32_t sdResourceBleLabelCount();
bool sdLookupNamesReady();
const char* sdResourceVendorState();
const char* sdResourceBleLabelState();
#endif
#else
inline void sdResourcesBegin(){}
inline const char* sdResourceVendor(const uint8_t*,bool,uint8_t){return nullptr;}
inline bool sdResourcesReady(){return false;}
inline uint32_t sdResourceVendorCount(){return 0;}
#endif
