#pragma once
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
// Internal flash replaces SD storage on StopWatch.
#define SD_MMC LittleFS
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "detection.h"

using dragon::Family;
using dragon::Confidence;
struct Contact {
  bool used=false,ble=false; uint8_t addressType=0,mac[6]{}; char name[64]{};
  int16_t rssi=-100; uint8_t channel=0; uint32_t seen=0,firstSeen=0,packets=0,lastLog=0;
  dragon::Match match{Family::Wifi,Confidence::None,""};
  dragon::Burst deauthBurst; uint16_t lastSequence=0; bool sequenceKnown=false;
  uint32_t wifiAlerts=0;
};
struct Alert { uint32_t at=0; char title[40]{},detail[128]{},mac[18]{}; };
constexpr size_t MAX_CONTACTS=160,MAX_ALERTS=40;
extern Contact contacts[MAX_CONTACTS];
extern Alert alerts[MAX_ALERTS];
extern uint32_t alertTotal, wifiFrames,bleFrames,eapolFrames,queueDrops,malformedFrames;
extern uint32_t storageDrops;
extern uint32_t channelFrames[14],channelDwellMs[14],wifiListenMs,bleListenMs;
extern bool sdReady,loggingEnabled,captureEnabled,radioEnabled,blePhase;
extern uint8_t wifiChannel,channelLock;
extern SemaphoreHandle_t sdMutex;
extern String sessionPath;
void macText(const uint8_t* mac,char* out);
void storageBegin();
void storageLoop();
void logContact(const Contact& c,const char* event);
void addAlert(const char* title,const char* detail,const uint8_t* mac=nullptr);
void writePcap(const uint8_t* data,size_t len,size_t original,uint64_t us);
bool setCapture(bool enabled);
void radioBegin();
void radioLoop();
void setRadio(bool enabled);
void uiBegin();
void uiLoop();
void uiShow(const String& name);
void audioBegin();
void runSelfTests();
void boardBegin();
void boardLoop();
void uiKey(bool home);
void uiStatus();
void testTouch(int x,int y,bool pressed);
void audioBeep();
bool badgeLoad(const String& path);
void badgeNext();
void badgeUnload();
void badgeView(void* parent);
void badgeLoop();
void badgeInit();
extern bool alertSound,alertVibration,badgeSlideshow;
extern String badgePath;
bool storageHasSpace(size_t bytes);
