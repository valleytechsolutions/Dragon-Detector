<p align="center"><img src="media/branding/dragon-detector.png" width="220" alt="Dragon Detector"></p>

# Dragon Detector

Wi-Fi and Bluetooth detection, signal tracking, and logo watch faces for five ESP32-S3 displays.

[**Install firmware**](https://valleytechsolutions.github.io/Dragon-Detector/#install) · [Latest release](https://github.com/valleytechsolutions/Dragon-Detector/releases/latest) · [User guides](docs/GUI.md) · [Clock setup](https://github.com/valleytechsolutions/Dragon-Detector/wiki/Clock-Setup)

## Clock and timer power saving

On every board, the clock and stopwatch/Locksport Timer pause background Wi-Fi and Bluetooth scanning. Lap history and time/date settings also stay paused. Return to the radar or other tools to resume scanning; a running timer keeps counting in the background. Previously configured Wi-Fi clock recovery may briefly connect for time synchronization. See [power-saving behavior](docs/CLOCK-POWER.md).

## Install

1. Open the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/#install) in desktop Chrome or Edge and connect your board with a USB data cable.
2. Select the exact board. Use **Update matching Dragon Detector** for a compatible installation, or **Fresh install** for a new board. Fresh install replaces saved files and settings.
3. Choose **Connect & install** and keep USB connected until installation and automatic date/time verification finish.

[Installation and recovery](docs/FLASHING.md) · [M5Burner package](docs/M5BURNER.md) · [Release checksums](https://github.com/valleytechsolutions/Dragon-Detector/releases/latest)

## Supported hardware

| Board | Display | Controls and peripherals |
|---|---|---|
| **Waveshare ESP32-S3-Touch-AMOLED-1.75C** | 466 × 466 AMOLED | Touch, BOOT/PWR, 32 MB flash |
| **M5Stack StopWatch C152** | 466 × 466 AMOLED | Touch, yellow/blue buttons, RTC, haptics, 16 MB flash |
| **Spotpear / Guition JC3636W518** | 360 × 360 LCD | Touch, optional microSD, 16 MB flash |
| **Seeed XIAO ESP32-S3 + Round Display** | 240 × 240 LCD | Touch, RTC, microSD, 8 MB flash |
| **Waveshare ESP32-S3-LCD-1.28 Lite** | 240 × 240 LCD | BOOT button, 16 MB flash; no touch |

The four full boards share the radar, 33 themes, 31 logo watch faces, timers, notebook, badge editor, radio tools, and BLE Lab. StopWatch and Waveshare AMOLED support 2 MiB captures and 2 MiB recon sessions. Their total file storage differs.

Lite provides five button-operated pages: radar, clock, timer, signals, and diagnostics. It shares detection and all 33 themes, with compact clocks. It does not include Badge Studio, audio, or active labs.

Firmware is board-specific. The Waveshare **1.75C** and non-C models have different wiring. [Hardware guide](docs/HARDWARE.md) · [Feature comparison](docs/FEATURE-MATRIX.md) · [Tested hardware and limitations](docs/VALIDATION.md)

## Radar and watches

| Dragon Ball | Covert Instruments × McNally | Logo watch |
|---|---|---|
| ![Dragon Ball radar](web/assets/radar-dragon-01313.png) | ![McNally and CI radar](web/assets/radar-covert-01314.png) | ![Dragon Ball watch](web/assets/watch-dragon-01313.png) |

Choose a theme in **Settings → Theme & radar labels**. It applies to the radar, matching clock, menus, and dialogs. The Dragon Ball/BWM animation appears at power-on on every board. Swipe the clock to choose another watch face.

The collection includes maker, community, character, and locksport designs. [Browse the theme gallery](docs/THEMES.md) · [Watch controls](docs/STOPWATCH-CLOCK-ORIENTATION.md) · [Artwork credits](CREDITS.md)

## Capsule Corp

A silver Dragon Radar bezel, emerald grid and yellow signal markers, paired with a white-and-blue Capsule Corp chronometer. Included on all five boards, with matching menus on the full profiles. [Theme guide](docs/CAPSULE-CORP.md).

| Radar | Watch |
|---|---|
| ![Capsule Corp radar](web/assets/radar-capsule-01315.png) | ![Capsule Corp watch](web/assets/watch-capsule-01315.png) |

## Sparrows

Black, red and steel with the supplied keyhole emblem, a hook sweep and pick-shaped clock hands. The Locksport Timer includes SPLIT, OPEN and a moving rim. [Theme guide](docs/SPARROWS.md).

| Radar | Watch | Locksport Timer |
|---|---|---|
| ![Sparrows radar](web/assets/radar-sparrows-01315.png) | ![Sparrows watch](web/assets/watch-sparrows-01315.png) | ![Sparrows timer](web/assets/timer-sparrows-01315.png) |

## Locksport

| McNally × Covert Instruments | LockPickingLawyer × CI | DMAC | Lockpickers United |
|---|---|---|---|
| ![McNally pick watch](web/assets/watch-covert-01314.png) | ![LockPickingLawyer watch](web/assets/watch-lpl-0138.png) | ![DMAC pick watch](web/assets/watch-dmac-0136.png) | ![LPU watch](web/assets/watch-lpu-0139.png) |
| ![McNally timer](web/assets/timer-covert-01314.png) | ![LockPickingLawyer timer](web/assets/timer-lpl-0138.png) | ![DMAC timer](web/assets/timer-dmac-0136.png) | ![LPU timer](web/assets/timer-lpu-0139.png) |

**START / PAUSE** controls the attempt, **SPLIT** records a milestone, and **OPEN** records the finish. The rim animates while counting and stops when paused. **LOG** holds the latest 50 splits until reset or reboot. TOOOL and TOOOL Dark also include matching locksport timers.

Images are firmware display captures. [Locksport gallery](https://valleytechsolutions.github.io/Dragon-Detector/#locksport)

## Tools

- **Detection and Hunt:** Wi-Fi/BLE signature matching, evidence labels, filtered signal strength, per-family alerts, watch/ignore rules, and local encounter history.
- **Recon:** surveys, protocol counters, packet captures, sessions, and CSV exports. [Recon guide](docs/RECON-013.md)
- **BLE Lab:** GATT inspection, explicit pairing, profile storage, emulation, and a cached-read bridge. [Capabilities and limits](docs/FEATURE-MATRIX.md)
- **Clock and timers:** analog/digital watches, stopwatch, locksport timing, and separate Time, Date, and Sync settings. USB sets local time and timezone. Devices without an external RTC need USB or configured Wi-Fi after complete power loss. [Clock setup](docs/CLOCK-SETUP.md)
- **Badge Studio:** prepare and upload your own artwork from the browser. [Open Studio](https://valleytechsolutions.github.io/Dragon-Detector/#studio)

Detection matches observable radio signatures; it does not establish intent, ownership, or recording state. Radar radius represents signal strength, and its angle is decorative. Silent devices, Bluetooth Classic, and 5/6 GHz Wi-Fi are outside coverage. [Detection evidence](docs/DETECTION.md)

## Development

[Build instructions](docs/BUILDING.md) · [Security](SECURITY.md) · [Release notes](https://github.com/valleytechsolutions/Dragon-Detector/releases)

| Directory | Contents |
|---|---|
| `firmware/` | Shared application and board adapters |
| `web/` | USB flasher, device tools, and Badge Studio |
| `tools/` | Build, packaging, and validation scripts |
| `tests/` | Native firmware and browser-module tests |
| `hardware/` | Board partitions and archived projects |
| `docs/` | Setup guides, architecture, and validation |

CI tests the firmware core and browser modules, builds all five profiles, and publishes matching firmware, manifests, and SHA-256 checksums. Historical projects under `hardware/` retain their original licenses and are not current update targets.

## Credits and license

Original application, web, and tooling code: [MIT](LICENSE), Your Pal Kal / Valleytech Custom Solutions. Dependencies, reference research, and supplied artwork retain their own rights and notices. [Attributions](CREDITS.md)

Dragon Detector is an independent project. Brand and character artwork does not imply affiliation or endorsement.
