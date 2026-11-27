#pragma once
#include <Arduino.h>
#include "detection.h"
#ifndef BOARD_RADAR_LITE
#ifdef BOARD_WAVESHARE_P4_KNOB
const char* p4SessionUnavailable(bool ble);
void p4RecordSessionContact(bool ble,const uint8_t* address,uint8_t addressType,int rssi,uint8_t channel,
                            const uint8_t* advertisement,size_t length,uint32_t seen);
#endif
bool surveySave(const char*);
void missionLoop();bool missionCommand(const String&);bool missionActive();bool missionStart(bool,const char*);void missionStop();void missionStatus(char*,size_t);
void missionWifi(const dragon::WifiFrame&,const uint8_t*,size_t,int,uint8_t);
void missionBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t);
#else
inline void missionLoop(){} inline bool missionCommand(const String&){return false;}inline bool missionActive(){return false;}
#endif
