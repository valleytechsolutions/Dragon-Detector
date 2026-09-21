[Repository home](../../README.md) | [Detailed flashing guide](../../docs/FLASHING.md) | [Credits](../../CREDITS.md)

# Dragon Detector — M5Stack StopWatch

Dragon-style passive Wi-Fi/BLE radar for the **M5Stack StopWatch C152**. This is a separate hardware port; the Spotpear Dragon Detector project is unchanged.

![Radar screen](docs/radar.png)
![Custom badge](docs/badge.png)

## Controls

- Yellow KEY A (GPIO2): return to radar.
- Blue KEY B (GPIO1): open tools; press again to cycle RECON / TOOLS / SYSTEM.
- Touch a mode to open it. Lists scroll normally; scrolling does not dismiss them.
- Swipe upward from the bottom of the radar to open tools.
- Badge mode: tap to advance images; hold to toggle a ten-second slideshow; yellow exits.

## StopWatch features

Wi-Fi/BLE contacts, RSSI radar, candidate device signatures with evidence, deauthentication/disassociation burst observations, BLE advertising pattern observations, channel monitoring, opt-in event logs, bounded PCAP capture, flash file viewer, custom badges, optional alert tones and vibration. The microphone, music playback, audio recording and media decoder are excluded from this port.

Radar angles are decorative and distance is signal strength, not measured meters. Candidate signatures do not prove identity or malicious intent. Scanning alternates 2.4 GHz Wi-Fi channels 1–11 and BLE, so coverage has gaps. Bluetooth Classic and 5 GHz are unsupported. Times are device uptime. This is not a full Marauder feature port.

## Install

Use the **factory-0x0.bin** for a fresh installation on StopWatch only. Flash at **0x0**. It contains bootloader, partitions, app and a clean internal filesystem with the VTS badge. It replaces existing files and settings. Never flash this image on the Spotpear board.

The **update-0x0.bin** keeps the filesystem only when upgrading this exact partition layout; it resets NVS preferences. For developers, flashing only firmware.bin at 0x10000 keeps both NVS and files.

```sh
python -m pip install esptool
python -m esptool --chip esp32s3 --port COM4 --baud 921600 --after watchdog-reset write-flash 0x0 release/Dragon-Detector-StopWatch-v0.2.0-factory-0x0.bin
```

Replace COM4 with your port. Watchdog reset reliably starts this unit after flashing. Check SHA256SUMS.txt against your downloaded binary.

## Import your badge

Use tools/Badge-Importer.html in Chrome or Edge, served on localhost if Web Serial is unavailable when opened directly. Choose an image, connect the StopWatch and import with a unique name. Or:

```sh
python -m pip install pillow pyserial
python tools/badge_import.py your-image.png --port COM4 --name MyBadge
```

Images convert to 466×466 RGB565 with a DDB1 header. Existing names are protected. Files live in internal flash; this board has no built-in SD slot. Each image uses approximately 424 KiB. The tools pause scanning during USB transfers, then restore it. The factory image contains no user captures or recordings.

## Build

Install PlatformIO Core, then from this folder:

```sh
pio run
pio run -t buildfs
python tools/build_release.py
```

M5Unified and M5GFX are pinned to exact commits in platformio.ini. LVGL and NimBLE are vendored with their licenses. Python helper dependencies: pillow, pyserial and esptool. The build targets 16 MB flash and 8 MB octal PSRAM, with two 4 MB application slots and approximately 7.94 MiB internal file storage. Captures stop at 512 KiB; logs rotate at 256 KiB. Logging is off by default.

## Hardware and attribution

- [M5Stack StopWatch official specifications](https://docs.m5stack.com/en/core/StopWatch)
- [Actual device and manufacturer photographs](https://shop.m5stack.com/products/m5stack-stopwatch-dev-kit-esp32-s3)
- [M5Stack StopWatch demo](https://github.com/m5stack/M5StopWatch-UserDemo)
- **[ESP32 Marauder by JustCallMeKoko](https://github.com/justcallmekoko/ESP32Marauder)**: upstream inspiration and adapted passive detection work. Its MIT attribution is retained in MARAUDER-LICENSE.txt. No affiliation or upstream endorsement is implied.
- [M5Unified](https://github.com/m5stack/M5Unified) and [M5GFX](https://github.com/m5stack/M5GFX): hardware, touch, power and speaker support.

Project source is GPL-3.0; included third-party components retain their own licenses. VTS logo supplied by the project owner; third-party branding rights are not granted by the software license. See docs/VALIDATION.md for what was tested and remaining physical checks.

## Additional community acknowledgment

Thanks to [CodeHedge](https://github.com/CodeHedge) for related Marauder/CYD and Biscuit community work. See the [central credits](../../CREDITS.md) for attribution scope.
