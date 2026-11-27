#ifdef BOARD_WAVESHARE_P4_KNOB
#include "p4_radar_scan.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_wifi.h"

static uint8_t reported_security(wifi_auth_mode_t auth) {
    switch(auth) {
    case WIFI_AUTH_OPEN: return P4_AP_OPEN;
    case WIFI_AUTH_WEP: return P4_AP_WEP;
    case WIFI_AUTH_WPA_PSK: return P4_AP_WPA;
    case WIFI_AUTH_WPA2_PSK: return P4_AP_WPA2;
    case WIFI_AUTH_WPA_WPA2_PSK: return P4_AP_WPA_WPA2;
    case WIFI_AUTH_WPA3_PSK: case WIFI_AUTH_WPA3_EXT_PSK: return P4_AP_WPA3;
    case WIFI_AUTH_WPA2_WPA3_PSK: case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return P4_AP_WPA2_WPA3;
    case WIFI_AUTH_ENTERPRISE: case WIFI_AUTH_WPA3_ENT_192: case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: case WIFI_AUTH_WPA_ENTERPRISE: return P4_AP_ENTERPRISE;
    case WIFI_AUTH_WAPI_PSK: return P4_AP_WAPI;
    case WIFI_AUTH_OWE: return P4_AP_OWE;
    case WIFI_AUTH_DPP: return P4_AP_DPP;
    default: return P4_AP_UNKNOWN;
    }
}

esp_err_t p4_read_passive_radar(p4_radar_wifi_snapshot_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    out->total = 0;
    out->count = 0;
    out->finished_ms = 0;
    memset(out->channel_counts, 0, sizeof(out->channel_counts));
    memset(out->channel_peaks, INT8_MIN, sizeof(out->channel_peaks));
    wifi_scan_config_t scan = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time.passive = 200,
    };
    esp_err_t result = esp_wifi_scan_start(&scan, true);
    if (result != ESP_OK) {
        return result;
    }
    uint16_t total = 0;
    uint16_t count = 0;
    result = esp_wifi_scan_get_ap_num(&total);
    for (uint16_t i = 0; result == ESP_OK && i < total; ++i) {
        wifi_ap_record_t record;
        result = esp_wifi_scan_get_ap_record(&record);
        if (result == ESP_OK) {
            if (record.primary >= 1 && record.primary <= P4_RADAR_CHANNELS) {
                const unsigned ch = record.primary - 1;
                ++out->channel_counts[ch];
                if (record.rssi < 0 && record.rssi > out->channel_peaks[ch])
                    out->channel_peaks[ch] = record.rssi;
            }
            if (count == P4_RADAR_MAX_APS) continue;
            p4_radar_ap_t *ap = &out->aps[count++];
            memcpy(ap->bssid, record.bssid, sizeof(ap->bssid));
            ap->ssid_length = (uint8_t)strnlen((const char *)record.ssid, sizeof(ap->ssid));
            memcpy(ap->ssid, record.ssid, ap->ssid_length);
            ap->rssi = record.rssi;
            ap->channel = record.primary;
            ap->security = reported_security(record.authmode);
        }
    }
    esp_err_t cleared = esp_wifi_clear_ap_list();
    if (result != ESP_OK) {
        return result;
    }
    if (cleared != ESP_OK) {
        return cleared;
    }
    out->total = total;
    out->count = count;
    out->finished_ms = (uint32_t)(esp_timer_get_time() / 1000);
    return ESP_OK;
}
#endif
