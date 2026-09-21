# Validation — 2026-09-21

- Builds successfully for ESP32-S3 StopWatch with pinned M5 drivers.
- Flashed application to attached COM4; esptool verified written bytes.
- 17 firmware self-tests passed.
- Actual device receives Wi-Fi and BLE frames. Some queue drops were observed during heavy UI work; counters expose these rather than claiming complete capture.
- On-device LVGL pointer injection: channel, settings and about lists scroll both ways without leaving the page. Button handlers return to radar, open/cycle menu tabs, and exit badge mode. See ui-validation.json. Physical button actuation still needs user confirmation.
- VTS image imported successfully over USB (434,320 bytes). Transfer chunks are bounded to 128 bytes to avoid USB receive overruns.
- Display transfer uses explicitly typed big-endian RGB565, matching LVGL. Physical color confirmation is pending; rendered screenshots cannot verify the AMOLED transport.
- Release merge checks compare every component byte-for-byte at its flash offset. Factory image is built from clean files, never a user's device dump.

Battery runtime, physical tone/vibration output, all candidate device families, browser-based importer end-to-end, and extended endurance have not been validated. IMU and RTC do not provide bearing or UTC timestamps in this firmware.
