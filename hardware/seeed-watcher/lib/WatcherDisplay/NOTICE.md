# Watcher display driver provenance

Vendored Espressif Apache-2.0 sources retain their copyright headers and adjacent license files.

- `esp_lcd_spd2010.c/.h`: espressif/esp-iot-solution commit `4962410c80a51cd5822da1d8d2041eb4285265f1`, component 2.0.0~1.
- `esp_lcd_touch_spd2010.c/.h`: same commit, component 2.1.0.
- `esp_lcd_touch.c/.h`: espressif/esp-bsp commit `f259a6abd5808019b1903a422092a4234ca95a51`.

Local changes: component version defines normally provided by CMake are included in the two driver C files for Arduino builds. The SPD2010 touch HDP retry loop is bounded to four iterations to avoid indefinitely blocking radio/UI work on a faulty touch response. Watcher integration supplies a Wire-based panel IO transport with bounded I2C timeouts. A background task samples touch and the rotary button independently of display rendering; LVGL consumes queued input on its own thread.

Board pin mapping was checked against Seeed-Studio/SenseCAP-Watcher-Firmware commit `8e37f7ccd3d836bb99f483e0f6e74e8252406644` and the Watcher v1.0 schematic. The integration is original project code; it does not redistribute factory firmware.
