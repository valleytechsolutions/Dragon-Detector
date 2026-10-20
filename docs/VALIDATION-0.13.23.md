# Ogg / XomBi validation — 0.13.23

Checked on 2026-10-20:

- Both supplied JPEGs are retained byte-for-byte, with hashes in the artwork
  ledger.
- The production logo decoder reproduced the expected CRC32 for all 186
  encoded assets across the three panel resolutions.
- Both clocks and radar fields were rendered through the actual firmware
  drawing code and native LVGL at 240, 360 and 466 pixels. Clock previews also
  cover an unset date/time.
- The 100 web tests passed, including palette text contrast and radar
  theme command parsing. Existing saved theme IDs remain unchanged.
- All seven profiles compiled successfully: Spotpear, Waveshare 1.75C,
  M5Stack StopWatch, Waveshare 1.75, Waveshare 1.75-G, XIAO Round and Waveshare
  Lite. Each passed the RAM and application-partition size checks.
- The new selector's version gate rejects 0.13.22 and malformed versions and
  accepts 0.13.23 and later 0.13 releases.

The StopWatch was no longer connected during this release. No physical board
was flashed or tested for the two new themes. Native renders verify layout and
artwork but are not photographs or physical display tests.

Build and download checks are recorded with the release package.
The previously validated radar tap and beeper changes are retained.
