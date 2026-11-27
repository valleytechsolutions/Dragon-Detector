cmake_minimum_required(VERSION 3.21)

# Two boards build ESP-IDF from source: the RGB Elecrow (default) and the
# ESP32-P4 Knob, which keeps its own sdkconfig defaults, component lock and
# P4-only components so the Elecrow's resolved dependencies stay unchanged.
if(DRAGON_BOARD STREQUAL "waveshare_p4_knob")
    set(version_header "${CMAKE_SOURCE_DIR}/firmware/include/dragon_version.h")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${version_header}")
    file(STRINGS "${version_header}" version_line REGEX "^#define DRAGON_VERSION ")
    if(NOT version_line MATCHES "^#define DRAGON_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"$")
        message(FATAL_ERROR "Missing DRAGON_VERSION in ${version_header}")
    endif()
    set(PROJECT_VER "${CMAKE_MATCH_1}")
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/hardware/waveshare-p4-knob-175/sdkconfig.defaults")
    set(DRAGON_LOCK "${CMAKE_SOURCE_DIR}/hardware/waveshare-p4-knob-175/dependencies.lock")
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/hardware/waveshare-p4-knob-175/components")
else()
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/hardware/elecrow-rotary-21/sdkconfig.defaults")
    set(DRAGON_LOCK "${CMAKE_SOURCE_DIR}/dependencies.lock")
    include("${CMAKE_SOURCE_DIR}/hardware/elecrow-rotary-21/usb.cmake")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(DEPENDENCIES_LOCK "${DRAGON_LOCK}")
project(DragonDetector)

if(DRAGON_BOARD STREQUAL "waveshare_p4_knob")
    # extconn normally acquires the adjustable LDO4. This board already holds
    # that physical rail at 3.3 V for both the microSD and the radio.
    idf_component_get_property(dd_radio_transport espressif__esp-extconn COMPONENT_LIB)
    target_compile_definitions(${dd_radio_transport} PRIVATE
        sd_pwr_ctrl_new_on_chip_ldo=dragon_radio_fixed_rail
        sdmmc_card_init=dragon_extconn_card_init
        sdmmc_send_cmd_go_idle_state=dragon_extconn_cmd0)
    idf_component_get_property(p4_mp3_lib chmorgan__esp-libhelix-mp3 COMPONENT_LIB)
    target_compile_options(${p4_mp3_lib} PRIVATE "-include${CMAKE_SOURCE_DIR}/firmware/include/p4_mp3_alloc.h")
endif()

if(NOT DRAGON_BOARD STREQUAL "waveshare_p4_knob")
# PlatformIO compiles the CMake sources without running Ninja's embed commands.
foreach(asset IN ITEMS
    "espressif__esp_insights/server_certs/https_server.crt"
    "espressif__esp_rainmaker/server_certs/rmaker_mqtt_server.crt"
    "espressif__esp_rainmaker/server_certs/rmaker_claim_service_server.crt"
    "espressif__esp_rainmaker/server_certs/rmaker_ota_server.crt")
    get_filename_component(asset_name "${asset}" NAME)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DDATA_FILE=${CMAKE_SOURCE_DIR}/managed_components/${asset}"
            "-DSOURCE_FILE=${CMAKE_BINARY_DIR}/${asset_name}.S"
            -DFILE_TYPE=TEXT
            -P "$ENV{IDF_PATH}/tools/cmake/scripts/data_file_embed_asm.cmake"
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
endif()
