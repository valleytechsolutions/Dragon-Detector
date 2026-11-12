# Waveshare ESP32-S3-Touch-AMOLED-1.43C panel data

The ten CO5300 start-up register values in `firmware/src/board_waveshare_143c.cpp` (`panelInit`), the 40 MHz QSPI settings and the column offset follow Waveshare's official board examples for the ESP32-S3-Touch-AMOLED-1.43C, licensed under Apache-2.0. The complete license is included alongside this notice.

Source: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.43C
Revision: `8cea11ddae6167dd591d1f5465fd7b615080360f`
Source files: `examples/esp-idf/05_LVGL_V8_Test/components/port_bsp/esp32_s3_touch_amoled_1.43c.cpp` (start-up values, QSPI settings, column offset and touch orientation) and `bsp_config.h` in the same folder (pin assignments). The repository's other examples carry their own copies of these files.
The repository's license is the `LICENSE` file at its root.

Changes: the values are written as one table for the bundled `esp_lcd_co5300` driver, which sends the pixel format and display-on commands itself; brightness starts at zero and the frame memory is cleared before the display is switched on. Dragon Detector's board integration, touch handling, button navigation, battery estimate and diagnostics are separate original code.
