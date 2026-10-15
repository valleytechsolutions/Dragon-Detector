// Deliberately absent Lite peripherals: no audio, badge, stopwatch or RTC UI.
#include "app.h"
bool alertSound=false,alertVibration=false,huntSound=false;
int audioFocus=-1;
void audioBegin(){} void audioLoop(){} void audioSave(){} void audioStop(){}
void audioPlay(const String&){} void audioRecord(){} void audioVolume(int){}
void audioBeep(){} void audioAlert(Family){}
bool audioReady(){return false;}
bool audioCommand(const String&){return false;}
AppAudioStatus audioStatus(){AppAudioStatus s;s.volume=0;strlcpy(s.message,"Lite / no audio hardware",sizeof(s.message));return s;}
void badgeInit(){} void badgeLoop(){} void badgeUnload(){} void badgeView(void*){}
bool badgeCommand(const String& c){if(c!="badge info")return false;Serial.println("BADGE_INFO " DRAGON_VERSION " 240 RADAR_ONLY " BOARD_NAME);return true;}
void wearableBegin(){} bool wearableCommand(const String&){return false;}
bool configureClock(uint32_t){return false;}
