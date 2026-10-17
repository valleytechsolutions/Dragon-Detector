# M5Burner — M5Stack StopWatch C152

The M5Burner submission uses the same clean **v0.13.12** StopWatch factory image as the [GitHub release](https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.13.12). It includes 31 themes, 29 logo watch faces, the Hackazillarex and m33k_t3ch animations, locksport timers, the VTS boot animation, and separate Time / Date / Sync settings.

**Submission checked October 17, 2026:** v0.13.12 is Public / Pending review in the existing Dragon Detector project (`2111312268664066049`), version `2111530771929190402`. The old v0.13.8 version remains a private historical entry. The complete 16 MB image downloaded back from M5Burner matches the release byte for byte. Public catalog approval is controlled by M5Stack.

## Install

1. Select **Dragon Detector** by **ValleytechSolutions**, device **StopWatch**, version **0.13.12** in M5Burner once M5Stack approves the public submission. This image is for **M5Stack StopWatch C152 only**.
2. Back up files and settings first. The merged factory image replaces NVS, saved badges and files even without a separate erase operation. For a compatible existing Dragon Detector installation, use **Update** in the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/) instead.
3. Connect USB. Hold the StopWatch power button for about two seconds until the green LED lights, then release to enter download mode. Burn at **0x0**, **ESP32-S3**, **16 MB**, **DIO / 80 MHz**. No separate filesystem upload is needed.
4. Briefly press the power button to restart after flashing. Close the M5Burner serial connection before opening the browser tools.
5. Open the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/) in desktop Chrome or Edge and connect **Detector tools** to the running StopWatch. This sets and verifies the computer's current time, date and timezone, including direct hardware RTC readback. **Sync my local time** repeats it without reflashing. M5Burner itself does not run this clock setup.

On the device, **Settings → Time** provides separate **Time**, **Date** and **Sync** tabs. Its RX8130CE RTC keeps time while its backup supply is powered. Resync after travel or daylight-saving changes. [Clock setup](CLOCK-SETUP.md).

## Image and verification

- Filename: `Dragon-Detector-0.13.12-M5Stack-StopWatch-C152-FACTORY-0x0.bin`
- Size: **16,777,216 bytes**; address **0x0**.
- SHA-256: `b422fda9a173044091e599ce7581cb8ea2f4d3be4169545c8cffe5693f59c5cf`.
- Source: `aca3563c73d430a17adf6c322ce6212bdf98a779` (`v0.13.12`, clean CI build).
- Embedded application: **2,955,616 bytes**, **29.53% application-slot free**. The 21% reserve guard is unchanged.
- Bootloader checksum/hash and all merged segment offsets verified; NVS and spare OTA slot contain only erased bytes. This is a release build, never a personal device dump.
- The released application passed **23/23 C152 self-tests**, RTC ticking/readback and restart retention. Factory installation through M5Burner itself has not been physically tested.

The release includes the merged BIN, `M5BURNER-SHA256SUMS.txt`, `M5BURNER-VERIFICATION.json` and four actual C152 logical display captures. [Validation](VALIDATION.md) · [Credits and licenses](../CREDITS.md).

| Hackazillarex radar | m33k_t3ch watch |
|---|---|
| ![Bone sweep on StopWatch](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.12/StopWatch-radar-hackazillarex-0.13.12.png) | ![Neon logo watch on StopWatch](https://github.com/valleytechsolutions/Dragon-Detector/releases/download/v0.13.12/StopWatch-watch-m33k-0.13.12.png) |

These are logical screenshots captured from the physical StopWatch running the release, not photographs of its panel.
