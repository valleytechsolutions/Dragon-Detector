# M5Burner — M5Stack StopWatch C152

The **v0.13.13** factory image matches the [GitHub release](https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.13.13). It includes the simple Dragon Ball / BWM startup and logo watch, all 31 themes and 29 watches, locksport timers and the existing clock fixes.

**Submission checked October 17, 2026:** v0.13.13 is **Public / Pending review** in the existing Dragon Detector project (`2111312268664066049`), version `2111536584752189441`. Its full 16 MB download matches the release byte for byte. M5Stack controls public catalog approval.

## Install

1. Select **Dragon Detector** by **ValleytechSolutions**, device **StopWatch**, version **0.13.13** in M5Burner once M5Stack approves the public submission. This image is for **M5Stack StopWatch C152 only**.
2. Back up files and settings first. The merged factory image replaces NVS, saved badges and files even without a separate erase operation. For a compatible existing Dragon Detector installation, use **Update** in the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/) instead.
3. Connect USB. Hold the StopWatch power button for about two seconds until the green LED lights, then release to enter download mode. Burn at **0x0**, **ESP32-S3**, **16 MB**, **DIO / 80 MHz**. No separate filesystem upload is needed.
4. Briefly press the power button to restart after flashing. Close the M5Burner serial connection before opening the browser tools.
5. Open the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/) in desktop Chrome or Edge and connect **Detector tools** to the running StopWatch. This sets and verifies the computer's current time, date and timezone, including direct hardware RTC readback. **Sync my local time** repeats it without reflashing. M5Burner itself does not run this clock setup.

On the device, **Settings → Time** provides separate **Time**, **Date** and **Sync** tabs. Its RX8130CE RTC keeps time while its backup supply is powered. Resync after travel or daylight-saving changes. [Clock setup](CLOCK-SETUP.md).

## Image and verification

- Filename: `Dragon-Detector-0.13.13-M5Stack-StopWatch-C152-FACTORY-0x0.bin`; **16,777,216 bytes**, address **0x0**.
- SHA-256: `553340aaf81f5db39809818570b12a291e284c9e95fe7693f6b1ef2552cff0ab`.
- Source: `613b3cb1f8b87e8080b8da9bc835d040d4005709` (`v0.13.13`, clean CI).
- Application: **2,957,696 bytes**, **29.48% slot free**, with the 21% guard unchanged.
- All merged offsets match release segments; NVS and spare OTA slot contain erased bytes. This is a release build, never a personal device dump.
- Released application passed 23/23 C152 self-tests, RTC ticking/readback and powered restart retention. Factory installation through M5Burner itself is not hardware-tested.

The release includes the factory BIN, checksums, verification JSON, project logo and actual startup/radar/watch display captures.

| Startup | Logo watch |
|---|---|
| ![Startup on StopWatch](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.13/StopWatch-startup-0.13.13.png) | ![Logo watch on StopWatch](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.13/StopWatch-watch-dragon-0.13.13.png) |

Logical screenshots captured from the physical StopWatch, not panel photographs. [Credits](../CREDITS.md) · [Artwork provenance](DRAGON-BRAND.md).
