# Arduino-ESP32 3.3.12's TinyUSB for the ESP32-P4, as prebuilt by Espressif's
# lib-builder for the same IDF (5.5.5). The hybrid build needs it for the USB
# serial console on the P4's USB-OTG port; the source component is not on the
# component registry. TinyUSB is MIT licensed (see docs/licenses and CREDITS.md).
set(libs "${IDF_PATH}/../framework-arduinoespressif32-libs/esp32p4")
if(NOT EXISTS "${libs}/lib/libarduino_tinyusb.a")
    message(FATAL_ERROR "framework-arduinoespressif32-libs for esp32p4 not found at ${libs}")
endif()
idf_component_register(
    INCLUDE_DIRS "${libs}/include/arduino_tinyusb/include" "${libs}/include/arduino_tinyusb/tinyusb/src"
    REQUIRES esp_hw_support usb)
add_prebuilt_library(arduino_tinyusb_prebuilt "${libs}/lib/libarduino_tinyusb.a"
    REQUIRES esp_hw_support esp_rom freertos log usb)
target_link_libraries(${COMPONENT_LIB} INTERFACE arduino_tinyusb_prebuilt)
