#pragma once
#include <Arduino.h>
#ifndef BOARD_RADAR_LITE
void networkBegin();
bool networkActive();
void networkLoop();
void networkStop();
void networkStatus(char*,size_t,bool readable=false);
bool networkCommand(const String&);
bool networkConnect(unsigned,bool clockOnly=false);
bool networkPingGateway();
bool networkScanLocal();
unsigned networkProfiles(char names[][33],unsigned slots[],unsigned capacity);
bool networkSaveResults();
#endif
