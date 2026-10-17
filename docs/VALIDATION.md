# Release verification · 0.13.15

[Release](https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.13.15) · [Build and test run](https://github.com/valleytechsolutions/Dragon-Detector/actions/runs/39073525657) · [Web deployment](https://github.com/valleytechsolutions/Dragon-Detector/actions/runs/39073757943)

All five board builds passed. The live web flasher's firmware segments, manifests and updated assets match the clean CI artifacts and source commit `b83a33aba0406a580125630eaeae18fde215764f`. Release downloads were checked against their SHA-256 hashes.

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

Updates preserved the existing partition layout and saved appearance. Actual display captures confirm the Capsule Corp and Sparrows radar/watch layouts, Sparrows Locksport Timer and Dragon Ball/BWM startup. StopWatch allocated its 2 MiB capture buffer and recorded BLE traffic while retaining more than 2 MiB free PSRAM; its recon session limit reports 2 MiB.

On all three connected boards, the Sparrows timer changed frames while running, held an identical frame and elapsed value while paused, and recorded SPLIT followed by OPEN correctly. The checks reset their own test attempts and restored each board's previous watch and radar selection.

XIAO and Lite are build/package-tested in this release. Lite hardware checks remain paused. AMOLED 1.75C and Spotpear report no external RTC; complete power loss requires USB synchronization or configured Wi-Fi recovery. Wi-Fi setup remains deferred.

The real web clock modules were exercised over a local serial-stream adapter. Browser USB selection and a complete browser flashing session were not hardware-tested. The M5Burner factory download matches the release; a factory burn through M5Burner itself was not hardware-tested. [M5Burner status](M5BURNER.md).

`RELEASE-VERIFICATION.json` and `M5BURNER-VERIFICATION.json` accompany the release. They contain the measured results and checksums, without private backups, credentials or scan identities. Earlier verification records remain with their [releases](https://github.com/valleytechsolutions/Dragon-Detector/releases).
