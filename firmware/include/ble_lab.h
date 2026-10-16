#pragma once
#include <Arduino.h>
#include "ble_profile.h"
#ifndef BOARD_RADAR_LITE
void bleLabBegin();
void bleLabLoop();
bool bleLabCommand(const String&);
bool bleLabOwnsRadio();
void bleLabStop();
void bleLabObserve(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t);
void bleLabView(void*);
void bleLabViewLoop();
void bleLabSelect(const uint8_t*,uint8_t,const char*);
void radioRebindBle();
#else
inline void bleLabBegin(){}
inline void bleLabLoop(){}
inline bool bleLabCommand(const String&){return false;}
inline bool bleLabOwnsRadio(){return false;}
inline void bleLabStop(){}
inline void bleLabObserve(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t){}
#endif
