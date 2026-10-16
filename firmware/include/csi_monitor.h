#pragma once
#include <Arduino.h>
#ifndef BOARD_RADAR_LITE
bool csiActive();bool csiStart();void csiStop();void csiLoop();
void csiStatus(char*,size_t);bool csiCommand(const String&);
#else
inline bool csiActive(){return false;}inline void csiStop(){}inline void csiLoop(){}
#endif
