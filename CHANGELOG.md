# Changes

## 0.9.4 — Waveshare 1.28 Lite

- Add the non-touch Waveshare ESP32-S3-LCD-1.28 with 16 MB flash, 2 MB quad-SPI PSRAM, a GC9A01 panel and CH343 serial diagnostics. Select SPI3/HSPI explicitly for display initialization.
- Boot directly into a fixed, full-brightness radar. Share the current signature engine, Axon priority, RSSI tracking, family colors, labels and renderer with the full boards; cycle extra target pages every seven seconds.
- Exclude menu, touch, audio, badge and wearable implementations. Ignore old full-UI preferences, disable persistent history, and reject commands that pause scanning or enter unavailable modes.
- Use an 11.5 KB internal display tile and an eight-frame-per-second animation limit, with stable geometry across tiles. Add Lite recovery instructions, native paging tests, exact USB capability checks and the fifth flasher/CI profile.

## 0.9.3 — Axon reception and first-hit alerts

- Add saved Axon priority under Tools → Detectors: registered vendor evidence alerts on its first observation, including when other alerts require strong confidence. Respect quiet/ignore/snooze/family controls and cooldowns.
- Reserve receive capacity for registered Axon signals and drain those packets first. Prevent older ordinary packets from rolling a contact's state backward.
- Start with BLE; alternate 2.2-second phases and plan an eleven-channel Wi-Fi sweep at 200 ms dwell. Dedicated Hunt retains its radio lock.
- Keep public address evidence from empty advertisements, complete Axon fields before damaged tails, and Wi-Fi source evidence despite malformed information elements. Surface broad AXEOFF partial-UUID/random-address patterns as weak clues without assigning them a vendor identity.
- Add per-signature reception, alert, queue-drop and latency diagnostics through `axon status`, host transport/fuzz tests and six additional on-device checks. Shared by all four board profiles.

## 0.9.2 — Seeed XIAO Round Display

- Add an 8 MB XIAO ESP32-S3 + Seeed Round Display profile with GC9A01 LCD, buffered CHSC6X touch/BOOT input, BM8563 RTC and shared-SPI microSD. Preserve existing card files; keep logging/capture opt-in.
- Add native 240-pixel fonts/logo, full-frame PSRAM drawing, 240-pixel Badge Studio encoding and exact USB/manifest board identity. Visual alerts and RSSI hunting work without an onboard speaker.
- Extend the web flasher, release checks, CI, hardware/SD guide and attributions to all four boards. Test 240-pixel badges and rotation along with existing detection fixtures.
- Use 127-byte badge upload chunks and separated acknowledgements to avoid observed USB transfer stalls; verify CRC failure/interruption preserves the previous badge.

## 0.9.1 — Manual orientation, responsive touch and device colors

- Use saved 0°/90°/180°/270° display orientation on every board. Remove automatic rotation, calibration and motion polling; clear old Auto preferences on update. Match touch coordinates to the chosen angle.
- Buffer M5Stack touch transitions and timestamped hardware keys at 100 Hz so short presses survive a radar redraw. Use persistent PSRAM display buffers on its synchronous M5GFX path to reduce repeated drawing and free internal memory.
- Give all 26 device families distinct RGB565 colors shared by dots, leaders and labels across all three themes. Use opaque dark label plates, stronger leaders and dark marker outlines for contrast; retain hollow/segmented markers and explicit last-seen text for weak/older evidence. Add an on-device color legend.
- Keep the stopwatch, RTC editor, web daily tools and existing detector behavior. Add family-color uniqueness/contrast checks alongside stopwatch/date/rotation tests.

## 0.9.0 — Stopwatch, clock and steady orientation

- Add a monotonic stopwatch with start/pause, guarded reset, 50 recent split/total laps and background timing. Map M5Stack yellow/blue and Waveshare BOOT/short PWR; provide touch controls on every board.
- Add hardware RTC read/write with validation and readback, local date/time editing, fixed UTC offsets and USB clock configuration. Preserve UTC detection history and keep timing independent of wall-clock changes.
- Add filtered automatic rotation with a 1.2-second stable hold, flat/diagonal/motion rejection, touch suppression and cooldown. Save fixed angles, current-orientation lock and upright calibration. Fresh installs remain upright/locked.
- Use bounded display-tile rotation buffers and matching touch transforms; add native model tests and web clock/rotation protocol tests.
- Update the flasher, daily-tools documentation and hardware attribution.


## 0.8.0 · StopWatch and Badge Studio

- Rename the repository to Dragon-Detector and update flasher, guide and source links.
- Add exact-board badge previews, whole-image fit by default, rotate/mirror, precise pan/nudge controls, preview-only alignment guides, reset and circular PNG export. USB locks the detected model and resolution.
- Validate flasher board names, partition offsets, image paths and erase policy; retain the independently pinned ESP Web Tools 10.4.0.
- Remove the pre-initialization StopWatch I²C bus scan, map CST820B coordinates to the native panel, and add bounded USB input diagnostics. Keep yellow/blue buttons and visible Tools/Home navigation; remove swipe-to-open across all profiles.
- Preserve detection, RSSI hunting, family tones and the 100% brightness/volume defaults with no dim/off timers. All 134 pinned reference signature facts retain recognition/radar coverage.

## 0.7.0 — radar themes, complete labels and recent signals

- Add persistent Green radar, Dragon Ball and Black/gold/red themes with animated sweep and pulsing live dots. Green is the default; all retain the BWM1 logo and RX/count/battery. Rings remain unnumbered and radius uses filtered RSSI.
- Restore labels and leaders by default for every plotted dot. Six targets per radar page keep text readable; every contact remains available across pages and in Targets. Tap labels to open evidence. Add matching USB web appearance controls.
- Retain intermittent classified signals for up to two minutes with explicit age and segmented stale markers. This changes display retention only, not alert or hunt freshness. Recent retention and labels are configurable.
- Recognize service identifiers in 16-bit, zero-extended 32-bit and exact Bluetooth Base 128-bit advertising fields. Preserve confidence rules, custom-UUID exclusions, payload offsets and malformed-packet boundaries.
- Add service-width, label-layout, pagination, stale-marker and USB preference fixtures. Update flasher compatibility, actual theme images and Flipper/Pineapple Pager reception guidance.

## 0.6.0 — radar, tracking and traffic evidence

- Replace the radar heading with the supplied BWM1 logo, retain RX/targets/battery, add dBm rings, sweep ticks, smoother marker movement and continuous age fading. Cache contact angles instead of recalculating trigonometry per marker/frame.
- Cap median samples at 10 Hz before elapsed-time smoothing so repeated packets cannot dominate RSSI. Hunt shows raw RSSI; its 24-second trace contains actual reception gaps.
- Improve disconnect retry filtering with a dedicated recent-frame cache and retain channel/reason evidence. Detect both changing-address BLE bursts and rapid pairing payload changes, with bounded tables, duplicate suppression and cooldowns.
- Add Radio Activity, a radar warning banner, per-contact traffic evidence and a version-gated web shortcut. Update flasher compatibility, documentation, logo attribution and screenshots.

## 0.5.1 — diagnostic capture hardening

- Diagnostic screenshots now mirror an ordinary display refresh, including overlays, instead of constructing a temporary LVGL display context. This avoids the path active during one observed panic reset in 0.5.0; the precise original panic cause was not captured.
- USB image/file output uses bounded, paced writes and a larger transmit queue. Screenshots report a CRC32 and transfer failure explicitly. Runtime diagnostics retain the capture stage across a restart.
- Restore the independently pinned ESP Web Tools 10.4.0 loader after a firmware-version replacement changed its URL. Installation stays disabled if the component has not loaded or the browser lacks USB support.

## 0.5.0 — complete signature audit and radar visibility

- Audit every table entry at SquachWatch-CYD commit `a6167db`: 97 address patterns, 15 services, seven companies, seven exact names and eight SSID prefixes. Add per-entry recognition and radar tests plus runtime-predicate fixtures.
- Show first valid candidates independently of audible-alert confidence. Hollow dots mark weak clues; all families, including iBeacon, are on for fresh installs. Keep repeat evidence for ordinary alerts.
- Add missing shared-vendor, local-address, bare-service, battery-name, Apple pairing-size and changed-MAC Pwnagotchi clues with explicit uncertainty. Match six disconnect frames in three seconds, including changing source addresses.
- Expand contacts from 160 to 384 in PSRAM. Use an identity-checked lookup cache, indexed packet pool, bounded radio-processing batches, binary-search vendor lookups, paged lists and cached O(n log n) radar ordering. Remove the 60-dot cap.
- Add Targets shortcut, quick quiet toggle, radio-health screen, optional active BLE scan responses and stale Wi-Fi hunt channel searching. Fix traffic-burst radar/audio routing and stale list selection after slot reuse.
- Preserve display defaults, maximum-volume support, existing badge storage and update partition layout.

## 0.4.0 — detection and audio

- Add detector families, Remote ID, persistent history, watch/ignore/snooze controls, Dragon Hunt and training fox.
- Add 26 unique RTTTL melodies, configurable USB tone editor, Axon heartbeat and signal-dependent hunt feedback; support speakers across three board adapters.

## 0.3.1 — USB badge transfer fix

- Reserve a 4096-byte USB receive queue before serial startup. The default 256-byte queue dropped data from the original 1024-byte image chunks on a real Waveshare 1.75C.
- Pace browser uploads in acknowledged 128-byte chunks, also compatible with existing 0.3.0 firmware.
- Add transfer tests covering the original queue capacity, ACK pacing, a partial final chunk and device-error handling.

## 0.3.0 — daily badge development release

- Unified the application across Spotpear JC3636W518, Waveshare 1.75C and M5Stack StopWatch C152.
- Added browser crop/zoom/center/fit and CRC-verified, rollback-preserving USB badge upload; persisted image and startup mode; idle dimming and slow pixel shift.
- Added Axon company/services, Samsung SmartTag, Flock accessory/strict probe and Biscuit composite evidence; reduced vendor/service false positives and required fresh repeat evidence.
- Added median/time-based RSSI filtering, stability/trend information, dedicated target listening and stale-target status; preserved random-address identity boundaries.
- Added board-specific factory/update web flashing, release checksums, native/parser/browser protocol checks and GitHub Actions/Pages deployment.
- Applied MIT to the owner's current original code. Retained historical GPL releases and third-party notices; excluded the GPL audio decoder from current builds; used a pinned Apache-2.0 Waveshare CO5300 driver.

Physical board and controlled field validation remain necessary; see `docs/VALIDATION.md`.
