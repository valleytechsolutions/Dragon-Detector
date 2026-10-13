// MIT: daily badge builds do not link the legacy GPL audio decoder.
#include "app.h"
static AppAudioStatus status;
void audioBegin(){}
void audioPlay(const String&){}
void audioRecord(){}
void audioStop(){}
void audioVolume(int){}
AppAudioStatus audioStatus(){return status;}
#ifndef BOARD_STOPWATCH
void boardBegin(){}
void boardLoop(){}
void audioBeep(){}
bool alertSound=false,alertVibration=false;
#endif
