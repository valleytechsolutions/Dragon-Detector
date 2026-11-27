#pragma once
#ifdef BOARD_WAVESHARE_P4_KNOB
#include <stddef.h>
#include "p4_radar_snapshot.h"
#include "p4_ble_passive.h"
const char* p4RadarStatus();
bool p4RadarBluetoothAvailable();
uint8_t p4RadarScanMode();
bool p4RadarSetScanMode(unsigned mode);
bool p4RadarProfileSupported(unsigned mode);
uint32_t p4RadarIbeaconReports();
uint32_t p4RadarEddystoneReports();
const char* p4RadarBluetoothStatus();
bool p4RadarBleListening();
uint32_t p4RadarBleWindows();
uint32_t p4RadarBleReports();
bool p4RadarTakeBleSnapshot(p4_ble_snapshot_t& out);
uint16_t p4RadarLastApCount();
uint32_t p4RadarCompletedScans();
void p4RadarSetEnabled(bool enabled);
bool p4RadarRetry();
void p4TransportBootWindow();
bool p4TransportNeedsRestart();
const char* p4TransportRestartBlock();
bool p4RestartFailedTransport();
void p4RadioWakeForGatt();
bool p4RadarChannelSummary(uint16_t counts[P4_RADAR_CHANNELS], int8_t strongest[P4_RADAR_CHANNELS], uint32_t* completedMs);
// Owned by radioLoop on the application task. Valid until that task takes its next survey.
const p4_radar_wifi_snapshot_t* p4LatestWifiSurvey();
bool p4SaveWifiSurvey(const char* name,char* message,size_t size);
bool p4RadarAcquireClockNetwork(uint32_t timeoutMs=5000);
void p4RadarReleaseClockNetwork();
bool p4RadarTakeSnapshot(p4_radar_wifi_snapshot_t& out);
// Node sessions hold the same exclusive companion lease as clock networking.
bool p4NodeBlePrepare();
bool p4NodeBleStart();
bool p4NodeBleBusy();
void p4NodeBleDrain(void (*seen)(const uint8_t*,int));
void p4NodeBleStop();
#endif
