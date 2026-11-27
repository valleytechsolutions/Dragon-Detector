file(GLOB app_sources "*.c" "*.cpp")
if(DRAGON_BOARD STREQUAL "waveshare_p4_knob")
    # Full UI; every other board adapter and the S3-only raw frame adapter stay out.
    list(FILTER app_sources EXCLUDE REGEX "/(display|board_amoled175c|board_stopwatch|board_xiao_round|board_waveshare_lite|board_lilygo_175|board_waveshare_185b|board_waveshare_143c|board_m5dial|board_crowpanel_21|board_watcher|audio_m5dial|ui_lite|lite_support|raw_frame_adapter)\\.cpp$")
    list(FILTER app_sources EXCLUDE REGEX "/p4_radio_(install|shim)\\.cpp$")
    idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "../include"
        PRIV_REQUIRES bt espressif__esp-extconn esp_driver_ppa esp_lcd esp_driver_gpio esp_driver_gptimer esp_driver_sdmmc sdmmc esp_driver_i2s esp_hw_support esp_timer fatfs arduino_tinyusb chmorgan__esp-libhelix-mp3)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_hosted_init" "-Wl,--wrap=usbd_edpt_xfer" "-Wl,--wrap=esp_wifi_80211_tx"
        "-Wl,--wrap=esp_wifi_set_csi_rx_cb" "-Wl,--wrap=esp_wifi_set_csi" "-Wl,--wrap=esp_wifi_set_csi_config"
        "-Wl,--wrap=GATTC_Write" "-Wl,--wrap=esp_extconn_boot" "-Wl,--wrap=esp_extconn_sdio_init"
        "-Wl,--wrap=esp_sip_write_mem" "-Wl,--wrap=esp_sip_bootup"
        "-Wl,--wrap=esp_extconn_trans_wifi_init" "-Wl,--wrap=esp_extconn_trans_bt_init")
else()
    list(FILTER app_sources EXCLUDE REGEX "/(display|board_amoled175c|board_stopwatch|board_xiao_round|board_waveshare_lite|ui_lite|lite_support)\\.cpp$")
    idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "../include")
endif()
