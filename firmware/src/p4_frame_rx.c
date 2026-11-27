#ifdef BOARD_WAVESHARE_P4_KNOB
#include "p4_frame_rx.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

esp_err_t p4_read_frame_window(wifi_promiscuous_cb_t receive,bool (*wanted)(void)) {
    if (!receive || !wanted) return ESP_ERR_INVALID_ARG;
    if (!wanted()) return ESP_ERR_INVALID_STATE;
    wifi_promiscuous_filter_t filter={WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA};
    esp_err_t result=esp_wifi_set_promiscuous_filter(&filter);
    if (result==ESP_OK) result=esp_wifi_set_promiscuous_rx_cb(receive);
    if (result==ESP_OK) result=esp_wifi_set_promiscuous(true);
    for (uint8_t channel=1;result==ESP_OK && channel<=11 && wanted();++channel) {
        result=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
        for (unsigned ms=0;result==ESP_OK && ms<200 && wanted();ms+=10)
            vTaskDelay(pdMS_TO_TICKS(10));
    }
    const esp_err_t stopped=esp_wifi_set_promiscuous(false);
    const esp_err_t cleared=esp_wifi_set_promiscuous_rx_cb(NULL);
    if (result==ESP_OK) result=stopped==ESP_OK?cleared:stopped;
    return result;
}
#endif
