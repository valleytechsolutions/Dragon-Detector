# Validation and known limitations

The repository organization update changes documentation and file placement, not firmware behavior. Release BINs were compared byte-for-byte with the existing release files, and checksums are provided at the root.

## StopWatch v0.2.0

The application compiled and was flashed with esptool verification. All 17 firmware self-tests passed. Real Wi-Fi/BLE observations were received. On-device LVGL tests exercised scrolling in channel/settings/about screens and button handlers; custom logo import was tested over USB. Some queue drops were observed, so complete capture is not claimed.

Physical display color confirmation, physical button actuation, audible/vibration feedback, battery runtime, long-duration stability and all candidate-device families remain unverified. The screenshot captures the renderer; it cannot prove the AMOLED transfer. Browser badge import has not been tested end-to-end. See the [detailed StopWatch notes](../hardware/m5stack-stopwatch/docs/VALIDATION.md).

## Spotpear v0.1.1

The existing firmware/source were retained without changing its functionality. Its previous [export validation](../hardware/spotpear/docs/export-validation.json) and [feature limits](../hardware/spotpear/docs/FEATURES.md) are included. This repository cleanup did not reflash or retest the Spotpear device.

## Repository checks

Local project Markdown links/images and firmware checksums are checked before publication. Build caches, device dumps, raw packet captures, personal recordings and saved vendor web pages are excluded. Third-party libraries and original license notices are retained because the build depends on them.
