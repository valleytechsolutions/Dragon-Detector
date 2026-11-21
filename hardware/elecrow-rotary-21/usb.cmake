# Arduino-as-an-IDF-component does not supply the TinyUSB component included in
# ordinary Arduino builds. Use the exact upstream revisions recorded in the
# Arduino 3.0.7 SDK's versions.txt, with checked downloads kept in .pio/build.
include(FetchContent)
FetchContent_Declare(dragon_usb_builder
    URL https://codeload.github.com/espressif/esp32-arduino-lib-builder/tar.gz/769c168
    URL_HASH SHA256=c72c77a1f91e0e0c8682e277e56de2cddcb23bae1ff4d89cb8641a59d34a8586)
FetchContent_GetProperties(dragon_usb_builder)
if(NOT dragon_usb_builder_POPULATED)
    FetchContent_Populate(dragon_usb_builder)
endif()
set(dragon_usb_component "${dragon_usb_builder_SOURCE_DIR}/components/arduino_tinyusb")
FetchContent_Declare(dragon_tinyusb
    URL https://codeload.github.com/hathach/tinyusb/tar.gz/8b1e40c3e
    URL_HASH SHA256=eae9b9c2b5d4bc200a43b084b8a64fb1aee4739694f0c7b68657ae12b519ad36
    SOURCE_DIR "${dragon_usb_component}/tinyusb")
FetchContent_GetProperties(dragon_tinyusb)
if(NOT dragon_tinyusb_POPULATED)
    FetchContent_Populate(dragon_tinyusb)
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${dragon_usb_component}")
