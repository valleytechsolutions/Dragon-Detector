# Release verification · 0.13.16

[Release](https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.13.16) · [Build and test run](https://github.com/valleytechsolutions/Dragon-Detector/actions/runs/39138745421) · [Web deployment](https://github.com/valleytechsolutions/Dragon-Detector/actions/runs/39138999457)

All five board builds passed. The live web flasher's firmware segments, manifests and assets match the clean CI artifacts and source commit `4ff5ee2d690f3a5374c62483c0d09f900d8e2c43`. Release downloads were checked against their SHA-256 hashes.

## Automated checks

- 81 web tests, native and sanitizer suites, and 150,000 parser fuzz inputs.
- 162 compressed logo variants decoded to their expected pixel CRC32.
- Three flash-budget tests, including integer rounding and the application limit.
- Gitleaks found no credentials in tracked source or Git history. GitHub Actions are pinned to commit hashes. This is a credential scan, not a comprehensive vulnerability assessment.

## Connected hardware

| Board | Released application | Self-tests | Clock verification |
|---|---|---|---|
| Waveshare AMOLED 1.75C | Written and hash-verified | 23/23 | Current time, offset and powered restart |
| M5Stack StopWatch C152 | Written and hash-verified | 23/23 | Current time, offset, RTC readback/ticking and powered restart |
| Spotpear JC3636W518 | Written and hash-verified | 23/23 | Current time, offset and powered restart |

On all three connected boards, clock, timer, lap history and Time/Date/Sync settings stopped both Wi-Fi/BLE listening counters and packet counts while time continued advancing. Start, lap, pause and reset worked with scanning paused. Returning to radar resumed both radios and received traffic. The serial resume command could not override the pause. A separate AMOLED check stopped a passive capture on entering the clock, retained its data and rejected a new capture while paused.

Updates preserved the partition layout, saved theme, watch and selected radio mode. Each device was returned to its previous screen. No current-consumption or battery-life percentage was measured.

XIAO and Lite are build/package-tested in this release. Lite hardware checks remain paused. AMOLED 1.75C and Spotpear report no external RTC; complete power loss requires USB synchronization or configured Wi-Fi recovery. Wi-Fi setup remains deferred; live NTP recovery was not tested in this release.

The real web clock modules were exercised over a local serial-stream adapter. Browser USB selection and a complete browser flashing session were not hardware-tested. The StopWatch factory image was assembled from the same verified CI segments and its GitHub download was checked. M5Burner's upload form rejected the new version; no 0.13.16 catalog entry was created. A factory burn through M5Burner itself was not hardware-tested. [M5Burner status](M5BURNER.md).

`RELEASE-VERIFICATION.json` and `M5BURNER-VERIFICATION.json` accompany the release. They contain measured results and checksums, without private backups, credentials or scan identities. Earlier verification records remain with their [releases](https://github.com/valleytechsolutions/Dragon-Detector/releases).
