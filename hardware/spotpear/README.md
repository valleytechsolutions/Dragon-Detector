[Repository home](../../README.md) | [Detailed flashing guide](../../docs/FLASHING.md) | [Credits](../../CREDITS.md)

![Dragon Detector cover](media/brand/dragon-detector-cover.png)

# Dragon Detector

Dragon Detector is development-stage ESP32-S3 firmware for the **Spotpear / Guition JC3636W518 1.8-inch 360×360 round touch display**. It presents passive Wi-Fi and BLE observations as a Dragon Radar-inspired interface, logs evidence to microSD, records microphone audio on explicit command, and plays local WAV/MP3 files.

This project adapts selected passive detection ideas from [ESP32 Marauder by JustCallMeKoko](https://github.com/justcallmekoko/ESP32Marauder), pinned during development at commit [`8ae4622`](https://github.com/justcallmekoko/ESP32Marauder/tree/8ae4622abcc9c9c5729d4e97491907581d7f0c34). Marauder is copyright Just Call Me Koko and contributors and is distributed under the MIT License; the preserved license is in [`docs/MARAUDER-LICENSE.txt`](docs/MARAUDER-LICENSE.txt). Dragon Detector is a separate GPL-3.0 project and is not an official Marauder build.

## Actual supported device

[Spotpear product page](https://spotpear.com/shop/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3.html) · [Spotpear hardware wiki and vendor demo](https://spotpear.com/wiki/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3.html)

![Actual Spotpear JC3636W518 board](https://cdn.static.spotpear.com/uploads/picture/product/esp32/EPS32-S3-1.8inch-Touch-LCD/WEB-EPS32-S3-1.8inch-Touch-LCD.jpg)

![Actual Spotpear kit](https://cdn.static.spotpear.com/uploads/picture/product/esp32/EPS32-S3-1.8inch-Touch-LCD/WEB-EPS32-S3-1.8inch-Touch-LCD-Kit.jpg)

The two product photographs above are hosted by and credited to Spotpear. They are included as links for hardware identification and are not covered by this repository's GPL license.

## Current interface

| Radar evidence callout | Tools menu |
|---|---|
| ![Radar with a labeled evidence line](media/screenshots/radar-callouts.png) | ![Touchscreen tools menu](media/screenshots/menu.png) |

- Tap a radar dot or contact row to open its address, RSSI, channel, signature confidence, and evidence.
- On the radar only, swipe upward from the bottom edge to open tools. Scroll gestures in lists stay in the list.
- Select a Wi-Fi contact to lock monitoring to its channel or start/stop PCAP capture.
- Passive checks cover deauthentication/disassociation bursts, BLE pairing-spam patterns, EAPOL traffic, and documented heuristic candidates for Flock, Axon, Meta glasses, Pineapple, Pwnagotchi, Nyanbox, HaleHound, Find My and Flipper.
- The radar radius uses RSSI. Dot angle is a stable visual arrangement, not a measured bearing. A signature is evidence to inspect, not proof of identity, recording, ownership, or malicious intent.

This release does not transmit deauthentication frames, BLE spam, credential attacks, evil portals, or other disruptive traffic. It does not claim full upstream Marauder feature parity. See [`docs/FEATURES.md`](docs/FEATURES.md) for the exact implemented and missing features.

## Flash the complete image at `0x0`

The release image is for the JC3636W518 with **16 MB flash and 8 MB octal PSRAM**. Back up an existing device before replacing its firmware.

```bash
python -m pip install "esptool==5.4.0"
python -m esptool --chip esp32s3 --port YOUR_PORT write-flash 0x0 firmware/Dragon-Detector-0.1.1-ESP32S3-JC3636W518-0x0.bin
```

Use a port such as `COM7` on Windows or `/dev/ttyACM0` on Linux. If automatic reset fails, hold BOOT while connecting or resetting, release it, and retry.

SHA-256:

```text
BDAB1F52D66ACF9A60D111029BA3E32C7FFA18AB5F1D6BD61C57851BC712F189
```

## Build

The repository vendors the exact display, LVGL, NimBLE, and audio library sources needed by this board. With Python 3.11 or newer:

```bash
python -m pip install "platformio==6.2.0"
pio run
```

Upload with `pio run --target upload --upload-port YOUR_PORT`. Native parser tests live in `tests/detection_test.cpp`.

## Licenses and media

Dragon Detector source is GPL-3.0. Vendored components retain their own licenses; see [`THIRD_PARTY.md`](THIRD_PARTY.md). The VTS logo remains the project owner's supplied artwork. The generated cover is promotional artwork; the screenshots are direct captures of the firmware renderer. Manufacturer photographs remain Spotpear's property.


## Additional community acknowledgment

Thanks to [CodeHedge](https://github.com/CodeHedge) for related Marauder/CYD and Biscuit community work. See the [central credits](../../CREDITS.md) for attribution scope.
