cmake_minimum_required(VERSION 3.21)

# Only the RGB Elecrow board builds ESP-IDF from source. Other boards use Arduino.
set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/hardware/elecrow-rotary-21/sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(DragonDetector)

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
