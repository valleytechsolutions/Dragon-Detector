# M5Burner · M5Stack StopWatch C152

The **v0.13.16** factory image matches the [GitHub release](https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.13.16). It includes 33 themes, 31 logo watches, locksport timers, the Dragon Ball/BWM startup and 2 MiB capture/session limits. Clock, timer, lap history and time/date settings now pause background scanning; leaving those screens resumes detection.

**Catalog status checked October 18, 2026:** the M5Burner website rejected the new-version upload, reporting that new versions cannot modify the shared firmware description, including when the form reused the saved text. No 0.13.16 catalog entry was created. The previous 0.13.15 entry remains pending review. Download 0.13.16 from the GitHub release or install through the web flasher.

## Installation

1. Download the **v0.13.16 StopWatch C152 factory BIN** from the GitHub release. It is not yet available in the M5Burner catalog. This image is for **C152 only**; the web flasher provides the supported installation flow.
2. Back up saved files and settings. A factory image replaces them. For a compatible existing installation, use **Update matching Dragon Detector** in the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/#install).
3. Connect USB. Hold the StopWatch power button for about two seconds until the green LED lights, then release. Burn at **0x0**, **ESP32-S3**, **16 MB**, **DIO / 80 MHz**. No separate filesystem upload is needed.
4. Briefly press the power button to restart. Close M5Burner's serial connection, then connect **Detector tools** in the web flasher to set and verify time, date and timezone. M5Burner does not synchronize the clock.

On the device, **Settings → Time** has separate **Time**, **Date** and **Sync** tabs. The RX8130CE RTC keeps time while its backup supply is powered. Resync after travel or daylight-saving changes. [Clock setup](CLOCK-SETUP.md) · [Clock and timer power saving](CLOCK-POWER.md).

## Package verification

- Factory image: `Dragon-Detector-0.13.16-M5Stack-StopWatch-C152-FACTORY-0x0.bin`, **16,777,216 bytes**.
- SHA-256: `f883ad6b6f83ed862bc63e1eb850f80da07f908bee96e7d48113ac50ca3abc92`.
- Source: `4ff5ee2d690f3a5374c62483c0d09f900d8e2c43`; application: **2,980,784 bytes**.
- All merged offsets match release segments. NVS and the spare OTA slot contain erased bytes; no personal device dump is included.
- Released application passed 23/23 C152 self-tests, clock/timer scanning checks and RTC checks. Factory installation through M5Burner itself was not hardware-tested.
- Existing M5Burner project `2111312268664066049`. No new version ID was returned.

![Clock running with background scanning paused](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.16/StopWatch-clock-current-0.13.16.png)

Display capture from a physical StopWatch. [Credits](../CREDITS.md) · [Validation](VALIDATION.md).
