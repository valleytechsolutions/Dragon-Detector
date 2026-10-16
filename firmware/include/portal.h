#pragma once
#include <Arduino.h>
#ifdef BOARD_AMOLED_175C
bool portalActive();bool portalStart();void portalStop();void portalLoop();bool portalCommand(const String&);void portalStatus(char*,size_t,bool readable=false);
#else
inline bool portalActive(){return false;}inline void portalStop(){}inline void portalLoop(){}
#endif
