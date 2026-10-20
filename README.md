<p align="center"><img src="media/branding/dragon-detector.png" width="220" alt="Dragon Detector"></p>

# Dragon Detector

Wi-Fi and Bluetooth detection, signal tracking, and logo watch faces for ESP32-S3 displays.

**Web flasher available:** [Install from the independent host](https://valleytech-dragon-detector.pages.dev/). The original GitHub Pages address and GitHub-hosted images are temporarily unavailable. Source, artwork and release files are intact; existing devices are unaffected. [Hosting status](docs/HOSTING-STATUS.md).

[**Install firmware**](https://valleytech-dragon-detector.pages.dev/#install) · [Latest release](docs/RELEASE-0.13.23.md) · [User guides](docs/GUI.md) · [Clock setup](https://git.churchofmalware.org/Valleytech/Dragon-Detector/wiki/Clock-Setup)

## Radar controls and repeating Find My Beeper · 0.13.22

Press a dot for immediate selection, then tap its summary or double-tap for details.
Device shades are distinct within their type's color range; AirTags stay low priority.
Find My Beeper can repeat one selected tracker or **Beep All**, with a visible **Stop**
control, sequential requests and per-device results. [Instructions](docs/RELEASE-0.13.22.md)
· [Validation](docs/VALIDATION-0.13.22.md).

## Elecrow CrowPanel 2.1 Rotary · experimental port

A native 480 × 480 stationary profile with touch and rotary controls: turn to highlight, press to select, and hold to return home. Internal storage replaces SD functions; time synchronizes over USB or a saved Wi-Fi network. Display centering, ring navigation, scanning and clock/timer operation have passed initial hardware checks. [Installation and controls](docs/ELECROW-ROTARY-21.md).

## Waveshare AMOLED 1.75 / 1.75-G · 0.13.21 beta

The tested non-C baseline now has SD-first profiles, events, captures and surveys,
plus a separate LC76G GPS build for passive Wi-Fi/BLE wardriving and WiGLE CSV.
Tap a dot for its summary; double-tap for details, Hunt and BLE/GATT tools.
New SD/GPS/touch behavior awaits physical testing. No mandatory SD pack is needed.
[Beta guide](docs/WAVESHARE-AMOLED-175.md) · [Change notes](docs/RELEASE-0.13.21.md).

## Clock and timer power saving

On every board, the clock and stopwatch/Locksport Timer pause background Wi-Fi and Bluetooth scanning. Lap history and time/date settings also stay paused. Return to the radar or other tools to resume scanning; a running timer keeps counting in the background. Previously configured Wi-Fi clock recovery may briefly connect for time synchronization. See [power-saving behavior](docs/CLOCK-POWER.md).

## Personal themes

Build your own clock, radar and counter in the [Custom theme tab](https://valleytech-dragon-detector.pages.dev/#theme-studio). Add a logo, choose colors, hands and numerals, and enable locksport controls. The personal slot appears on the device only after you explicitly install a design. [Creator guide](docs/CUSTOM-THEME.md).

## Install

1. Open the [web flasher](https://valleytech-dragon-detector.pages.dev/#install) in desktop Chrome or Edge and connect your board with a USB data cable.
2. Select the exact board. Use **Update matching Dragon Detector** for a compatible installation, or **Fresh install** for a new board. Fresh install replaces saved files and settings.
3. Choose **Connect & install** and keep USB connected until installation and automatic date/time verification finish.

[Installation and recovery](docs/FLASHING.md) · [M5Burner package](docs/M5BURNER.md) · [Release checksums](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases/latest)

## Supported hardware

Some store links are affiliate links. Purchases through them may earn Valleytech a commission.

| Board | Display | Controls and peripherals | Get the hardware |
|---|---|---|---|
| **Waveshare ESP32-S3-Touch-AMOLED-1.75C** | 466 × 466 AMOLED | Touch, BOOT/PWR, 32 MB flash; choose the **C** model | [Waveshare](https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm?&aff_id=122099) |
| **Waveshare ESP32-S3-Touch-AMOLED-1.75 · SD beta** | 466 × 466 AMOLED | Touch, BOOT/PWR, RTC, microSD, 16 MB flash; select **Standard version** | [Waveshare](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm?&aff_id=122099) |
| **Waveshare ESP32-S3-Touch-AMOLED-1.75-G · GPS beta** | 466 × 466 AMOLED | RTC, microSD, onboard LC76G GPS; select **GPS version** | [Waveshare · choose GPS](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm?&aff_id=122099) |
| **M5Stack StopWatch C152** | 466 × 466 AMOLED | Touch, yellow/blue buttons, RTC, speaker, haptics, 16 MB flash | [M5Stack](https://shop.m5stack.com/products/m5stack-stopwatch-dev-kit-esp32-s3?ref=marvvxzb&variant=48862593515777) |
| **Elecrow CrowPanel 2.1 Rotary · Experimental** | 480 × 480 IPS | Touch, rotary ring and press, 16 MB flash; V1.0 board | [Elecrow](https://www.elecrow.com/crowpanel-2-1inch-hmi-esp32-rotary-display-480-480-ips-round-touch-knob-screen.html) |
| **Spotpear / Guition JC3636W518** | 360 × 360 LCD | Touch, optional microSD, 16 MB flash; match the board model | [Spotpear](https://spotpear.com/shop/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3/EPS32-S3-1.8inch-Touch-LCD-360-Aida64.html) |
| **Seeed XIAO ESP32-S3 + Round Display** | 240 × 240 LCD | Touch, RTC, microSD, 8 MB flash; **both parts required** | [ESP32-S3 board](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html?c_event_medium=search%3A%3Aresults&c_event_id=2529207033%3A%3Axiao+round+display&sensecap_affiliate=qV5RXdv&referring_service=link) + [Round Display](https://www.seeedstudio.com/1-28-Round-Touch-Display-for-Seeed-Studio-XIAO-ESP32.html?c_event_medium=search%3A%3Aresults_recommend&c_event_id=2529207033%3A%3Axiao+round+display&sensecap_affiliate=qV5RXdv&referring_service=link) |
| **Waveshare ESP32-S3-LCD-1.28 Lite** | 240 × 240 LCD | BOOT button, 16 MB flash; **non-touch** model | [Waveshare](https://www.waveshare.com/esp32-s3-lcd-1.28.htm) |

The seven full profiles share the radar, 37 built-in themes, logo watch faces, timers, notebook, badge editor, radio tools, and BLE Lab. StopWatch and Waveshare AMOLED support 2 MiB captures and 2 MiB recon sessions. Their total file storage differs.

Lite provides five button-operated pages: radar, clock, timer, signals, and diagnostics. It shares detection and all 37 built-in themes, with compact clocks. It does not include Badge Studio, audio, or active labs.

Firmware is board-specific. The Waveshare **1.75C**, **1.75** and **1.75-G** each have a separate installer profile. New non-C SD/GPS features await field testing; Elecrow has passed initial checks on one unit. [Hardware guide](docs/HARDWARE.md) · [Feature comparison](docs/FEATURE-MATRIX.md) · [Tested hardware and limitations](docs/VALIDATION.md)

## Radar and watches

| Valleytech | BWM | Dragon Ball radar |
|---|---|---|
| ![Valleytech clock](web/assets/watch-valleytech-0136.png) | ![BWM clock](web/assets/watch-bwm-0136.png) | ![Dragon Ball radar](web/assets/radar-dragon-01313.png) |

Choose a theme in **Settings → Theme & radar labels**. It applies to the radar, matching clock, menus, and dialogs. The Dragon Ball/BWM animation appears at power-on on every board. Swipe the clock to choose another watch face.

The collection includes maker, community, character, and locksport designs. [Browse the theme gallery](docs/THEMES.md) · [Watch controls](docs/STOPWATCH-CLOCK-ORIENTATION.md) · [Artwork credits](CREDITS.md)

## Locksport

| TOOOL | TOOOL Dark |
|---|---|
| ![TOOOL watch](web/assets/watch-toool-0136.png) | ![TOOOL Dark watch](web/assets/watch-toool-dark-0136.png) |
| ![TOOOL Locksport Timer](web/assets/timer-toool-0136.png) | ![TOOOL Dark Locksport Timer](web/assets/timer-toool-dark-0136.png) |

**START / PAUSE** controls the attempt, **SPLIT** records a milestone, and **OPEN** records the finish. The rim animates while counting and stops when paused. **LOG** holds the latest 50 splits until reset or reboot.

Images are firmware display captures. [Locksport gallery](https://valleytech-dragon-detector.pages.dev/#locksport)

## Tools

- **Detection and Hunt:** Wi-Fi/BLE signature matching, evidence labels, filtered signal strength, per-family alerts, watch/ignore rules, and local encounter history.
- **Recon:** surveys, protocol counters, packet captures, sessions, and CSV exports. [Recon guide](docs/RECON-013.md)
- **BLE Lab:** GATT inspection, explicit pairing, profile storage, emulation, and a cached-read bridge. [Capabilities and limits](docs/FEATURE-MATRIX.md)
- **Clock and timers:** analog/digital watches, stopwatch, locksport timing, and separate Time, Date, and Sync settings. USB sets local time and timezone. Devices without an external RTC need USB or configured Wi-Fi after complete power loss. [Clock setup](docs/CLOCK-SETUP.md)
- **Badge Studio:** prepare and upload your own artwork from the browser. [Open Studio](https://valleytech-dragon-detector.pages.dev/#studio)

Detection matches observable radio signatures; it does not establish intent, ownership, or recording state. Radar radius represents signal strength, and its angle is decorative. Silent devices, Bluetooth Classic, and 5/6 GHz Wi-Fi are outside coverage. [Detection evidence](docs/DETECTION.md)

## Development

[Build instructions](docs/BUILDING.md) · [Security](SECURITY.md) · [Release notes](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases)

| Directory | Contents |
|---|---|
| `firmware/` | Shared application and board adapters |
| `web/` | USB flasher, personal-theme creator, device tools, and Badge Studio |
| `tools/` | Build, packaging, and validation scripts |
| `tests/` | Native firmware and browser-module tests |
| `hardware/` | Board partitions and archived projects |
| `docs/` | Setup guides, architecture, and validation |

CI tests the firmware core and browser modules, builds all eight profiles, and publishes matching firmware, manifests, and SHA-256 checksums. Historical projects under `hardware/` retain their original licenses and are not current update targets.

## Credits and license

Original application, web, and tooling code: [MIT](LICENSE), Your Pal Kal / Valleytech Custom Solutions. Dependencies, reference research, and supplied artwork retain their own rights and notices. [Attributions](CREDITS.md)

Dragon Detector is an independent project. Brand and character artwork does not imply affiliation or endorsement.
