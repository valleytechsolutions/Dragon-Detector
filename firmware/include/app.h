#pragma once
#include <Arduino.h>
#include <FS.h>
#include "board_pins.h"
#if BOARD_HAS_SD
#if defined(BOARD_SD_SPI)
#include <SD.h>
#define CardFS SD
#else
#include <SD_MMC.h>
#define CardFS SD_MMC
#endif
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "detection.h"
#include "tracking.h"
#include "monitor.h"
#include "contacts.h"
#include "version.h"
#include "traffic.h"
#include "wearable.h"
extern dragon::TrafficStatus trafficStatus;
unsigned recentDisconnects();

using dragon::Family;
using dragon::Confidence;
struct Alert { uint32_t at=0; char title[40]{},detail[128]{},mac[18]{}; };
constexpr size_t MAX_ALERTS=40;
extern ContactStore contacts;
extern Alert alerts[MAX_ALERTS];
extern uint32_t alertTotal, wifiFrames,bleFrames,eapolFrames,queueDrops,malformedFrames;
extern uint32_t storageDrops;
extern uint32_t channelFrames[14],channelDwellMs[14],wifiListenMs,bleListenMs;
extern bool sdReady,loggingEnabled,captureEnabled,radioEnabled,blePhase;
extern uint8_t wifiChannel,channelLock;
extern bool activeBle;
extern bool axonPriority;
extern uint32_t axonAlertCount;
void axonStatus();
extern uint32_t radioMaxBatchUs,radioMaxLagMs,contactEvictions,contactCacheHits,contactCacheMisses;
extern uint16_t radioQueueHigh;
void setActiveBle(bool enabled);
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
bool radioStandby();
void setRadioStandby(bool enabled);
void uiBegin();
void uiLoop();
void uiShow(const String& name);
void audioBegin();
void audioPlay(const String& path);
void audioRecord();
void audioStop();
void audioVolume(int value);
enum class AudioMode:uint8_t {Idle,Playing,Recording,Error};
struct AppAudioStatus { AudioMode mode=AudioMode::Idle; char message[100]{}; uint32_t seconds=0; uint16_t peak=0; uint8_t volume=50; };
AppAudioStatus audioStatus();
void runSelfTests();
int batteryPercent(); // -1 when no battery or fuel gauge
bool batteryCharging();

extern int trackedContact;
extern bool candidatesOnly;
extern bool radarRecent;
bool radarVisible(const Contact& c,uint32_t now);
void trackContact(int index);
void badgeInit();
void badgeView(void* parent);
void badgeUnload();
void badgeLoop();
bool badgeCommand(const String& command);
void boardBegin();
void boardLoop();
void uiKey(bool home);
bool uiCommand(const String& command);
void audioBeep();
extern bool alertSound,alertVibration;

extern dragon::DetectorSettings detector;
dragon::DeviceKey contactKey(const Contact& c);
enum DeviceRule:uint8_t { RuleNone=0,RuleIgnore=1,RuleWatch=2,RuleSnooze=4 };
uint8_t deviceRules(const dragon::DeviceKey& key);
bool toggleRule(const dragon::DeviceKey& key,DeviceRule flag);
bool ruleAt(size_t i,dragon::DeviceKey& key,uint8_t& flags);
void detectorBegin();
void detectorSave();
void detectorLoop();
bool detectorCommand(const String& command);
bool emitDetection(Contact& c,Family family,Confidence confidence,const char* evidence,bool force=false);
void uiNotify(const char* title,const char* detail,const dragon::DeviceKey* key,bool watched);
void displayMac(const uint8_t* mac,char* out);
String displayName(const char* name);
bool huntDevice(const dragon::DeviceKey& key,uint8_t channel=0);
int findContact(const dragon::DeviceKey& key);
struct HistoryRecord {
  uint32_t magic=0,sequence=0,firstEpoch=0,lastEpoch=0,firstUptime=0,lastUptime=0,boot=0,encounters=0;
  dragon::DeviceKey key;
  uint8_t family=0,confidence=0,channel=0,reserved=0;
  int16_t peak=-127,rssi=-127;
  char name[64]{},evidence[128]{};
  uint32_t checksum=0;
};
constexpr size_t HISTORY_LIMIT=512,RULE_LIMIT=64;
extern HistoryRecord* historyRecords;
extern size_t historyCount;
extern bool historyReady;
void historyObserve(const Contact& c,Family family,Confidence confidence,const char* evidence);
void historyClear();
bool historyExport();
uint32_t wallTime();
void setWallTime(uint32_t epoch);
bool foxTransmit();
void setFoxTransmit(bool enabled);

void audioLoop();
void audioSave();
void audioAlert(Family family);
bool audioReady();
bool audioCommand(const String& command);
extern bool huntSound;
extern int audioFocus;

extern dragon::Stopwatch stopwatch;
extern uint8_t displayRotation;
extern int clockOffsetMinutes;
extern bool rtcAvailable;
void wearableBegin();
bool wearableCommand(const String& command);
uint64_t stopwatchNow();
bool configureClock(uint32_t epoch);
void configureClockOffset(int minutes);
void clockLocal(dragon::Calendar& date);
bool configureOrientation(int degrees); // saved clockwise angle
void boardSensorsBegin();
bool boardReadRtc(uint32_t& epoch);
bool boardWriteRtc(uint32_t epoch);
bool uiStopwatchKey(bool start,uint64_t at=0);
