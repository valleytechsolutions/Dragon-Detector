#pragma once
#include <Arduino.h>
#ifndef BOARD_RADAR_LITE
bool csiActive();bool csiStart();void csiStop();void csiLoop();
void csiStatus(char*,size_t);bool csiCommand(const String&);
// Counts frames from the selected AP by PHY class while Room motion runs.
#include <esp_wifi_types.h>
void csiFrame(const wifi_promiscuous_pkt_t*);
#else
inline bool csiActive(){return false;}inline void csiStop(){}inline void csiLoop(){}
#endif
