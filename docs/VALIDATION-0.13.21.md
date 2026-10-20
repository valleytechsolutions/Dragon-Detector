# Validation — AMOLED 1.75 SD/GPS beta

Automated validation was run on Windows on October 19, 2026. No 1.75/1.75-G
device was connected. The owner's successful test applies to the earlier
non-C baseline, not these new changes.

## Native checks

- `tools/test_core.py`: passed, including existing detector/protocol regressions,
  158 pinned SquachWatch table entries, malformed-input fuzz fixtures, and new
  GNSS, WiGLE CSV, queue, tap identity/timing and FAT32 replacement checks.
- `tools/test_sd_records.py`: passed against the production writer with a fake
  filesystem: short writes, card loss, reserve exhaustion, queue pressure,
  failed finalization, and 32 MiB segment rollover.
- `tools/test_gps_transport.py`: passed against the production transport with
  a fake shared I²C bus: delays, bounded FIFO draining, backlog handling, clock
  restoration, NACKs, short reads, invalid lengths and uptime rollover.
- `tools/test_amoled175.py`: passed C/non-C pin isolation, 16 MB partitions,
  RTC UTC round trips and missing/invalid RTC behavior.
- `tools/test_flash_budget.py`: three checks passed.

## Browser and packaging checks

- `node --test tests/*.test.js`: **99 passed**, zero failures.
- `node --check web/app.js`: passed.
- Static-site generation validates local guide links and packaged firmware hashes.
- Browser inspection confirmed selecting 1.75-G reveals the SD setup notice beside
  the installer and its link opens the correct card setup section.
- Fresh/update manifests are validated with the actual browser validator.
  Packaging verifies every merged image segment against its source image.
- The flasher bundle keeps the five other published profiles at 0.13.20.
  Only non-C 1.75 and 1.75-G packages advance to this beta.

## Firmware builds

The artifact folder's validation record contains final PlatformIO results and
image sizes for `waveshare_175`, `waveshare_175_gps`, `waveshare` and
`waveshare_lite`. The latter two are regression builds, not new published packages.
Dependencies emit existing upstream warnings; successful compilation is not a
hardware acceptance test.

## Pending physical acceptance

Validate SD throughput/removal/full-card behavior, long recordings, touch hit
areas, GPS acquisition and FIFO behavior, RTC/touch coexistence during GPS reads,
route accuracy, GPS loss/reacquisition, and real WiGLE import. Follow the
[board checklist](WAVESHARE-AMOLED-175.md#new-feature-field-checklist).

GATT emulation does not imply proprietary protocol compatibility. Microphone
recording and IMU gesture/step features have not been added. No hardware flashing
or WiGLE upload was performed. Publication status is recorded separately from
these automated and local browser checks.
