#pragma once
#include <Arduino.h>
#include "detection.h"
#ifndef BOARD_RADAR_LITE
void missionLoop();bool missionCommand(const String&);bool missionActive();bool missionStart(bool,const char*);void missionStop();void missionStatus(char*,size_t);
void missionWifi(const dragon::WifiFrame&,const uint8_t*,size_t,int,uint8_t);
void missionBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t);
#else
inline void missionLoop(){} inline bool missionCommand(const String&){return false;}inline bool missionActive(){return false;}
#endif
