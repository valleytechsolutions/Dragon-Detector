# Exact board profiles

Current firmware: **0.13.10**. Another display on the same ESP32-S3 needs different firmware.

| Feature | Waveshare AMOLED 1.75C | M5Stack StopWatch C152 | Spotpear JC3636W518 | XIAO ESP32-S3 + Round Display | Waveshare LCD 1.28 Lite |
|---|---|---|---|---|---|
| Display | 466×466 AMOLED / touch | 466×466 AMOLED / touch | 360×360 LCD / touch | 240×240 LCD / touch | 240×240 LCD / no touch |
| Flash / PSRAM | 32 / 8 MB | 16 / 8 MB | 16 / 8 MB | 8 / 8 MB | 16 / 2 MB |
| Physical timing controls | BOOT + short PWR | Yellow Start/Pause + blue Lap | On-screen | BOOT Start/Pause + on-screen Lap | BOOT tap / double-tap / hold |
| Radar / same detection families | Yes | Yes | Yes | Yes | Yes |
| 29 themes / badge / touch menus | Yes | Yes | Yes | Yes | 29 themes; no badge/touch |
| RSSI Hunt / watch lists / history | Yes | Yes | Yes | Yes | Radar + signal details; no saved history |
| Notebook / eight modes / 29 radio tools | Yes | Yes | Yes | Yes | No |
| BLE Lab / GATT / recon sessions / recipes | Yes | Yes | Yes | Yes | No |
| RTTTL alerts / audible Hunt | Speaker connection | Built-in speaker | Speaker connection | Visual only | Visual only |
| Haptic alerts | No configured motor | Built-in motor | No | No | No |
| Stopwatch / clock settings | Yes; USB / saved-Wi-Fi system clock | Yes; RTC when detected | Yes; software clock | Yes; RTC when detected | Compact clock/timer; USB time setup |
| Fixed orientation setting | Yes | Yes | Yes | Yes | Fixed radar orientation |
| SD card support | **No slot / no SD controls** | **No slot / no SD controls** | Optional SD CSV/PCAP + browser | Optional SD CSV/PCAP + browser | No |
| Internal user storage | 3.94 MiB badge/history + **16 MiB recon** | 7.94 MiB shared | 3.94 MiB shared | 1.94 MiB shared | Theme + UTC offset settings; no file features |
| Capture buffer in PSRAM | 2 MiB | 1 MiB | 1 MiB | 512 KiB | No recorder |
| Capture files / profiles download | USB web tools | USB web tools | USB web tools; separate SD logs | USB web tools; separate SD logs | None |

All four full boards use the same classifiers and lab workflows; Waveshare's advantage is dedicated recon storage, not different radio bands or extra processing power. Builds and new file writes enforce a 21% reserve; existing files are never deleted to meet it. [Exact budgets](FLASH-BUDGET.md) · [Feature limits](FEATURE-MATRIX.md) · [GUI tour](GUI.md).

## Storage follows the hardware

**Waveshare AMOLED and M5Stack:** Utilities → Saved files opens the internal capture library. Settings provides Capture studio and Saved captures; SD controls and SD command handlers are excluded from the build. Capture Studio saves Wi-Fi PCAP and BLE CSV in flash. GATT profiles and event logs are in BLE Lab, detection history is separate, and downloads are in the browser's Radio tools / BLE Lab / Detector tools. No card is required.

**Spotpear and XIAO:** Utilities → SD files browses an inserted FAT32 card. Settings retains opt-in SD event logging and SD PCAP. These are separate from internal Capture Studio, badge/history and BLE Lab storage; an absent card does not disable those internal features. Inserted-card behavior is not newly validated by this release.

**Lite:** radar, compact clock, stopwatch/locksport timer, signal details and diagnostics, operated with BOOT. It has no storage menu, badge, interactive lab or persistent detection history.

Shared storage sizes are partition allocations, not free-space promises. They include badge/history and lab data; the reserve policy prevents new writes from consuming the final 21%. Updates never reformat these volumes automatically.

[Pin definitions](../firmware/include/board_pins.h) are authoritative for these builds. Waveshare C uses display reset GPIO 1 and touch reset GPIO 2; these are not the non-C board's wiring. It has no SD slot in this profile. M5Unified identifies the StopWatch and rejects an unexpected display.

Manufacturer references:

- [Spotpear exact product](https://spotpear.com/shop/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3.html).
- [Seeed Round Display documentation](https://wiki.seeedstudio.com/get_start_round_display/) and [XIAO ESP32-S3 documentation](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/).
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

[M5Stack StopWatch official documentation](https://docs.m5stack.com/en/core/StopWatch) identifies the RX8130CE RTC, BMI270 IMU and GPIO 2/1 yellow/blue keys. The Waveshare **1.75C** must not be confused with the non-C 1.75: the [C schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf) contains no PCF85063 calendar RTC. Its AXP2101 RTCLDO is a power rail, not a calendar. On the tested 32 MB unit, the codecs, PMU, touch and IMU acknowledge on SDA15/SCL14 at both 100/400 kHz; address 0x51 does not. The adapter retains optional PCF85063 probing for revisions that provide one, but does not promise an RTC on this profile. Use USB synchronization or opt-in saved-Wi-Fi recovery after full shutdown. Short PWR events come from AXP2101. Version 0.9.1 uses manual display orientation on all boards and does not poll motion sensors. The Spotpear JC3636W518 profile uses touch timing controls and a software clock. [User guide](STOPWATCH-CLOCK-ORIENTATION.md).

## Seeed XIAO ESP32-S3 + Round Display · 0.9.2

A fourth profile, `xiao_round`, supports the 240×240 GC9A01/CHSC6X expansion with BM8563 RTC and SPI microSD. It uses the XIAO’s 8 MB flash and 8 MB PSRAM, visual alerts, buffered touch and a separate 8 MB partition layout. It has no speaker, haptics or automatic rotation. [Exact wiring, switches, controls and SD behavior](XIAO-ROUND.md).

## Waveshare 1.28 Lite

The `waveshare_lite` profile supports the **non-touch ESP32-S3-LCD-1.28**, with 16 MB flash, 2 MB quad-SPI PSRAM and a 240×240 GC9A01 panel. This is a dedicated always-on passive radar with every contact on one screen, no audio, touch, menus, badge, clock or persistent history. [Wiring, behavior and recovery](WAVESHARE-LITE.md).

All four full profiles compile the [mode launcher and radio toolkit](RADIO-TOOLKIT.md), BLE Lab, notebook and sessions in 0.13.x. Lite excludes interactive labs. Waveshare's 32 MB partition layout remains unchanged from 0.9.8. No current profile implements automatic orientation, MP3 playback, microphone recording, 5/6 GHz or Bluetooth Classic.
