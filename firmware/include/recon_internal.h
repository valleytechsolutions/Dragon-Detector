#pragma once
#include "recon.h"
#ifndef BOARD_RADAR_LITE
#include "attack_frames.h"
#include "recon_analysis.h"
namespace dragon::toolkit {
#if defined(BOARD_AMOLED_175) || defined(BOARD_WATCHER)
constexpr unsigned ApCapacity=128,ClientCapacity=256;
#else
constexpr unsigned ApCapacity=48,ClientCapacity=96;
#endif
struct Ap {bool used=false;uint8_t mac[6]{},channel=0,security=0;int8_t rssi=-127;uint32_t seen=0,packets=0;char name[33]{};uint16_t length=0;uint8_t beacon[1024]{};};
struct Client {bool used=false;uint8_t mac[6]{},ap[6]{};int8_t rssi=-127;uint32_t seen=0,packets=0;};
struct Status {bool active=false,saving=false,recording=false,bleCapture=false;tx::AttackMode attack=tx::AttackMode::Beacon;uint32_t started=0,accepted=0,errors=0,responses=0,eapol=0,records=0;int error=0;size_t bytes=0;char message[128]="Select a mode or open the radio tools";};
extern analysis::Counters counters;extern analysis::Probe* probes;
extern analysis::Filter captureFilter;extern bool captureSelected;extern uint16_t hopMask,hopDwell;
bool setCaptureFilter(unsigned,bool);
extern Ap* aps;extern Client* clients;extern Status status;extern tx::AttackTarget target;
extern uint16_t seconds,rate;
bool selectAp(unsigned);bool selectClient(unsigned);bool start(tx::AttackMode);void stop();
bool captureStart(bool ble,bool eapol=false);void captureStop();bool captureSave();
bool available(tx::AttackMode,char*,size_t);bool listAdd(const char*);bool listClear();unsigned listCount();
void say(const char*);unsigned fileList(char names[][48],size_t* sizes,unsigned max);
bool fileRemove(const char*);bool fileExport(const char*);
}
#endif
