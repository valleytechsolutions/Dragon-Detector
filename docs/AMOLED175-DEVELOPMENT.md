# AMOLED 1.75 SD and GPS development

Baseline: `45ecdd6496789e2223e1ae76d769a95c09f18bde`, version 0.13.20.
The owner reports successful third-party testing of the existing non-C
`waveshare_175` beta. This confirms that baseline, not the changes in this branch.
Develop in this separate worktree; preserve the main checkout and its commits.

Requested outcome:

- Prefer SD for BLE profiles/events, surveys, recon sessions, and captures on the
  16 MB non-C AMOLED 1.75, preserving the tested display/touch/RTC pin mapping.
- Buffered capture writes, useful storage/error reporting, and persistent scans.
- Separate `waveshare_175_gps` installation target for the onboard LC76G variant.
- Dedicated passive Wi-Fi + BLE wardrive mode, exclusive radio ownership,
  timestamped WiGLE CSV, checked GNSS fixes, clean stop and storage failures.
- Boot without an SD asset pack. Provision missing directories without formatting,
  overwriting user content, or requiring downloaded data for core functions.
- Review and credit Marauder, ChimeraBLE and JimGat/CYM workflows. Document
  supported features and hardware limits rather than claiming complete parity.
- Native tests for parsers, CSV, buffering and mode policy; build base/GPS and
  representative existing targets; package matching manifests and test artifacts.
  No connected target hardware is available, so GPS/SD field validation is pending.

Research:

- Waveshare hardware reference at `waveshareteam/ESP32-S3-Touch-AMOLED-1.75`:
  SDMMC CLK 2 / CMD 1 / D0 3; I2C SDA 15 / SCL 14; PCF85063 0x51;
  GPS LC76G command address 0x50, read address 0x54; TCA9554 reset P7.
  Preserve LCD reset 39, touch reset 40 and audio MCLK 42.
- Vendor `examples/arduino/09_LC76G_I2C/09_LC76G_I2C.ino` documents the
  little-endian length request and data request protocol. Implement bounded,
  cooperative transfers on the existing bus; do not copy its blocking demo.
- Existing SD logging uses `/dragon/session-*`. BLE Lab and all users of
  `bleLabStorage()` currently use LittleFS. A scoped VFS filesystem at
  `/sdcard/dragon` can retain their paths without cluttering the card root.
- Existing recon capture uses a 2 MiB RAM buffer and explicit Save. SD should
  stream complete records via a bounded queue, preserve completed segments,
  and report dropped records/failed writes.
- `JimGat/CYM` README: its OUI/dictionary assets are optional, with automatic
  directory provisioning. No CYM code or data assets will be imported.
- `MatthewKuKanich/ChimeraBLE/CARDPUTER_GUIDE.md`: automatically created
  `/ChimeraBLE` directory, optional OUI database; SD requirement for its
  launcher workflow is distinct from a requirement to boot this firmware.

Implementation is recorded in [0.13.21 release notes](RELEASE-0.13.21.md).
The new touch workflow also uses identity-checked double taps and direct BLE
Toolkit / GATT profile entry points. Automated validation does not replace the
physical-board checklist in the [hardware guide](WAVESHARE-AMOLED-175.md).

## October 26, 2026: connected hardware

Both non-C models are now available. Their complete factory flash images were
backed up and verified before installation. Version 0.13.39 fixes LC76G wake and
complete command/reply handling on the shared bus, adds a dedicated wardrive
dashboard and safe return to menu, and caches the classic radar background.
Physical SD captures have been exported and checked on each board. The earlier
"no connected hardware" statement above describes the original 0.13.21 work,
not this bench session. See [current validation](VALIDATION-0.13.39.md).

Piglet's asynchronous Wi-Fi schedule is now the GPS variant's reference: 100 ms
maximum active dwell per channel, then 1,500 ms of passive BLE. The antenna
outside the window acquired a fix; a finalized file contained 2,732 Wi-Fi and
3,969 BLE location-tagged rows, all checked after CRC-verified SD export. The
latest build maintains a published fresh fix through ordinary FIFO reads.
Explicit SD-configured WiGLE/WDGoWars uploads have native protocol/failure tests
and a verified physical UI, but await configured credentials for live service
validation. Original CSVs and per-service upload receipts are retained.
