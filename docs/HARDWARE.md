# Exact board profiles

| Board | Resolution | Adapter | Storage in 0.3 |
|---|---|---|---|
| Spotpear / Guition JC3636W518 | 360×360 ST77916 LCD | Vendor ESP32 Display Panel configuration | microSD logs/PCAP; internal flash badge |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | 466×466 CO5300 AMOLED, CST9217 touch, AXP2101 | KODE DIY CO5300 / ESP-IDF QSPI, SensorLib, XPowersLib | internal flash badge; observations in RAM |
| M5Stack StopWatch C152 | 466×466 AMOLED | Pinned M5Unified / M5GFX StopWatch support | internal flash badge; observations in RAM |

[Pin definitions](../firmware/include/board_pins.h) are authoritative for these builds. Waveshare C uses display reset GPIO 1 and touch reset GPIO 2; these are not the non-C board's wiring. It has no SD slot in this profile. M5Unified identifies the StopWatch and rejects an unexpected display.

Manufacturer references:

- [Spotpear exact product](https://spotpear.com/shop/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3.html).
- [Waveshare C-variant hardware/demo source](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C) and [schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf).
- [M5Stack StopWatch documentation](https://docs.m5stack.com/en/core/StopWatch), [product photographs](https://shop.m5stack.com/products/m5stack-stopwatch-dev-kit-esp32-s3), and [manufacturer demo](https://github.com/m5stack/M5StopWatch-UserDemo).

Battery percentage is available through Waveshare AXP2101 and M5Stack power APIs. Spotpear has no configured fuel gauge; no battery estimate is invented. Hardware-in-the-loop testing is distinct from a successful compile; see [validation](VALIDATION.md).
