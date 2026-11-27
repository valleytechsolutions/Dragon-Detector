#pragma once
#include "board_pins.h"
#include <stdint.h>
// BLE company and service names from the card: the 1.85B's only source, and
// the P4 Knob's first choice over its built-in table.
#if defined(BOARD_WAVESHARE_185B) || defined(BOARD_WAVESHARE_P4_KNOB)
#define SD_BLE_LABELS 1
#endif
#if BOARD_HAS_SD
void sdResourcesBegin();
void sdResourcesStatus();
const char* sdResourceVendor(const uint8_t*,bool,uint8_t);
bool sdResourcesReady();
uint32_t sdResourceVendorCount();
#ifdef SD_BLE_LABELS
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
