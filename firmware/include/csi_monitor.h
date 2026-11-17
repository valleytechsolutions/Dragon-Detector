#pragma once
#include <Arduino.h>
#ifndef BOARD_RADAR_LITE
#include "csi_networks.h"
bool csiActive();bool csiStart();void csiStop();void csiLoop();
void csiStatus(char*,size_t);bool csiCommand(const String&);
// Counts frames from the selected AP by PHY class while Room motion runs.
#include <esp_wifi_types.h>
void csiFrame(const wifi_promiscuous_pkt_t*);
// Room motion's own network choice. None of these touch the Wi-Fi attack
// target, a Hunt, a Watch rule or the radar channel lock.
struct CsiNetwork {uint8_t mac[6],channel;int8_t rssi;uint32_t age;dragon::csi::Evidence evidence;bool last;char name[33];};
unsigned csiNetworks(CsiNetwork* out,unsigned max,uint32_t fresh=dragon::csi::FreshMs); // ranked, best first
bool csiStartNetwork(const uint8_t* bssid);bool csiStartAuto();bool csiStartNext();
bool csiNoData();       // the running network gives no motion data (not heard / DSSS only)
bool csiNextName(char*,size_t);
const char* csiMessage();
void csiSurvey(bool);   // passive listening for the network list while the screen is open
#else
inline bool csiActive(){return false;}inline void csiStop(){}inline void csiLoop(){}
#endif
