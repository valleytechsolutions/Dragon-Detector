#pragma once
#include <Arduino.h>
#include "recon_modes.h"
#ifndef BOARD_RADAR_LITE
#include <FS.h>
size_t reconCaptureCapacity();
void reconBegin();
void reconLoop();
bool reconCommand(const String&);
dragon::ReconMode reconMode();
bool reconSetMode(dragon::ReconMode);
uint8_t reconRadio();
bool reconAll();
bool reconAccepts(dragon::Family,bool);
bool reconOwnsRadio();
bool reconStorageBusy();
bool reconRecording();
void reconStop();
void reconWifi(const uint8_t*,size_t,size_t,uint64_t,int,uint8_t);
void reconBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t);
void reconView(void*,bool tools=false);
void reconViewLoop();
void reconPage(const String&);
void reconLeave();
void radioApplyMode();
uint16_t reconHopDwell();uint8_t reconNextChannel(uint8_t);
fs::FS* bleLabStorage();
bool bleLabStorageSpace(size_t);
#else
inline uint16_t reconHopDwell(){return 180;}inline uint8_t reconNextChannel(uint8_t c){return c%11+1;}
inline void reconBegin(){}
inline void reconLoop(){}
inline bool reconCommand(const String&){return false;}
inline dragon::ReconMode reconMode(){return dragon::ReconMode::Radar;}
inline uint8_t reconRadio(){return 0;}
inline bool reconAll(){return false;}
inline bool reconAccepts(dragon::Family,bool){return true;}
inline bool reconOwnsRadio(){return false;}
inline bool reconStorageBusy(){return false;}
inline bool reconRecording(){return false;}
inline void reconStop(){}
inline void reconWifi(const uint8_t*,size_t,size_t,uint64_t,int,uint8_t){}
inline void reconBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t){}
#endif
