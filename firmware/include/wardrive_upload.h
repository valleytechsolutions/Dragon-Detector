#pragma once
#include "board_pins.h"
#include <Arduino.h>
struct WardriveUploadSnapshot {
  bool busy=false,cancelling=false,wigleConfigured=false,wdgConfigured=false;
  uint32_t total=0,done=0,accepted=0,pending=0,skipped=0,failed=0,sent=0,size=0;
  int http=0;
  char file[48]{},message[144]{},service[16]{};
};
#ifdef BOARD_AMOLED_175
void wardriveUploadBegin();
void wardriveUploadLoop();
bool wardriveUploadBusy();
bool wardriveUploadStart(bool wdg,bool checkOnly=false,bool retryUncertain=false);
void wardriveUploadCancel();
WardriveUploadSnapshot wardriveUploadSnapshot();
bool wardriveUploadCommand(const String&);
#else
inline void wardriveUploadBegin(){}
inline void wardriveUploadLoop(){}
inline bool wardriveUploadBusy(){return false;}
inline bool wardriveUploadStart(bool,bool=false,bool=false){return false;}
inline void wardriveUploadCancel(){}
inline WardriveUploadSnapshot wardriveUploadSnapshot(){return {};}
inline bool wardriveUploadCommand(const String&){return false;}
#endif
