#pragma once
#include <Arduino.h>
#ifndef BOARD_RADAR_LITE
bool recipeCommand(const String&);void recipeLoop();void recipeStop();bool recipeActive();void recipeStatus(char*,size_t);
#else
inline bool recipeCommand(const String&){return false;}inline void recipeLoop(){}inline void recipeStop(){}inline bool recipeActive(){return false;}
#endif
