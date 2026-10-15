#pragma once
#include <Arduino.h>
#include "recon_modes.h"
#ifdef BOARD_AMOLED_175C
#include <FS.h>
void reconBegin();
void reconLoop();
bool reconCommand(const String&);
dragon::ReconMode reconMode();
bool reconSetMode(dragon::ReconMode);
uint8_t reconRadio();
bool reconAll();
bool reconAccepts(dragon::Family,bool);
bool reconOwnsRadio();
bool reconRecording();
void reconStop();
void reconWifi(const uint8_t*,size_t,size_t,uint64_t,int,uint8_t);
void reconBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t);
void reconView(void*,bool tools=false);
void reconViewLoop();
void reconLeave();
void radioApplyMode();
fs::FS* bleLabStorage();
bool bleLabStorageSpace(size_t);
#else
inline void reconBegin(){}
inline void reconLoop(){}
inline bool reconCommand(const String&){return false;}
inline dragon::ReconMode reconMode(){return dragon::ReconMode::Radar;}
inline uint8_t reconRadio(){return 0;}
inline bool reconAll(){return false;}
inline bool reconAccepts(dragon::Family,bool){return true;}
inline bool reconOwnsRadio(){return false;}
inline bool reconRecording(){return false;}
inline void reconStop(){}
inline void reconWifi(const uint8_t*,size_t,size_t,uint64_t,int,uint8_t){}
inline void reconBle(const uint8_t*,uint8_t,int,const uint8_t*,size_t,size_t,uint32_t){}
#endif
