#pragma once

#include <stdint.h>

#define P4_RADAR_MAX_APS 32
#define P4_RADAR_CHANNELS 14

enum p4_ap_security {
    P4_AP_UNKNOWN, P4_AP_OPEN, P4_AP_WEP, P4_AP_WPA, P4_AP_WPA2, P4_AP_WPA_WPA2,
    P4_AP_WPA3, P4_AP_WPA2_WPA3, P4_AP_ENTERPRISE, P4_AP_WAPI, P4_AP_OWE, P4_AP_DPP
};

static inline const char* p4_ap_security_name(uint8_t security) {
    static const char* const names[]={"Unreported","Open","WEP","WPA","WPA2","WPA / WPA2",
        "WPA3","WPA2 / WPA3","Enterprise","WAPI","Enhanced Open (OWE)","DPP"};
    return security < sizeof(names)/sizeof(names[0]) ? names[security] : names[P4_AP_UNKNOWN];
}

typedef struct {
    uint8_t bssid[6];
    uint8_t ssid[32];
    uint8_t ssid_length;
    uint8_t channel;
    uint8_t security;
    int8_t rssi;
} p4_radar_ap_t;

typedef struct {
    uint32_t finished_ms;
    uint16_t total;
    uint16_t count;
    uint16_t channel_counts[P4_RADAR_CHANNELS];
    int8_t channel_peaks[P4_RADAR_CHANNELS];
    p4_radar_ap_t aps[P4_RADAR_MAX_APS];
} p4_radar_wifi_snapshot_t;
