// Compact profiles share timing/USB support. Dial adds its RTC and buzzer.
#include "app.h"
#include <Preferences.h>
#include <esp_timer.h>
#include <time.h>
dragon::Stopwatch stopwatch;
int clockOffsetMinutes=0;
bool rtcAvailable=false;
uint8_t displayRotation=0;
static Preferences timePrefs;
#ifndef BOARD_M5DIAL
bool alertSound=false,alertVibration=false,huntSound=false;
int audioFocus=-1;
void audioBegin(){} void audioLoop(){} void audioSave(){} void audioStop(){}
void audioPlay(const String&){} void audioRecord(){} void audioVolume(int){}
void audioBeep(){} void audioAlert(Family){}
bool audioReady(){return false;}
bool audioCommand(const String&){return false;}
AppAudioStatus audioStatus(){AppAudioStatus s;s.volume=0;strlcpy(s.message,"Lite / no audio hardware",sizeof(s.message));return s;}
#endif
void badgeInit(){} void badgeLoop(){} void badgeUnload(){} void badgeView(void*){}
bool badgeCommand(const String& c){if(c!="badge info")return false;Serial.println("BADGE_INFO " DRAGON_VERSION " 240 LITE " BOARD_NAME);return true;}
uint64_t stopwatchNow(){return esp_timer_get_time();}
void wearableBegin(){timePrefs.begin("dd-wearable",false);clockOffsetMinutes=constrain(timePrefs.getInt("utc-offset",0),-720,840);
#ifdef BOARD_M5DIAL
  boardSensorsBegin();uint32_t epoch=0;if(boardReadRtc(epoch))setWallTime(epoch);
#endif
}
bool configureClock(uint32_t epoch){if(!dragon::validEpoch(epoch))return false;
#ifdef BOARD_M5DIAL
if(!boardWriteRtc(epoch))return false;
#endif
setWallTime(epoch);return true;}
void configureClockOffset(int minutes){clockOffsetMinutes=constrain(minutes,-720,840);timePrefs.putInt("utc-offset",clockOffsetMinutes);}
void clockLocal(dragon::Calendar& date){uint32_t epoch=wallTime();if(!epoch){date={};return;}time_t local=int64_t(epoch)+clockOffsetMinutes*60;tm t{};gmtime_r(&local,&t);date={t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec};}
bool wearableCommand(const String& c){
#ifdef BOARD_M5DIAL
 if(c=="clock hardware"){uint32_t epoch=0;bool valid=boardReadRtc(epoch);Serial.printf("CLOCK_HARDWARE detected=%u valid=%u epoch=%lu\n",rtcAvailable,valid,valid?epoch:0);return true;}
 if(c=="clock diagnostics"){boardRtcDiagnostics();return true;}
 if(c=="clock status"){Serial.printf("CLOCK_STATUS rtc=%u valid=%u epoch=%lu offset=%d\n",rtcAvailable,wallTime()!=0,wallTime(),clockOffsetMinutes);return true;}
#else
 if(c=="clock hardware"){Serial.println("CLOCK_HARDWARE detected=0 valid=0 epoch=0");return true;}
 if(c=="clock status"){Serial.printf("CLOCK_STATUS rtc=0 valid=%u epoch=%lu offset=%d\n",wallTime()!=0,wallTime(),clockOffsetMinutes);return true;}
#endif
 if(c=="orientation status"){Serial.println("ORIENTATION_STATUS manual=1 degrees=0");return true;}
 if(c.startsWith("clock offset ")){int value;char extra;bool ok=sscanf(c.c_str(),"clock offset %d %c",&value,&extra)==1&&value>=-720&&value<=840;if(ok)configureClockOffset(value);Serial.println(ok?"CLOCK_OK":"CLOCK_ERROR offset");return true;}
 if(c=="stopwatch status"){Serial.printf("STOPWATCH_STATUS running=%u elapsed_us=%llu laps=%lu retained=%u\n",stopwatch.running,stopwatch.elapsed(stopwatchNow()),stopwatch.lapCount,unsigned(stopwatch.retained()));return true;}
 if(c=="stopwatch laps"){for(size_t i=0;i<stopwatch.retained();++i){auto* l=stopwatch.newest(i);Serial.printf("LAP %lu split_us=%llu total_us=%llu\n",l->number,l->split,l->total);}Serial.println("LAPS_END");return true;}
 if(c=="stopwatch start"){stopwatch.start(stopwatchNow());Serial.println("STOPWATCH_OK");return true;}
 if(c=="stopwatch pause"){stopwatch.pause(stopwatchNow());Serial.println("STOPWATCH_OK");return true;}
 if(c=="stopwatch reset"){Serial.println(stopwatch.reset()?"STOPWATCH_OK":"STOPWATCH_ERROR pause first");return true;}
 if(c=="stopwatch lap"||c=="stopwatch finish"){bool ok=c=="stopwatch lap"?stopwatch.lap(stopwatchNow()):stopwatch.finish(stopwatchNow());Serial.println(ok?"STOPWATCH_OK":"STOPWATCH_ERROR not running or zero interval");return true;}
 return false;
}
