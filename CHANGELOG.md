# Changes

## 0.3.0 — daily badge development release

- Unified the application across Spotpear JC3636W518, Waveshare 1.75C and M5Stack StopWatch C152.
- Added browser crop/zoom/center/fit and CRC-verified, rollback-preserving USB badge upload; persisted image and startup mode; idle dimming and slow pixel shift.
- Added Axon company/services, Samsung SmartTag, Flock accessory/strict probe and Biscuit composite evidence; reduced vendor/service false positives and required fresh repeat evidence.
- Added median/time-based RSSI filtering, stability/trend information, dedicated target listening and stale-target status; preserved random-address identity boundaries.
- Added board-specific factory/update web flashing, release checksums, native/parser/browser protocol checks and GitHub Actions/Pages deployment.
- Applied MIT to the owner's current original code. Retained historical GPL releases and third-party notices; excluded the GPL audio decoder from current builds; used a pinned Apache-2.0 Waveshare CO5300 driver.

Physical board and controlled field validation remain necessary; see `docs/VALIDATION.md`.
