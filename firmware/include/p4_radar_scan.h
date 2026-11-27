#pragma once

#include "esp_err.h"
#include "p4_radar_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

// Run on the single radio owner task, with extconn and the Wi-Fi station already
// started. The UI may publish a completed snapshot after ESP_OK.
esp_err_t p4_read_passive_radar(p4_radar_wifi_snapshot_t *out);

#ifdef __cplusplus
}
#endif
