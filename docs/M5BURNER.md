# M5Burner · M5Stack StopWatch C152

The **v0.13.15** factory image matches the [GitHub release](https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.13.15). It includes 31 themes, 29 logo watches, locksport timers, the Dragon Ball/BWM startup, corrected McNally branding and 2 MiB capture/session limits.

**Submission checked October 17, 2026:** **Public / Pending review** in the existing Dragon Detector project. The downloaded 16 MB image matches the release byte for byte. M5Stack controls catalog approval.

## Installation

1. Select **Dragon Detector** by **ValleytechSolutions**, device **StopWatch**, version **0.13.15** after approval. This image is for **C152 only**.
2. Back up saved files and settings. A factory image replaces them. For a compatible existing installation, use **Update matching Dragon Detector** in the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/#install).
3. Connect USB. Hold the StopWatch power button for about two seconds until the green LED lights, then release. Burn at **0x0**, **ESP32-S3**, **16 MB**, **DIO / 80 MHz**. No separate filesystem upload is needed.
4. Briefly press the power button to restart. Close M5Burner's serial connection, then connect **Detector tools** in the web flasher to set and verify time, date and timezone. M5Burner does not synchronize the clock.

On the device, **Settings → Time** has separate **Time**, **Date** and **Sync** tabs. The RX8130CE RTC keeps time while its backup supply is powered. Resync after travel or daylight-saving changes. [Clock setup](CLOCK-SETUP.md).

## Package verification

- Factory image: `Dragon-Detector-0.13.15-M5Stack-StopWatch-C152-FACTORY-0x0.bin`, **16,777,216 bytes**.
- SHA-256: `0959e4fd9b522c81bf93e1975229a7e75a6521c455a088ec1dfa6367b125fa77`.
- Source: `b83a33aba0406a580125630eaeae18fde215764f`; application: **2,980,352 bytes**.
- All merged offsets match release segments. NVS and the spare OTA slot contain erased bytes; no personal device dump is included.
- Released application passed 23/23 C152 self-tests and RTC checks. Factory installation through M5Burner itself was not hardware-tested.
- M5Burner project `2111312268664066049`, version `2111581181645770754`.

| Startup | Capsule Corp radar | Sparrows watch |
|---|---|---|
| ![Startup](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.15/StopWatch-startup-0.13.15.png) | ![Radar](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.15/StopWatch-radar-capsule-0.13.15.png) | ![Watch](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.15/StopWatch-watch-sparrows-0.13.15.png) |

Display captures from a physical StopWatch. [Credits](../CREDITS.md) · [Validation](VALIDATION.md).
