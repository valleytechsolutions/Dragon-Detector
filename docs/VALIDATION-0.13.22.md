# 0.13.22 validation

Validated on 2026-10-19. Compilation and synthetic BLE tests do not establish
that every tracker will make an audible sound.

Automated checks completed:

- Native production-code tests: sequential beeper scheduling, fairness across
  384 targets, fresh-observation requirement, retry backoff, explicit rejection,
  Stop, timer rollover and capacity boundaries.
- Distinct RGB565 colors for 384 devices within each detector family.
- Press identity retention and scaled radar hit areas at 240, 360 and 466 pixels.
- Selection expiry with an older cached animation timestamp, the ten-second
  boundary and timer rollover; two release events queued in one frame.
- Existing parser, detector priority, crowded-tracker and SD/GPS tests in the core suite.
- 100 browser/unit tests.

## Connected M5Stack StopWatch C152

- Identified the board over USB and backed up its full 16 MB flash before the
  app-only update from 0.13.20. Settings, partition layout and internal files
  were retained.
- Reproduced the disappearing selection: a press newer than the cached radar
  frame timestamp caused unsigned subtraction to expire the selection at once.
  The corrected elapsed-time comparison passed on the device.
- Injected presses and releases through the board's actual LVGL input driver:
  a dot stays selected after release, releasing elsewhere preserves its identity,
  double-tapping opens details and tapping the summary opens details.
- The user then confirmed that normal physical tap-and-release and opening
  details both work on the updated StopWatch.
- Observed distinct dot colors on the device. The native tests cover uniqueness
  at the full 384-contact capacity.
- A synthetic Windows BLE AirTag service received two sequential sound writes.
  The device's touch Stop button ended the session; no additional write arrived
  during the following fifteen seconds. The badge checksum was unchanged.
- The built-in self-test passed 23 checks with no failures.

The synthetic service validates protocol writes and scheduling, not a real
AirTag's speaker. Audible repeated beeping on physical trackers remains
unverified. Other boards receive the shared fix but were not physically tested
for this release. New SD/GPS functions on the non-C Waveshare 1.75 variants
remain beta and still need physical SD/GPS and WiGLE import testing.

## Release builds

All seven final profiles compiled successfully from the same firmware source:
`spotpear`, `waveshare`, `stopwatch`, `waveshare_175`,
`waveshare_175_gps`, `xiao_round` and `waveshare_lite`.
Each passed PlatformIO's RAM and application partition size checks.

Release packaging verifies every merged-image offset, binary SHA-256 and
filesystem size. The published installer keeps separate factory and compatible
update manifests; compatible updates omit filesystem images. Publication
records accompanying the local release identify the source commit and verify
the downloaded binary hashes for all seven boards.
