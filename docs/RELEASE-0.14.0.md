# Dragon Detector 0.14.0

This release audits the current SquachWatch detection reference, fixes
classification precedence, groups target pages and keeps detection popups on
the radar screen. The shared detection code applies to all eleven supported
ESP32-S3 profiles.

## Detection evidence

- SquachWatch-CYD master `3e9be6c02265b14c798e58da01a0913866143b0f`
  was reviewed on 2026-11-01, including changes since the previous pinned
  audit. All 158 table entries were already represented; no new signature
  count is claimed. The signature tables and Remote ID decoder are unchanged
  between those revisions. The recent receive-code refactor adds Tab5's
  ESP-Hosted transport; these Dragon profiles use native ESP32-S3 radios.
- Existing Flock probe/name/accessory, ALPR/camera address, Axon manufacturer,
  service and address, tracker, glasses, radio-tool, traffic-anomaly and
  decoded BLE/Wi-Fi beacon/NAN Remote ID paths remain under regression tests.
- A weak Axon address clue no longer overwrites stronger decoded evidence.
  The same rule applies when later packets update an existing contact.
- [Adult Mode](ADULT-MODE.md) adds optional passive BLE candidate matching.
  It requires a saved explicit opt-in, remains silent and does not add adult
  classifications to persistent detection history or detection-event logs.

See [the coverage audit](COVERAGE-AUDIT.md) for exact parity boundaries and
hardware limitations. RF coverage and product identity cannot be guaranteed.

## Navigation and appearance

- Detection popups are allowed only while both the current and requested
  screen are Radar. Leaving Radar removes queued detection cards, including
  watch-list cards. Badge, clock, timer, menus and tools remain uninterrupted
  by detection popups; scanning/history follow their existing mode rules.
- Targets use separate, nonempty category pages: ALPR/Flock, Axon, cameras,
  drones, acoustic sensors, radio tools/activity, other signals, optional
  adult candidates, and finally trackers. Arrows advance through groups and
  extra ten-row pages within a group. Row identity is rechecked on selection.
- ALPR/Flock precede Axon and cameras for primary tags and priority ordering.
  Dial's Signals list uses the same category order.
- Removed the radar's tap-a-dot instruction and target-priority subtitle.
- Valleytech now uses a black field, gold trim, fine chapter/range rings and
  a restrained gold sweep. Detection colors remain consistent across themes.

## Validation and distribution

Host fixtures, all-board compilation, package verification and StopWatch
hardware checks are recorded with release artifacts. Build/decoder tests do
not prove field reception of every device or hardware behavior on untested
boards. M5Burner submissions remain subject to M5Stack review.

Update images preserve the existing partition layout, NVS and badge files.
Fresh factory images are generated from repository assets, never device
backups. The approved public showcase and GitHub front page are unchanged.
