# Changes

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
