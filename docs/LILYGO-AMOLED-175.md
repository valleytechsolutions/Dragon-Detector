# LILYGO T-Display-S3 AMOLED 1.75

**Beta profile: `lilygo_175`, starting with 0.13.25.** Select the round **1.75-inch H0175Y003AM** model. It has a 466 × 466 CO5300 panel, CST9217 touch, 16 MB flash, 8 MB PSRAM, PCF8563 RTC and SPI microSD. The rectangular 1.64-inch and the 1.43-inch models need different profiles. The LILYGO shop groups these products on one page; the URL alone does not identify the selected size.

The owner supplied `dragon-detector-0.13.20-lilygo-1.75-combined.bin` and confirmed that its screen, touch and other features worked. Its embedded identity, display/touch configuration and partition table were inspected. This profile retains that display configuration and identity while using the current shared firmware. The **new build's RTC retention and SD behavior still require physical testing**; the earlier report is not a hardware test of this release.

## Install or update

Choose **LILYGO AMOLED 1.75** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install). Its setup guide appears beside the install button. Use **Update** from this profile or the supplied working 0.13.20 image: their NVS, OTA and filesystem offsets match. Use **Fresh install** for other firmware, after saving any files you need. Do not use Waveshare firmware or this profile on another LILYGO panel size.

The flasher verifies the PCF8563 directly after setting local time, then checks that it ticks. A missing, invalid or stopped RTC fails clock setup rather than silently falling back to a software clock. USB, manual clock settings and saved-Wi-Fi time recovery all use checked writes.

For offline installation, use this board's generated `factory-0x0.bin` at address `0x0` for a fresh installation. It includes the blank filesystem and replaces existing user files. The update manifest preserves NVS and the filesystem.

## RTC backup and retention

PCF8563 uses I²C address `0x51`, with its calendar beginning at register `0x02`. Firmware checks the voltage-loss flag, stopped/test modes, calendar ranges and readback after saving. Startup preserves the calendar and retries transient bus failures. It never substitutes an old timestamp stored in flash for the actual time.

LILYGO's schematic shows the clock's supply and a separate backup battery node. Retention depends on that backup supply being fitted and healthy on the actual board. Firmware cannot keep an unpowered RTC running. Follow the board's own battery specifications; this guide does not assume the XIAO's CR927 battery applies here.

Test once after updating: synchronize, disconnect USB, power down for at least three minutes, then power up and check the clock **before** connecting an automatic time-sync tool. Direct `clock hardware` and `clock diagnostics` commands do not synchronize it. Expected diagnostics are `chip=PCF8563 io=1 control=00 lost=0`, with a valid, advancing hardware epoch. See [clock setup](CLOCK-SETUP.md).

## SD card and controls

No SD download or boot asset pack is required. Insert a FAT32 microSD before startup. Firmware creates its own folders. The SPI card bus uses CS 38, MOSI 39, MISO 40 and clock 41, independently of the QSPI display bus. Event/PCAP logging and the file browser use the shared card support; internal storage remains available without a card. Stop recording and wait for files to finish saving before removing it. The Waveshare-G GPS/wardrive profile is not applicable to this board.

Tap radar dots for a persistent summary; double-tap or open the summary for details. BOOT opens the tools menu and serves as the main timing key on the stopwatch. Themes, badges, clock, radio tools and BLE Lab use the full shared interface. There is no configured onboard speaker, haptic motor or GPS. Alerts are visual. The status area shows brightness rather than an unsupported battery-charge percentage.

The SY6970 charging circuitry remains at its hardware configuration. LILYGO documents unstable output in some USB-only/no-battery conditions; use its specified battery/power arrangement if affected. The firmware does not apply Waveshare AXP2101 commands or change charging voltage/current limits.

## Configuration reference

| Signal | GPIO / setting |
|---|---|
| Display clock / CS | 12 / 10 |
| Display D0 / D1 / D2 / D3 | 11 / 13 / 14 / 15 |
| Display reset / enable | 17 / 16 |
| Column offset / QSPI clock | 6 / 40 MHz |
| Shared I²C SDA / SCL | 7 / 6 |
| CST9217 address / interrupt | `0x5A` / 9; no dedicated reset |
| Touch mapping | 466 × 466, mirror X and Y as in the working image |
| RTC | PCF8563 at `0x51` |
| App slots | `0x10000`, `0x410000`, each 4 MiB |
| Filesystem | `0x810000`, size `0x7F0000` |

References: [LILYGO source and hardware documentation](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED-1.43-1.75), [1.75 panel configuration](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED-1.43-1.75/blob/87aabe888eea6a7c0f95ce1991ca75fa98c2a172/libraries/Mylibrary/pin_config.h), [schematic](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED-1.43-1.75/blob/87aabe888eea6a7c0f95ce1991ca75fa98c2a172/project/T-Display-S3-AMOLED-1.43-1.75_V1.0.pdf), [NXP PCF8563 datasheet](https://www.nxp.com/docs/en/data-sheet/PCF8563.pdf).

Working reference SHA-256: `c3e8a38f73c515708a305b46cc9d8b15e2a0123b24af9f31256df40da6aaef8a`. The submitted binary is retained locally as a reference; release packages are built from this repository's source.
