# Exact board profiles

| Board | Resolution | Adapter | Storage |
|---|---|---|---|
| Spotpear / Guition JC3636W518 | 360×360 ST77916 LCD | Vendor ESP32 Display Panel configuration | microSD logs/PCAP; internal badge/history flash |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | 466×466 CO5300 AMOLED, CST9217 touch, AXP2101 | KODE DIY CO5300 / ESP-IDF QSPI, SensorLib, XPowersLib | 32 MB flash: preserved badge/history + dedicated 16 MB BLE Lab storage; live observations in PSRAM |
| M5Stack StopWatch C152 | 466×466 AMOLED | Pinned M5Unified / M5GFX StopWatch support | 32 MB flash: preserved badge/history + dedicated 16 MB BLE Lab storage; live observations in PSRAM |

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

Use the board's suitable speaker connection; a bare board with no speaker connected cannot produce sound. The Waveshare owner confirmed audible output. StopWatch speaker initialization and a 100% preview were exercised during the 0.8 hardware checks. Spotpear audio remains build-verified only. RTTTL melodies are synthesized locally, and no microphone input is enabled. Fresh installs use 100% speaker volume and 100% display brightness with no dim/off timers; both remain configurable.

## StopWatch input diagnostics

On current StopWatch firmware, USB `touch status` reports controller samples and LVGL press counts; `ui status` reports page/action counters. These are read-only diagnostic summaries and do not inject taps. Native 466px touch positions align with the centered display canvas. Navigation uses Tools/Home taps and yellow/blue buttons; swipe-to-open is disabled.

## Clock, motion and timing controls · 0.9

[M5Stack StopWatch official documentation](https://docs.m5stack.com/en/core/StopWatch) identifies the RX8130CE RTC, BMI270 IMU and GPIO 2/1 yellow/blue keys. [Waveshare 1.75C documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C) describes the onboard RTC/IMU and programmable BOOT/PWR buttons; the adapter probes PCF85063 (0x51) on its existing SDA15/SCL14 bus and reads short PWR events from AXP2101. Version 0.9.1 uses manual display orientation on all boards and does not poll motion sensors. The Spotpear JC3636W518 profile uses touch timing controls and a software clock. [User guide](STOPWATCH-CLOCK-ORIENTATION.md).

## Seeed XIAO ESP32-S3 + Round Display · 0.9.2

A fourth profile, `xiao_round`, supports the 240×240 GC9A01/CHSC6X expansion with BM8563 RTC and SPI microSD. It uses the XIAO’s 8 MB flash and 8 MB PSRAM, visual alerts, buffered touch and a separate 8 MB partition layout. It has no speaker, haptics or automatic rotation. [Exact wiring, switches, controls and SD behavior](XIAO-ROUND.md).

## Waveshare 1.28 Lite

The `waveshare_lite` profile supports the **non-touch ESP32-S3-LCD-1.28**, with 16 MB flash, 2 MB quad-SPI PSRAM and a 240×240 GC9A01 panel. This is a dedicated always-on passive radar with automatic pages, no audio, touch, menus, badge, clock or persistent history. [Wiring, behavior and recovery](WAVESHARE-LITE.md).
