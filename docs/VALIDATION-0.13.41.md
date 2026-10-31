# 0.13.41 validation

44Delta firmware source: `945d5255b2e872cab9c3fb228c4bd17a25f3ee4e`.

- 142 web tests pass, including persistent-theme readback, invalid-ID rejection,
  version gating and text contrast for every UI palette.
- 228 compressed artwork variants decode to their original pixel CRCs.
- Native core checks pass, including timer state and detector fuzz tests.
- Actual LVGL clock/radar code renders at 240, 360, 466 and 480 pixels, with
  multiple hand/sweep positions and an unset clock. The clock has no digital
  time overlay.
- [GitHub build matrix](https://github.com/valleytechsolutions/Dragon-Detector/actions/runs/41218681761)
  checks all eleven profiles, native tests, manifests and repository secrets.
  Downloaded release artifacts are checked against their SHA-256 lists and
  expected clean source commit before staging.

## Connected StopWatch C152, October 31

A complete 16 MB private backup was saved before flashing. The existing
partition table matched the new release. Only the bootloader, partition table,
OTA selection and application segments were updated; NVS and LittleFS were
preserved. The flash write passed hash verification.

The device reports `0.13.41`, `M5Stack StopWatch C152`, 466px logical display,
theme 45 and clock 48. All 23 self-tests pass. Theme and clock selection survive
a software restart. The hardware RTC reports valid time after synchronization
and restart. This is not a full-power-loss retention test.

USB-injected UI checks pass for Start, Split, Pause, Resume, Open and Log.
Paused and finished elapsed time stays fixed; completed attempts and splits
appear in the log. This verifies the UI action path, not physical touch sensing
or button feel. Clock, radar and timer screenshots from the actual device pass
the USB transfer CRC check and have been visually inspected.

The tested StopWatch application is 3,463,856 bytes, leaving 730,448 bytes
(17.42%) in its 4 MiB application slot. Other profiles are build-validated;
they were not physically exercised for this theme release.

## Distribution limits

The StopWatch website package uses the locally built, device-tested application.
The other ten packages use the clean GitHub build artifacts from the source
commit above. Each package carries its own provenance and SHA-256 checksums.
M5Burner's separate catalog upload remains unconfirmed; see [M5Burner](M5BURNER.md).
Existing hardware beta labels and Waveshare field-test limitations still apply.
