![Dragon Detector promotional artwork](media/cover.png)

# Dragon Detector · Daily Badge Edition

**A Dragon Radar-inspired daily badge for three ESP32-S3 boards.** Observe nearby Wi-Fi and Bluetooth LE signatures, follow signal changes, or wear your own image while scanning continues.

**[Open the web flasher & Badge Studio](https://valleytechsolutions.github.io/DragonBall-Z-Radar/)** · [Install guide](docs/FLASHING.md) · [Controls](docs/USER-GUIDE.md) · [Detection evidence](docs/DETECTION.md) · [Credits](CREDITS.md)

Version **0.3.0** is an experimental development release. See [what has actually been tested](docs/VALIDATION.md) before installing. The flasher requires desktop Chrome/Edge, HTTPS, a USB data cable, and the exact board selection; it stays disabled if matching release files are unavailable.

## Choose the exact board

| Board | Display | Controls | Build |
|---|---|---|---|
| **Spotpear / Guition JC3636W518** | 360×360 round LCD | Touch | `spotpear` |
| **Waveshare ESP32-S3-Touch-AMOLED-1.75C** | 466×466 round AMOLED | Touch + BOOT key | `waveshare` |
| **M5Stack StopWatch C152** | 466×466 round AMOLED | Touch + yellow/blue keys, haptics | `stopwatch` |

The Waveshare **C variant** has different reset wiring from the non-C 1.75. Firmware is not interchangeable just because a board contains an ESP32-S3. [Board details and manufacturer links](docs/HARDWARE.md).

| Spotpear hardware | M5Stack hardware |
|---|---|
| ![Spotpear manufacturer photograph](https://cdn.static.spotpear.com/uploads/picture/product/esp32/EPS32-S3-1.8inch-Touch-LCD/WEB-EPS32-S3-1.8inch-Touch-LCD.jpg) | ![M5Stack manufacturer photograph](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152_stopwatch_mainpictures_01.webp) |

Photographs are manufacturer-hosted identification images, not pictures of this firmware. [Waveshare's official C-variant source and hardware reference](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C) identifies the third board. The cover is promotional art; no manufacturer or Dragon Ball affiliation is implied.

## A radar you can wear

- **Candidate radar by default:** repeated, fresh device evidence gets priority. Broad vendor-OUI hints stay in contact lists; Settings can show all contacts.
- **Flock/ALPR-related, Axon, trackers and tools:** Axon company/service IDs, Flock accessory/battery/strict probe signatures, Find My, Samsung FD5A, Flipper, Nyanbox, Biscuit and other named candidates. Evidence strength stays visible.
- **Steadier tracking:** five-sample median filtering, time-based smoothing, signal trend and stability, stale-target indication, and dedicated BLE or Wi-Fi target listening. Random BLE addresses are not merged into imaginary identities.
- **Your own badge:** local browser image import, round crop preview, drag/zoom/center/fit, automatic 360/466 sizing, checksum-verified USB upload, retained previous image on failed transfer, and persistence across restart.
- **Daily defaults:** 50% brightness, dim to 8% after 45 seconds, slow badge pixel shift, passive scans, no persistent hotspot, and event logging off by default. Spotpear can record logs/PCAP to microSD when explicitly enabled.
- **One current source tree:** all three profiles use the same detector, tracking, UI, image protocol and release tooling.

**A detected signature does not prove malicious intent, ownership, recording state or a particular product model.** AirTag/Find My family matches may be another compatible accessory; Biscuit Pro and Ultra are not reliably distinguishable from their shared advertisements. Radar radius reflects filtered signal strength, not measured distance. Angle is decorative, not direction. These radios cannot detect silent devices, Bluetooth Classic, or 5/6 GHz Wi-Fi.

## Badge Studio preview

![Browser Badge Studio with owner-supplied VTS artwork](media/badge-studio-v03.jpg)

Browser capture of the 0.3.0 editor. This is a crop preview, not a photograph of a flashed device.

## Install and customize

1. Open the [web flasher](https://valleytechsolutions.github.io/DragonBall-Z-Radar/), select the exact board, and choose **Fresh install** for your first 0.3.x installation. This replaces badge storage. Back up existing firmware first.
2. Connect USB and follow the installer. Reset when finished if necessary.
3. Open **Badge Studio** on the same page, connect your running badge, choose an image, adjust the crop, and upload. Images stay in the browser and go directly over USB to the board.
4. Hold the displayed image to return to tools. Settings → Boot to radar restores radar startup. Scanning continues in badge mode; uploading briefly pauses it.

Use **Update** only for an existing 0.3.x installation on the same board. Split update images preserve NVS and the badge filesystem. A merged factory BIN writes at **0x0** and resets stored settings/files. [Full recovery and manual flashing instructions](docs/FLASHING.md).

## Repository map

| Path | Purpose |
|---|---|
| [firmware](firmware) | Current 0.3.x shared application and board adapters |
| [platformio.ini](platformio.ini) | Pinned board profiles and build dependencies |
| [web](web) | GitHub Pages flasher and browser Badge Studio |
| [tools](tools) | Build, package and validate releases |
| [tests](tests) | Native detector/tracking tests and image-protocol tests |
| [lib/esp_lcd_co5300](lib/esp_lcd_co5300) | Pinned Apache-2.0 CO5300 driver with original notice |
| [hardware](hardware) | Previous 0.1/0.2 projects, board partition tables and retained vendor libraries |
| [docs](docs) | Current guides, validation limits and third-party notices |

The older Spotpear multimedia and StopWatch releases remain in `hardware/` for reproducibility. The 0.3 badge builds omit the GPL audio decoder and microphone/music workflows. Earlier binaries are **not** 0.3.x update targets.

## Build and contribute

See [BUILDING.md](docs/BUILDING.md). CI tests the packet parser and tracking code, builds every board, and packages matching firmware, manifests and SHA-256 checksums. A successful `main` build feeds the GitHub Pages deployment from the same source commit.

Original current application, web and tooling code is **[MIT](LICENSE)**, by **Your Pal Kal / Valleytech Custom Solutions**. Dependencies keep their own notices; historical `hardware/` projects retain their existing licenses. Thanks to **Colonel Panic / OUI Spy / Unified Blue**, **JustCallMeKoko / ESP32 Marauder**, Flock Noir, CodeHedge, and the hardware/library authors. Read [CREDITS.md](CREDITS.md) for the exact scope and provenance.
