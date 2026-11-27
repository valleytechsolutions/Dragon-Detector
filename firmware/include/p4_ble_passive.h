#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
#define P4_BLE_MAX_SIGHTINGS 24
typedef struct {
    uint32_t seen_ms;
    uint32_t reports;
    uint8_t address[6];
    uint8_t address_type;
    bool connectable;
    int8_t rssi;
    uint8_t name_length;
    uint8_t name[29];
    uint8_t payload_length;
    uint8_t payload[31];
} p4_ble_sighting_t;
typedef struct {
    uint32_t captured_ms;
    uint32_t listen_ms;
    uint32_t reports,ibeacon_reports,eddystone_reports;
    uint8_t count;
    p4_ble_sighting_t sightings[P4_BLE_MAX_SIGHTINGS];
} p4_ble_snapshot_t;

esp_err_t p4_ble_listen_init(void);
esp_err_t p4_ble_listen_quiesce(void);
const char *p4_ble_window_stage(void);
esp_err_t p4_ble_listen_window(p4_ble_snapshot_t *out, bool (*wanted)(void));
#ifdef __cplusplus
}
#endif
