# Release verification

## 0.13.20 · Find My Beeper

- The owner's AirTag was isolated by a battery-out/battery-in scan comparison. On Waveshare 1.75C, one direct sound write was acknowledged and the owner confirmed the audible beep. The BLE worker disconnected and returned to radar, with the stored badge unchanged.
- Controlled Windows peripherals verified the AirTag opcode, FMNA indication subscription before the write, and DULT acceptance/rejection responses. Windows peripheral reconnections were intermittent; DULT completion parsing is covered by native fixtures rather than a physical DULT accessory claim.
- Native checks cover UUID parsing, iBeacon wire encoding/decoding, malformed lengths, signed calibration values and DULT response distinctions, alongside the existing detector, packet-builder and fuzz suites.
- A separate BLE receiver observed the configured iBeacon UUID, major/minor and signed measured power; the timed run stopped and restored scanning.
- 96 web-module tests pass, including board/version gating, explicit address selection and iBeacon input validation.
- All six profiles compile. The non-C AMOLED 1.75, XIAO Round and Lite remain build-tested for this release. The real AirTag result applies to the tested 1.75C and AirTag combination.

The web update preserves NVS and filesystems. Active BLE tests pause scanning and release the radio when finished. No claim is made that every pairing advertisement produces a popup or crash on current phones. [Feature instructions](BLE-TOOLS-01320.md).

## 0.13.17

[Release and verification files](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases/tag/v0.13.17)

Historical GitHub evidence: passing build/test run `39159301105` and successful deployment `39159542534`. These run records are temporarily unavailable publicly; release verification files are linked above.

Firmware and web assets were built from `eb83437ad23f37851ee55089d0d0cfe8590d10ab`.

## Automated checks

- 89 browser-module tests, including custom format round trips, malformed input, upload/readback checksums, board identity checks and cancelled-install handling.
- Native and sanitizer suites, 150,000 detector fuzz inputs and 10,000 personal-theme header fuzz inputs.
- 168 compressed logo variants decoded against their original pixel CRC32.
- All five board builds and application-size checks. The release includes each profile’s memory budget.
- Gitleaks scans tracked source and Git history. This credential scan is not a comprehensive vulnerability assessment.

## Connected hardware

AMOLED 1.75C, M5Stack StopWatch C152 and Spotpear are checked over USB. Tests cover application write verification, 23/23 self-tests, current time and offset, software restart, personal-theme upload/readback and hidden-slot behavior. StopWatch also receives hardware RTC readback and ticking checks. The AMOLED transfer tests include a wrong checksum and incomplete upload, both preserving the preceding theme.

Clock, timer, lap history and Time/Date/Sync settings pause scanning while time advances. Timer controls and resumed Wi-Fi/BLE reception are checked. DZ_AZ and personal-theme screenshots are actual firmware framebuffer captures. The Custom theme editor is exercised in a browser for import/export, preview controls, tab navigation and opt-in state.

Run-specific results, released binary hashes and live-flasher checks are recorded in `RELEASE-VERIFICATION.json`. Device updates preserve partitions, saved files and preferences. Test-created personal designs are removed before restoring the original theme, watch and daily screen.

The live web flasher's five firmware packages and new web assets match the release builds. All 13 GitHub release attachments were downloaded using the authenticated GitHub CLI and matched both the local package hashes and GitHub's SHA-256 digests.

## Limits

- XIAO and Lite are build/package-tested; Lite hardware tests remain paused.
- The real web transfer and clock modules run through a local serial-stream adapter for hardware tests. A complete browser USB chooser/firmware-flash session is not hardware-tested.
- No battery-life or current-consumption percentage is claimed.
- AMOLED 1.75C and Spotpear require USB or configured Wi-Fi time recovery after full power loss. Wi-Fi setup remains deferred; live NTP recovery is not tested here.
- The StopWatch factory package is assembled from clean release segments. Factory installation through M5Burner itself and catalog publication are not verified. [M5Burner status](M5BURNER.md).
- Signed-out GitHub requests returned HTTP 404 for the release, repository and owner profile during final verification on October 18, 2026. Public GitHub download access could not be confirmed; the live web flasher and its firmware downloads were reachable and verified.

Earlier evidence remains attached to its [release](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases).

## 0.13.19 non-C AMOLED 1.75

The `waveshare_175` port is prepared from the official schematic and maintained BSP. Automated pin/partition/manifest checks and compilation are separate from physical validation. No physical unit has been tested for this port. Display, touch, RTC retention, buttons, SD, audio and battery operation remain on the [arrival checklist](WAVESHARE-AMOLED-175.md#arrival-checklist).
