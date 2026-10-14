# Validation status · 0.4.0

This is an experimental development release. Automated signature checks and basic board checks do not establish field detection accuracy or all-day battery life.

All three board profiles compile locally: Spotpear JC3636W518, Waveshare 1.75C and M5Stack StopWatch C152. The final audio build includes full-range PCM/codec output and a 100% volume default. Release packaging checks are separate from physical hardware tests.

## Automated checks

- Native C++ positive/negative fixtures cover all added detector families, dedicated/shared vendor prefixes, public/random BLE addresses, Axon manufacturer/services, Meta composite evidence, Flock probe roles, Biscuit shared-UUID exclusions, Google Find Hub framing, Tile, Raven, iBeacon opt-in, and drone Remote ID BLE/beacon/NAN messages encoded with the upstream library.
- Tracking tests cover signal spikes, reacquisition, timer rollover, history traces, alert cooldown/auto-quiet, SSID conflicts including binary SSIDs and mesh false positives, and display defaults.
- 150,000 random inputs exercise both bounded radio parsers. A further 50,000 inputs exercise RTTTL parsing. All 26 default melodies validate; tests also cover invalid notes/tempo/octaves, size and duration bounds, silence, queue coalescing/expiry/cooldown/rollover, and hunt pitch/cadence bounds. CI runs the native suite with AddressSanitizer and UndefinedBehaviorSanitizer as well.
- Thirteen Node tests cover both image resolutions, RGB565/CRC, crop geometry, serial framing/disconnects/timeouts, acknowledged small transfers, and a full 512-row history export received in one USB chunk.
- The release packager checks partition sizes, every merged image segment, checksums and workstation-path exclusion. Manifests identify the exact board and distinguish fresh installs from updates.
- Browser QA checked detector/audio settings at mobile and desktop widths, 26 family choices, disabled disconnected controls, display defaults, and absence of JavaScript errors. Badge image selection/cropping was checked in 0.3.1. Hardware USB tests use host scripts implementing the same protocol; browser USB permission-picker and end-to-end upload remain unverified.

## Waveshare 1.75C hardware checks · 2026-10-13

The owner confirmed this exact board and working touch. A private 16 MB recovery image was saved before flashing. The 0.4.0 images were written and hash-verified with esptool.

- Wi-Fi/BLE reception counters advance, the display reports 466×466, and the on-device self-test passes all 17 checks. Short continuous runs and USB operations report no radio-queue or storage drops.
- A real BLE target was selected for continuous hunt reception; its live RSSI, peak and trace rendered. Hunt was stopped and balanced scanning restored. These are reception checks, not a calibrated range test.
- Real history was exported with the expected CSV structure. Sensitive local captures remain private and are excluded from the repository.
- Display output changed from 100% to the configured 8% after two seconds and to off after four seconds; waking restored 100%. Both timers were restored to Never. The settings screen image is an exact LVGL render, not a photograph.
- ES8311 and I2S output initialize successfully. A >255-byte custom RTTTL melody saved correctly; invalid RTTTL and overlong commands were rejected without replacing it. Silence, reload and actual restart persistence were verified, then the original Axon melody was restored.
- The owner heard both the Axon and glasses previews and requested higher volume. The driver now uses full codec gain and near-full-scale unclipped sine PCM, with user volume set to 100%. Volume remains configurable. Radio reception continued through preview playback.
- Radar startup, full brightness and no display timeouts were restored after checks.

## Retained badge checks from 0.3.1

A complete 434,320-byte RGB565 badge upload matched the LVGL screenshot pixel-for-pixel. Bad CRC and interrupted uploads preserved the prior image. A real restart restored the accepted badge. The temporary color test was removed. Version 0.3.1 fixed a USB receive-queue overflow with a 4096-byte receive buffer and acknowledged 128-byte chunks; this transfer implementation remains in 0.4.0.

## Remaining hardware limits

Spotpear and StopWatch still require physical testing, including speakers, buttons and haptics. Full touch alignment, maximum-volume acoustic quality, battery runtime and long-duration stability are unverified. No controlled field trial against every named product has been performed. Axon vendor evidence can include other Axon/TASER equipment; glasses and tracker models cannot always be distinguished. Radar angle is decorative and RSSI is not a measured distance or bearing.

Historical 0.1/0.2 reports apply to their archived board projects only.
