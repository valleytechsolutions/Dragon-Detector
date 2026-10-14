# Validation status · 0.7.0

This is an experimental development release. Automated signature checks and basic board checks do not establish field detection accuracy or all-day battery life.

All three board profiles compile locally: Spotpear JC3636W518, Waveshare 1.75C and M5Stack StopWatch C152. The final audio build includes full-range PCM/codec output and a 100% volume default. Release packaging checks are separate from physical hardware tests.

## Automated checks

- The 0.5 coverage ledger enumerates all 134 table entries in SquachWatch `a6167db`: each must be recognized and radar-visible even with strong-only audible alerts. Additional runtime-name, Apple pairing, changed-MAC Pwnagotchi, scan-response padding/boundary, address-cache collision/reuse, event-overlay, disconnect-window and lost-channel fixtures pass. See [the audit](COVERAGE-AUDIT.md).

- Native C++ positive/negative fixtures cover all added detector families, dedicated/shared vendor prefixes, public/random BLE addresses, Axon manufacturer/services, Meta composite evidence, Flock probe roles, Biscuit shared-UUID exclusions, Google Find Hub framing, Tile, Raven, iBeacon defaults, and drone Remote ID BLE/beacon/NAN messages encoded with the upstream library.
- Tracking tests cover signal spikes, reacquisition, timer rollover, history traces, alert cooldown/auto-quiet, SSID conflicts including binary SSIDs and mesh false positives, and display defaults.
- 150,000 random inputs exercise both bounded radio parsers. A further 50,000 inputs exercise RTTTL parsing. The runner explicitly keeps assertions enabled in optimized builds. All 26 default melodies validate; tests also cover invalid notes/tempo/octaves, size and duration bounds, silence, queue coalescing/expiry/cooldown/rollover, and hunt pitch/cadence bounds. CI runs the native suite with AddressSanitizer and UndefinedBehaviorSanitizer as well.
- Sixteen Node tests cover both image resolutions, RGB565/CRC, crop geometry, serial framing/disconnects/timeouts, acknowledged small transfers, a full 512-row history export received in one USB chunk, compatible 0.3/0.4/0.5/0.6/0.7 USB handshakes/tool availability, radar preference loading and acknowledged saves, and the independently pinned web installer dependency.
- The release packager checks partition sizes, every merged image segment, checksums and workstation-path exclusion. Manifests identify the exact board and distinguish fresh installs from updates.
- Browser QA checked detector/audio settings at mobile and desktop widths, 26 family choices, disabled disconnected controls, display defaults, and absence of JavaScript errors. Badge image selection/cropping was checked in 0.3.1. Hardware USB tests use host scripts implementing the same protocol; browser USB permission-picker and end-to-end upload remain unverified.

## Radar and protocol checks added in 0.7

- All 134 pinned reference facts still pass recognition and radar eligibility checks. New fixtures compare 23 service clues across 16/32/exact-Base-128 representations, including Google Find Hub and Remote ID payload offsets, Snap corroboration, custom UUID exclusions, truncation and advertisement/response boundaries.
- Pagination fixtures cover every contact count from zero to 384. Radius/angle fixtures verify markers stay clear of the header and controls while preserving RSSI radius within pixel rounding. Two thousand layouts verify deterministic unique label slots, display bounds and non-overlapping text boxes. Recent-signal fixtures verify the 30/120-second boundaries, expired evidence, disabled families, watch/ignore behavior and timer rollover. None of these establish physical ranging accuracy.
- The owner confirmed Flipper Zero on the attached Waveshare. The detector also observed an intermittent Pineapple local-address clue; that clue has not been authenticated as the owner's Pager. Private names and addresses are excluded from published images and logs.

## Waveshare 1.75C · 0.7 hardware and browser checks

- All three profiles compile locally. The Waveshare application was flashed at `0x10000` and hash-verified, preserving NVS and files. USB reports `BADGE_INFO 0.7.0 466 READY`; self-test passes 17/17. Spotpear and M5Stack were not physically available.
- Eight consecutive 434,312-byte captures passed CRC32: three themes, radar settings, a real Flipper hunt, two crowded radar pages and the restored candidate radar. Six labeled contacts per page and next/previous controls worked. Visual inspection found one wrapping signal label; the final build uses shorter strength text, checked again after flashing.
- Passive and active BLE reception continued with increasing uptime, no resets, zero queue/storage drops and no malformed frames. At the end of the 111-second final-layout run, free internal heap was 114,780 bytes, free PSRAM 7,933,672 bytes and the lowest loop stack margin 3,228 bytes. These are short desktop checks, not endurance or saturated-radio benchmarks.
- A real restart retained the selected theme and disabled label/recent preferences. Afterwards, green theme, labels on, recent signals on, candidate scope, passive reception and radar were restored. Volume and brightness remain 100%, dim/off timers zero, and the owner's existing quiet mode remains enabled. The speaker shortcut toggles quiet mode.
- The 0.7 flasher was checked at desktop and 390-pixel mobile widths with no horizontal overflow or browser errors. The logo and all three theme images loaded. All three boards loaded matching factory/update manifests; changing boards reset confirmation. Radar preferences were selectable and their save action remained disabled without compatible USB firmware. The browser USB chooser and end-to-end installation remain unverified; physical flashing and USB protocol tests used host tools.

## Traffic and RSSI checks added in 0.6

- The pinned SquachWatch reference was checked against the 134-entry ledger again; all recognition and radar-visibility fixtures still pass.
- Host fixtures exercise interleaved Wi-Fi retries, destination/subtype/sequence reuse, disconnect-window expiry and truncated protected frames. BLE fixtures cover changing addresses, fixed-address pairing-payload churn, identical repeats, normal tag telemetry, sparse traffic, cooldown and timer rollover. A further 50,000 crowded observations check bounded table turnover. No attack traffic was transmitted for these checks.
- RSSI fixtures compare identical signals arriving every 10 ms versus 100 ms, verify monotonic/clamped ring mapping, and exercise missing hunt samples, long gaps and timer rollover. These verify algorithm behavior, not physical ranging accuracy.
- All three final board profiles build locally. The BWM1 emblem is encoded at native sizes for both display resolutions. Only Waveshare was available for physical checks; Spotpear and M5Stack require on-device validation.

## Waveshare 1.75C · 0.6 hardware and browser checks

- The final application was flashed at `0x10000` and hash-verified; NVS, history and badge files were retained. USB reports `BADGE_INFO 0.6.0 466 READY`; the on-board self-test passes 17/17.
- Twelve consecutive 434,312-byte display captures passed CRC32, covering radar, Activity, Targets, a real BLE hunt, Radio health, Settings and filters. Passive and active BLE, two USB reconnects and two hunts completed with increasing uptime, no resets, no queue/storage drops and no malformed frames. Radar free internal heap was roughly 117–118 KB; the lowest observed loop stack margin was 1,948 bytes. This short desktop check does not establish all-day stability.
- The rendered logo, RX/count/battery row, dBm rings, fading markers, Activity text and hunt values were inspected. Sparse hunt reception originally produced no joined line; individual valid samples now render as points, with gaps preserved. The published radar image is an actual LVGL capture without names or addresses, not a photograph or simulated detection.
- `traffic status` was exercised; no deauth or BLE spam bursts were observed in the ambient run. Attack recognition is covered by host fixtures, not an over-the-air field validation. Dedicated hunting follows an observed address and cannot authenticate its transmitter.
- The final check retained volume 100%, brightness/output 100%, dim/off timers zero, existing quiet mode and badge storage. Hunt was stopped and passive reception/radar restored. Quiet mode can be changed with the radar speaker button.
- The local 0.6 flasher was checked at desktop and 390-pixel mobile widths with no horizontal overflow or browser errors. Both PNG assets loaded, all three board selections loaded their matching 0.6 update manifest, board changes reset confirmation, and installation stayed disabled until confirmation/component readiness. Activity stayed disabled without compatible USB firmware. The real browser USB chooser/install path remains unverified; the physical update and USB protocol checks used host tools.

## Waveshare 1.75C · 0.5 reception and resource checks

The update was written at the application offset and hash-verified, preserving NVS and files. Its on-device self-test passes 17/17. The 134-signature suite runs on the host against the same parser/visibility code; it does not broadcast simulated surveillance devices over the air.

- In the same desk environment, 0.4 reported about **76,092 bytes** free internal heap; 0.5 passive reception reported **121,456 bytes**, about 45 KB more, with contact capacity increased from 160 to 384. Board adapters allocate different display resources, so this measurement applies to Waveshare.
- Passive and optional active BLE runs continued receiving Wi-Fi/BLE with **zero queue drops, storage drops or malformed-frame counts**. Active mode stayed around 120–121 KB free heap during the short check. This is not a saturated-radio benchmark or an all-day endurance test.
- The cache served 6,861 hits with 40 misses by 223 seconds; no contact eviction was needed. The highest observed ready-queue occupancy was 21/96. Normal processing sometimes took about 12 ms for one batch, including first-contact history work: the 4 ms budget is a yield target, not a hard real-time limit.
- Radar, Targets, detector filters and Radio health were inspected through 466×466 display captures. The radar uses hollow clues, filled stronger detections and the Targets/quiet shortcuts. In 0.5.0, some diagnostic screenshot transfers were short and one panic reset occurred during capture. Another 42 captures reproduced transfer loss but not the panic, so its precise cause is unknown. Version 0.5.1 replaces the temporary LVGL snapshot display with a copy of the ordinary refresh and adds paced USB writes, a 2 KB transmit queue and a checksum footer. USB screenshot pauses inflate the reported maximum queue age and are not a radio-throughput measurement.
- USB reports `BADGE_INFO 0.5.1 466 READY`. The web tool accepts 0.5 and retains compatible 0.3 badge/0.4 detector support; unknown versions and unavailable storage remain rejected. The actual browser USB permission-picker/upload path is still not verified.
- On 0.5.1, **24 consecutive 434,312-byte captures passed CRC32 verification**, covering Radar, Targets, Radio health, Settings and detector filters, passive/active BLE and four USB reconnects. Uptime increased throughout, with no resets or queue/storage drops. The added USB transmit queue leaves roughly 119 KB free heap on the radar, still about 43 KB above the 0.4 desk baseline. This short check does not establish all-day stability or prove the unidentified earlier panic can never recur.
- The update retained 100% speaker volume, 100% brightness, Never dim/off, stored history and the existing badge filesystem. Active scanning was returned to passive and the screen to radar after checks.

The 0.5 changes have not been physically tested on Spotpear or StopWatch. All three board profiles build; use the preceding hardware checks only for the hardware/functions actually exercised.

## Retained Waveshare 1.75C checks from 0.4 · 2026-10-13

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
