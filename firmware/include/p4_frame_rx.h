#pragma once
#include <stdbool.h>
#include <esp_wifi.h>
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t p4_read_frame_window(wifi_promiscuous_cb_t receive,bool (*wanted)(void));
#ifdef __cplusplus
}
#endif
