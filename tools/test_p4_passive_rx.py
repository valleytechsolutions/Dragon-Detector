"""Exercise the P4 passive AP reader against synthetic driver responses."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "firmware/include"
SOURCE = ROOT / "firmware/src/p4_radar_scan.c"

ERR_HEADER = """
#pragma once
typedef int esp_err_t;
enum { ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE };
"""

WIFI_HEADER = """
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
enum { WIFI_SCAN_TYPE_ACTIVE, WIFI_SCAN_TYPE_PASSIVE };
typedef enum {WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_ENTERPRISE, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK,
    WIFI_AUTH_WAPI_PSK, WIFI_AUTH_OWE, WIFI_AUTH_WPA3_ENT_192, WIFI_AUTH_WPA3_EXT_PSK,
    WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE, WIFI_AUTH_DPP, WIFI_AUTH_WPA3_ENTERPRISE,
    WIFI_AUTH_WPA2_WPA3_ENTERPRISE, WIFI_AUTH_WPA_ENTERPRISE} wifi_auth_mode_t;
typedef struct { bool show_hidden; int scan_type; struct {uint32_t passive;} scan_time; } wifi_scan_config_t;
typedef struct { uint8_t ssid[33], bssid[6], primary; int8_t rssi; wifi_auth_mode_t authmode; } wifi_ap_record_t;
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*, bool);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t*);
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t*);
esp_err_t esp_wifi_clear_ap_list(void);
"""

CHECKS = """
#include <assert.h>
#include <string.h>
#include "p4_radar_scan.h"
#include "esp_wifi.h"

static unsigned total, reads, starts, clears, fail_record, fail_start, fail_clear;
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *cfg, bool block) {
    ++starts;
    assert(block && cfg->scan_type == WIFI_SCAN_TYPE_PASSIVE && cfg->scan_time.passive == 200);
    return fail_start ? ESP_ERR_INVALID_STATE : ESP_OK;
}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *out) { *out = total; return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *ap) {
    if (++reads == fail_record) return ESP_ERR_INVALID_STATE;
    memset(ap, 0, sizeof(*ap));
    ap->rssi = -63; ap->primary = 13; ap->bssid[5] = reads;
    ap->authmode = (wifi_auth_mode_t)(reads-1);
    if (reads == 33) { ap->primary = 14; ap->rssi = -54; }
    if (reads == 34) { ap->primary = 0; }
    if (reads == 35) { ap->rssi = -40; }
    // Simulate a malformed vendor response with no NUL in all 33 bytes.
    memset(ap->ssid, 'Z', sizeof(ap->ssid));
    return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list(void) { ++clears; return fail_clear ? ESP_ERR_INVALID_STATE : ESP_OK; }
int64_t esp_timer_get_time(void) { return 3654321; }

int main(void) {
    p4_radar_wifi_snapshot_t result = { .total=97, .count=21, .finished_ms=99 };
    assert(p4_read_passive_radar(NULL) == ESP_ERR_INVALID_ARG && !starts);
    total=42;
    assert(p4_read_passive_radar(&result)==ESP_OK && result.total==42 && result.count==P4_RADAR_MAX_APS);
    assert(result.finished_ms==3654 && result.aps[31].bssid[5]==32);
    assert(result.aps[31].ssid_length==32 && result.aps[31].ssid[31]=='Z');
    assert(result.aps[31].rssi==-63 && result.aps[31].channel==13 && reads==42 && clears==1);
    assert(result.aps[WIFI_AUTH_OPEN].security==P4_AP_OPEN);
    assert(result.aps[WIFI_AUTH_WPA2_PSK].security==P4_AP_WPA2);
    assert(result.aps[WIFI_AUTH_WPA3_PSK].security==P4_AP_WPA3);
    assert(result.aps[WIFI_AUTH_OWE].security==P4_AP_OWE);
    assert(result.aps[31].security==P4_AP_UNKNOWN);
    assert(!strcmp(p4_ap_security_name(result.aps[WIFI_AUTH_WPA2_WPA3_PSK].security),"WPA2 / WPA3"));
    assert(result.channel_counts[12]==40 && result.channel_peaks[12]==-40);
    assert(result.channel_counts[13]==1 && result.channel_peaks[13]==-54);
    total=0;
    assert(p4_read_passive_radar(&result)==ESP_OK && !result.count && !result.total && clears==2);
    assert(result.channel_counts[12]==0 && result.channel_peaks[12]==INT8_MIN);
    reads=0; total=5; fail_record=2;
    assert(p4_read_passive_radar(&result)==ESP_ERR_INVALID_STATE && !result.count && !result.total && !result.finished_ms && clears==3);
    fail_record=0; fail_clear=1;
    assert(p4_read_passive_radar(&result)==ESP_ERR_INVALID_STATE && !result.count && clears==4);
    fail_clear=0; fail_start=1;
    assert(p4_read_passive_radar(&result)==ESP_ERR_INVALID_STATE && clears==4 && !result.count);
}
"""

with TemporaryDirectory() as tmp:
    test = Path(tmp)
    (test / "esp_err.h").write_text(ERR_HEADER)
    (test / "esp_wifi.h").write_text(WIFI_HEADER)
    (test / "esp_timer.h").write_text("#include <stdint.h>\nint64_t esp_timer_get_time(void);\n")
    (test / "test.c").write_text(CHECKS)
    exe = test / ("check.exe" if sys.platform == "win32" else "check")
    subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=gnu17", "-Wall", "-Werror", "-UNDEBUG",
                    "-DBOARD_WAVESHARE_P4_KNOB", "-I", str(test), "-I", str(INCLUDE),
                    str(SOURCE), str(test / "test.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("P4 passive scan bounds, empty results, and cleanup passed.")
