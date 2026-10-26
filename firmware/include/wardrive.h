#pragma once
#include <Arduino.h>
#include "board_pins.h"
struct WardriveSnapshot {
  bool running=false,busy=false,fixed=false,present=false,failed=false;
  uint32_t wifi=0,ble=0,seenWifi=0,seenBle=0,elapsedMs=0,noFix=0,late=0,duplicates=0,drops=0,gpsErrors=0,queued=0;
  uint64_t bytes=0;
  unsigned satellites=0;
  char file[96]{},message[120]{};
};
#if BOARD_HAS_GPS
WardriveSnapshot wardriveSnapshot();
void wardriveBegin();
void wardriveLoop();
bool wardriveStart();
void wardriveStop();
bool wardriveActive();
bool wardriveI2cBusy();
bool wardriveBusy();
bool wardriveCommand(const String&);
void wardriveStatus(char*,size_t);
void wardriveWifi(const uint8_t*,size_t,int,uint8_t,uint32_t);
void wardriveWifiResult(const uint8_t*,const uint8_t*,size_t,const char*,int,uint8_t,uint32_t);
void wardriveBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,uint32_t);
bool radioWardrive(bool);
void radioWardriveStatus();
#else
inline WardriveSnapshot wardriveSnapshot(){return {};}
inline void wardriveBegin(){}
inline void wardriveLoop(){}
inline bool wardriveStart(){return false;}
inline void wardriveStop(){}
inline bool wardriveActive(){return false;}
inline bool wardriveI2cBusy(){return false;}
inline bool wardriveBusy(){return false;}
inline bool wardriveCommand(const String&){return false;}
inline void wardriveStatus(char* out,size_t cap){snprintf(out,cap,"Requires AMOLED 1.75-G GPS profile");}
inline void wardriveWifi(const uint8_t*,size_t,int,uint8_t,uint32_t){}
inline void wardriveBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,uint32_t){}
#endif
