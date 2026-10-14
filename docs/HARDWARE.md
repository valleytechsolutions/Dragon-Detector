# Exact board profiles

| Board | Resolution | Adapter | Storage in 0.4 |
|---|---|---|---|
| Spotpear / Guition JC3636W518 | 360×360 ST77916 LCD | Vendor ESP32 Display Panel configuration | microSD logs/PCAP; internal badge/history flash |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | 466×466 CO5300 AMOLED, CST9217 touch, AXP2101 | KODE DIY CO5300 / ESP-IDF QSPI, SensorLib, XPowersLib | internal flash badge/history; live observations in RAM |
| M5Stack StopWatch C152 | 466×466 AMOLED | Pinned M5Unified / M5GFX StopWatch support | internal flash badge/history; live observations in RAM |

[Pin definitions](../firmware/include/board_pins.h) are authoritative for these builds. Waveshare C uses display reset GPIO 1 and touch reset GPIO 2; these are not the non-C board's wiring. It has no SD slot in this profile. M5Unified identifies the StopWatch and rejects an unexpected display.

Manufacturer references:

- [Spotpear exact product](https://spotpear.com/shop/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3.html).
- [Waveshare C-variant hardware/demo source](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C) and [schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf).
- [M5Stack StopWatch documentation](https://docs.m5stack.com/en/core/StopWatch), [product photographs](https://shop.m5stack.com/products/m5stack-stopwatch-dev-kit-esp32-s3), and [manufacturer demo](https://github.com/m5stack/M5StopWatch-UserDemo).

Battery percentage is available through Waveshare AXP2101 and M5Stack power APIs. Spotpear has no configured fuel gauge; no battery estimate is invented. Hardware-in-the-loop testing is distinct from a successful compile; see [validation](VALIDATION.md).

## Speaker output

| Board | Tone adapter | Connections |
|---|---|---|
| Spotpear | Original 16 kHz I2S PCM synthesis | BCLK 18, WS 16, DOUT 17, amplifier enable 48 |
| Waveshare 1.75C | ES8311 codec, original 16 kHz I2S PCM synthesis | MCLK 16, BCLK 9, WS 45, DOUT 8, amplifier enable 46; codec I2C address 0x18 on the display/touch bus |
| StopWatch | M5Unified speaker | Board-specific wiring managed by the pinned library; vibration is independent |

Use the board's suitable speaker connection; a bare board with no speaker connected cannot produce sound. The Waveshare owner confirmed audible output. Spotpear and StopWatch audio are build-verified only. RTTTL melodies are synthesized locally, and no microphone input is enabled. Fresh installs use 100% speaker volume and 100% display brightness with no dim/off timers; both remain configurable.
