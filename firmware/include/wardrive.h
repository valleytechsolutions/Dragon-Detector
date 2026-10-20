#pragma once
#include <Arduino.h>
#include "board_pins.h"
#ifdef BOARD_AMOLED_175
void wardriveBegin();
void wardriveLoop();
bool wardriveStart();
void wardriveStop();
bool wardriveActive();
bool wardriveBusy();
bool wardriveCommand(const String&);
void wardriveStatus(char*,size_t);
void wardriveWifi(const uint8_t*,size_t,int,uint8_t,uint32_t);
void wardriveBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,uint32_t);
void radioWardrive(bool);
#else
inline void wardriveBegin(){}
inline void wardriveLoop(){}
inline bool wardriveStart(){return false;}
inline void wardriveStop(){}
inline bool wardriveActive(){return false;}
inline bool wardriveBusy(){return false;}
inline bool wardriveCommand(const String&){return false;}
inline void wardriveStatus(char* out,size_t cap){snprintf(out,cap,"Requires AMOLED 1.75-G GPS profile");}
inline void wardriveWifi(const uint8_t*,size_t,int,uint8_t,uint32_t){}
inline void wardriveBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,uint32_t){}
#endif
