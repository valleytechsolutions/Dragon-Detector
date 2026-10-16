![Dragon Detector promotional artwork](media/cover.png)

# Dragon Detector · Detection & Hunt Edition

**A Dragon Radar-inspired detector for five ESP32-S3 board profiles, including a radar-only Lite edition.** Observe device signatures, hunt a selected signal, and keep the evidence in local history. Create a custom image badge in the browser, sized for your exact board.

**[Open the web flasher & detector tools](https://valleytechsolutions.github.io/Dragon-Detector/)** · [Install guide](docs/FLASHING.md) · [Controls](docs/USER-GUIDE.md) · [Detection evidence](docs/DETECTION.md) · [Credits](CREDITS.md)

Version **0.13.0** brings the **29 timed Wi-Fi/BLE tools, eight modes and BLE Lab to all four full boards**. New workflows include saved recon sessions, historical AP archives, bounded recipes, HID report decoding and a GATT bridge with cached reads. **Tools → Recon → Attacks** opens the attack lists directly; **Utilities** replaces Daily. Normal screens retain background detection. All five profiles share the 158-fact SquachWatch audit and AXEOFF priority handling. [New workflows and limits](docs/RECON-013.md) · [Memory reserve](docs/FLASH-BUDGET.md) · [Feature comparison](docs/FEATURE-MATRIX.md).

Update preserves badge, history, settings, GATT profiles and saved captures. Active lab sessions temporarily own the shared radio; Stop/Home returns it to the previous listening state. This is an independent implementation, not complete Marauder or ChimeraBLE parity. [Validation and limits](docs/VALIDATION.md).

| Utilities | Attacks |
|---|---|
| ![Utilities on Waveshare](media/waveshare-utilities-013.png) | ![Attacks menu on Waveshare](media/waveshare-attacks-013.png) |

CRC-verified logical display captures from the connected Waveshare running 0.13.0; these are firmware renders, not panel photographs. [Open Radio tools](https://valleytechsolutions.github.io/Dragon-Detector/#radio-tools).

## Choose the exact board

| Board | Display | Controls | Build |
|---|---|---|---|
| **Spotpear / Guition JC3636W518** | 360×360 round LCD | Touch | `spotpear` |
| **Waveshare ESP32-S3-Touch-AMOLED-1.75C** | 466×466 round AMOLED · **32 MB / BLE Lab** | Touch + BOOT / short PWR | `waveshare` |
| **M5Stack StopWatch C152** | 466×466 round AMOLED | Touch + yellow/blue keys, haptics | `stopwatch` |
| **Seeed XIAO ESP32-S3 + Round Display** | 240×240 round LCD | Touch + BOOT, microSD, RTC; no speaker | `xiao_round` |
| **Waveshare ESP32-S3-LCD-1.28 — Lite** | 240×240 round LCD | **Radar only:** no touch/audio; full brightness, all contacts on one radar | `waveshare_lite` |

The Waveshare **C variant** has different reset wiring from the non-C 1.75. Firmware is not interchangeable just because a board contains an ESP32-S3. [Board details and manufacturer links](docs/HARDWARE.md).

| Spotpear hardware | M5Stack hardware |
|---|---|
| ![Spotpear manufacturer photograph](https://cdn.static.spotpear.com/uploads/picture/product/esp32/EPS32-S3-1.8inch-Touch-LCD/WEB-EPS32-S3-1.8inch-Touch-LCD.jpg) | ![M5Stack manufacturer photograph](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152_stopwatch_mainpictures_01.webp) |

Photographs are manufacturer-hosted identification images, not pictures of this firmware. [Waveshare's official C-variant source and hardware reference](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C) identifies the third board. The cover is promotional art; no manufacturer or Dragon Ball affiliation is implied.

The Seeed port has its own 8 MB flash layout, native 240-pixel fonts/logo, buffered touch, shared SPI SD support and RTC controls. USB detection, RTC and badge checks pass; physical panel/touch confirmation and inserted-card tests remain pending. [XIAO hardware, switches and installation](docs/XIAO-ROUND.md).

| XIAO radar · 0.9.2 | XIAO stopwatch · 0.9.2 |
|---|---|
| ![Actual XIAO radar renderer](media/xiao-radar-v092.png) | ![Actual XIAO stopwatch renderer](media/xiao-stopwatch-v092.png) |

Logical 240×240 captures from the connected XIAO, not panel photographs.

## A radar you can wear

![Dragon Detector 0.9.1 radar captured from the attached M5Stack renderer](media/radar-green-v091.png)

*Green radar theme with animated sweep, labeled signals and owner-supplied BWM1 logo. Inward means stronger filtered RSSI, with no numerical distance scale. Actual 466×466 LVGL render from M5Stack StopWatch C152, not a panel photograph. Targets opens the paged list; the speaker button toggles quiet mode.*

- **Three radar themes:** Green radar (default), Dragon Ball, and Black / gold / red. Each of the 26 device families has its own fixed color for dots, labels and leaders, with a [color legend](docs/RADAR-COLORS.md). Animated sweep, pulsing signals, automatic priority tags and tap-to-reveal tracker tags. All contacts share one radar; tap a selected dot again for evidence and Hunt; choose your theme in Settings or the USB web tools.
- **Audited signature coverage:** all 158 table entries in the pinned SquachWatch reference have recognition and radar tests. Includes Flipper, Wi-Fi Pineapple, Pwnagotchi, nyanBOX, Biscuit, Flock, Axon/TASER, Raven, camera glasses, Find My/DULT, SmartTag, Google Find Hub, Tile, Ring/camera/ALPR vendors, decoded drone Remote ID, iBeacon, pentest devices, disconnect bursts and SSID security conflicts. [Coverage and comparison](docs/DETECTION-PARITY.md).
- **All recognized candidates on radar:** first sightings appear immediately; device types use fixed colors and trackers use smaller dots. Intermittent signals remain for up to two minutes with a segmented marker and explicit last-seen age. A paged Targets list, quick quiet toggle and live radio-health screen make crowded areas easier to navigate.
- **Traffic awareness:** Wi-Fi deauthentication/disassociation bursts and BLE pairing/tag spam candidates, with a radar activity banner, counts, protocol/channel evidence and a dedicated Activity screen. See [how traffic detection works](docs/RADAR-AND-ACTIVITY.md).
- **Dragon Hunt:** dedicated target listening, filtered dBm, signal history, peak, trend, stability, a saved comparison mark, and automatic channel searching for stale Wi-Fi targets. Optional Dragon fox beacons support board-to-board training; transmission starts only when explicitly enabled.
- **Useful alert controls:** per-family and confidence filters, persistent watch/ignore rules, temporary snooze, repeat-alert suppression, quiet mode and grouped advertisement bursts.
- **Local detection history:** 512 address/radio/family records, bounded flash journal, encounters and peak signal. Connect USB to synchronize UTC and download CSV through the [detector tools](https://valleytechsolutions.github.io/Dragon-Detector/#detector).
- **Your requested display defaults:** 100% brightness, always on, no automatic dimming. Settings offers independent dim and screen-off timers, dim level and wake-on-alert.
- **Hear the hunt:** 26 original device-family melodies, a distinct Axon alert and double heartbeat, signal-dependent pitch/cadence, and a lost-signal cue. Edit, preview, silence or reset RTTTL tones in the [USB audio editor](https://valleytechsolutions.github.io/Dragon-Detector/#sound).
- **More room for reception:** 384 contacts in PSRAM, bounded packet processing, indexed packet queues and faster lookups across every board. Passive BLE is the default; optional active scanning requests response-only names. Assigned service IDs are recognized in 16-, 32- and exact Bluetooth Base 128-bit encodings. See [checking a Flipper or Pineapple Pager](docs/DEVICE-CHECKS.md).
- **Shared firmware:** all five profiles use the same detection, tracking, radar renderer and release pipeline. The four full profiles also include history and interactive controls. Lite excludes badge/menu/audio/clock features and persistent history; it keeps scanning with all families enabled. The original three profiles have synthesized RTTTL speaker alerts. Seeed XIAO Round uses visual alerts because it has no speaker. StopWatch adds haptics; Spotpear and Seeed XIAO Round add optional SD CSV/PCAP.
- **Badge Studio:** board-specific 240/360/466px previews, fit/fill, drag, zoom, rotate, mirror, fine positioning, background color, alignment guides and PNG export. USB identifies the exact board before a CRC-checked upload; interrupted transfers preserve the previous badge.

**A detected signature does not prove malicious intent, ownership, recording state or a particular product model.** AirTag/Find My family matches may be another compatible accessory; Biscuit Pro and Ultra are not reliably distinguishable from their shared advertisements. Radar radius reflects filtered signal strength, not measured distance. Angle is decorative, not direction. These radios cannot detect silent devices, Bluetooth Classic, or 5/6 GHz Wi-Fi.

## Stopwatch, clock and orientation

**Tools → Utilities** now includes a real start/pause/lap stopwatch and a clock. M5Stack yellow/blue and Waveshare BOOT/short PWR operate timing controls; the four full-feature boards also have touch buttons. The latest 50 laps remain available while detection keeps running. Settings provides RTC date/time and local UTC offset, saved 0°/90°/180°/270° orientation on the four full-feature boards (Lite has a fixed orientation). [Controls, behavior and hardware limits](docs/STOPWATCH-CLOCK-ORIENTATION.md).

| Stopwatch · 0.9 | Clock · 0.9 |
|---|---|
| ![Stopwatch with elapsed time and touch controls](media/stopwatch-timer-v09.png) | ![Clock with local date and UTC offset](media/stopwatch-clock-v09.png) |

Actual logical renderer captures from the attached M5Stack. Orientation changes only through Settings or the USB tools; fresh installations stay upright. [Clock and orientation web controls](https://valleytechsolutions.github.io/Dragon-Detector/#detector) are available after connecting firmware 0.9.

## Choose your radar

| Dragon Ball | Black / gold / red |
|---|---|
| ![Dragon Ball theme, actual M5Stack LVGL render](media/radar-dragon-v091.png) | ![Black/gold/red theme, actual M5Stack LVGL render](media/radar-gold-v091.png) |

Settings → Radar theme & labels saves the theme, label preference and recent-signal retention. The images are actual 466×466 renderer captures of received contacts, not physical-panel photographs.

## On-device display settings

![Waveshare display settings, captured from the firmware renderer](media/display-v04.png)

A 466×466 LVGL capture from the attached Waveshare. This is the firmware render, not a physical-panel photograph.

## Audio alerts and RTTTL editor

Open **Settings → Audio & hunt tones** on the device, or connect to the [USB audio editor](https://valleytechsolutions.github.io/Dragon-Detector/#sound). Fresh installations use 100% volume; existing preferences are retained on update. Axon, glasses, trackers, ALPR candidates and the other detector families each have their own original melody. Select a contact to Hunt or enable automatic Axon / ALPR / glasses focus.

![Audio settings and RTTTL editor](media/audio-studio-v04.png)

*0.4.0 browser screenshot. USB enables loading, previewing and saving tones. The image editor remains directly below it.*

## Badge Studio

![Badge Studio 0.8 with board selection and owner-supplied BWM1 artwork](media/badge-studio-v08.jpg)

Choose your board → compose your image → connect and upload. Images initially fit inside the circle with a small margin; choose Fill circle for a full-screen crop. Rotate/mirror, keyboard nudging, alignment guides and downloadable previews work before connecting a device. Guides are never uploaded. [Open Badge Studio](https://valleytechsolutions.github.io/Dragon-Detector/#studio). This is a browser capture, not a panel photograph.

The 0.8 M5Stack adapter uses native touch coordinates and direct Tools/Home taps. Outside Stopwatch, yellow returns to radar; blue opens Tools, or Badge from Tools. Upward-swipe navigation has been removed on all boards; lists still scroll normally.

| M5Stack radar · 0.8 | M5Stack saved badge · 0.8 |
|---|---|
| ![M5Stack radar renderer capture](media/stopwatch-radar-v08.png) | ![M5Stack BWM1 badge renderer capture](media/stopwatch-badge-v08.png) |

Actual 466×466 renderer captures from the attached StopWatch. The owner confirmed display, touch and physical-button navigation. The browser-exported badge matched uploaded RGB565 pixels and survived a restart; no private identifiers are shown.

## Install and customize

1. Open the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/), select the exact board, and choose **Fresh install** for your first current installation. This replaces stored files. Back up existing firmware first.
2. Connect USB and follow the installer. Reset when finished if necessary.
3. Open **Detector tools** on the same page to sync UTC, export history, or set display timers. On the device, tap a radar dot → **Hunt signal**, or use Tools → History.
4. Set detector families and the audible-alert threshold in Tools → Detectors. Weak clues remain visible when the alert threshold is raised. A target’s Controls page offers watch, ignore and snooze. Badge Studio is still available separately.

Use **Update** only for an existing 0.3.x, 0.4.x, 0.5.x, 0.6.x, 0.7.x, 0.8.x or 0.9.x installation on the same board. Split update images preserve NVS and the badge filesystem. A merged factory BIN writes at **0x0** and resets stored settings/files. [Full recovery and manual flashing instructions](docs/FLASHING.md).

## Repository map

| Path | Purpose |
|---|---|
| [firmware](firmware) | Current 0.9.x shared application and board adapters |
| [platformio.ini](platformio.ini) | Pinned board profiles and build dependencies |
| [web](web) | GitHub Pages flasher, USB detector tools and Badge Studio |
| [tools](tools) | Build, package and validate releases |
| [tests](tests) | Native detector/tracking tests and image-protocol tests |
| [lib/opendroneid](lib/opendroneid) | Pinned Apache-2.0 Remote ID decoder and notice |
| [lib/esp_lcd_co5300](lib/esp_lcd_co5300) | Pinned Apache-2.0 CO5300 driver with original notice |
| [hardware](hardware) | Previous 0.1/0.2 projects, board partition tables and retained vendor libraries |
| [docs](docs) | Current guides, validation limits and third-party notices |

The older Spotpear multimedia and StopWatch releases remain in `hardware/` for reproducibility. The current builds omit the GPL audio decoder and microphone/music workflows. Earlier binaries are **not** 0.3.x update targets.

## Build and contribute

See [BUILDING.md](docs/BUILDING.md). CI tests the packet parser and tracking code, builds every board, and packages matching firmware, manifests and SHA-256 checksums. A successful `main` build feeds the GitHub Pages deployment from the same source commit.

Original current application, web and tooling code is **[MIT](LICENSE)**, by **Your Pal Kal / Valleytech Custom Solutions**. Dependencies keep their own notices; historical `hardware/` projects retain their existing licenses. Thanks to **Colonel Panic / OUI Spy / Unified Blue**, **JustCallMeKoko / ESP32 Marauder**, Flock Noir, **skizzophrenic / SquachWatch**, OpenDroneID, CodeHedge, and the hardware/library authors. Read [CREDITS.md](CREDITS.md) for the exact scope and provenance.
