# M5Burner · M5Stack StopWatch C152

The **v0.13.17** factory image matches the [GitHub release](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases/tag/v0.13.17). It includes 34 built-in themes, 32 logo watches and an optional personal theme, locksport timers, the Dragon Ball/BWM startup and 2 MiB capture/session limits. DZ_AZ adds feather hands and a feather sweep. Use the web flasher’s Custom theme tab to install a personal logo design; it remains hidden until installed. Clock and timer screens pause scanning.

**Catalog status:** the 0.13.17 upload attempt on October 18, 2026 was rejected by M5Burner’s shared-description validation, even when submitting the existing public description. No new catalog version was created. The verified factory package is attached to the GitHub release. Use the web flasher for the supported update flow.

## Installation

1. Download the [**v0.13.17 StopWatch C152 factory BIN**](https://valleytech-dragon-detector.pages.dev/firmware/stopwatch/factory-0x0.bin) and its [SHA-256 checksums](https://valleytech-dragon-detector.pages.dev/firmware/stopwatch/SHA256SUMS.txt). It is not yet available in the M5Burner catalog. This image is for **C152 only**; the web flasher provides the supported installation flow.
2. Back up saved files and settings. A factory image replaces them. For a compatible existing installation, use **Update matching Dragon Detector** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install).
3. Connect USB. Hold the StopWatch power button for about two seconds until the green LED lights, then release. Burn at **0x0**, **ESP32-S3**, **16 MB**, **DIO / 80 MHz**. No separate filesystem upload is needed.
4. Briefly press the power button to restart. Close M5Burner's serial connection, then connect **Detector tools** in the web flasher to set and verify time, date and timezone. M5Burner does not synchronize the clock.

On the device, **Settings → Time** has separate **Time**, **Date** and **Sync** tabs. The RX8130CE RTC keeps time while its backup supply is powered. Resync after travel or daylight-saving changes. [Clock setup](CLOCK-SETUP.md) · [Clock and timer power saving](CLOCK-POWER.md).

## Package verification

- Factory image: `Dragon-Detector-0.13.17-M5Stack-StopWatch-C152-FACTORY-0x0.bin`, **16,777,216 bytes**.
- SHA-256 and source revision: see `M5BURNER-VERIFICATION.json` and `M5BURNER-SHA256SUMS.txt` attached to the release.
- All merged offsets match release segments. NVS and the spare OTA slot contain erased bytes; no personal device dump is included.
- Released application passed 23/23 C152 self-tests, clock/timer scanning checks and RTC checks. Factory installation through M5Burner itself was not hardware-tested.
- Existing M5Burner project `2111312268664066049`. No new version ID was returned.

![Clock running with background scanning paused](../web/assets/stopwatch-clock-01317.png)

Display capture from a physical StopWatch. [Credits](../CREDITS.md) · [Validation](VALIDATION.md).
