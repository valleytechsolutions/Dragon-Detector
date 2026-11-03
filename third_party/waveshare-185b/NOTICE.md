# Waveshare ESP32-S3-Touch-LCD-1.85B panel data

The two ST77916 initialization tables in `firmware/include/waveshare_185b_lcd_init.h` are adapted from Waveshare's official board examples, licensed under Apache-2.0. The complete license is included alongside this notice.

Source: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B
Revision: `7232af4c501c64d34215b73e1d8319376dfd9c19`
Source file: `examples/Arduino-V3.2.0/examples/01_lvgl_demo/Display_ST77916.cpp`
The board BSP's explicit license is at `examples/esp-idf/01_lvgl_demo/components/esp32_s3_touch_lcd_1_85B/LICENSE`.

Changes: command payloads use fixed inline arrays; commented alternatives were removed. Both panel versions and their delays are preserved. Dragon Detector's board integration, bounded touch handling, diagnostic hooks and read-only gauge integration are separate original code.

