# Changes

## 0.13.19

- Add the experimental non-C Waveshare AMOLED 1.75 port: corrected reset/audio wiring, RTC verification, microSD, full 466 px UI and a separate 16 MB web-flasher profile. Hardware testing is pending.

## 0.13.18 — Can Commander

- Adds Matthew Kukanich's original MK logo, a CAN-inspired paired radar sweep, circuit nodes and navy/silver/blue interface colors.
- Adds a dedicated clock with three time fields, date, hexadecimal clock-data bytes, differential-waveform animation and seconds marker.
- Adds a matching session timer with a one-minute perimeter indicator on all five boards.
- Appends theme 35 and watch 38 without moving saved selections or the optional personal-theme slot.


## 0.13.6 — Complete themes, locksport and community watches

- Adds DeFlock (theme 25, watch 28): supplied camera-raven artwork, navy/steel-blue menus and radar, viewfinder details, large 24-hour time and an optical seconds rim.
- Adds 0ct0’s Pig (theme 24, watch 27), an ASCII octopus radar/interface and a wandering pig with live 24-hour speech-bubble time, gaze changes and tap reactions.
- Adds a shared web-flasher page-visit counter at the top right. Counts begin with this release; previews only read the count.

- Adds DagNazty (theme 23, watch 26): white field, black accents, supplied skull, talon details and unique tapered analog hands on all four full boards.

- Adds Seeed Studio (theme 22, watch 25): original logo, lime/deep teal/white palette, leaf-circuit radar and unique Arabic analog watch across all four full boards.
- Adds an orbiting comet to TOOOL, TOOOL Dark, Covert and DMAC timing screens. Each ten-second lap follows elapsed attempt time; pause/open freezes it, resume continues and reset returns it to the top.

- Removes the white ValleyTech backing plate and uses its transparent silver mark on the radar. DMAC radar/watch/counter use a circular logo without white corners. M5Stack retains its supplied white circular artwork.

- Adds DMAC (theme 21, watch 24): original teal/pink logo, separate acrylic McTickler hands with copper inlays, full GUI colors and a Locksport Counter using START/PAUSE, SPLIT, OPEN and confirmed RESET.
- Adds Nyan Devices with a moving pixel cat/rainbow radar, Talking Sasquach with a portrait-head radar and cartoon character arms as watch hands, and unique Waveshare waveform, M5Stack modular hybrid and LAB5 digital watches. Themes 16–20 map to faces 19–23.
- Removes unbranded Neon analog/digital, Fairytale, Steampunk and Galaxy from swipe/USB selection. Their saved IDs 3–7 fall back to BWM; existing logo IDs stay stable.
- Adds HaleHound (theme 15, face 18), with both supplied logos, a pink/violet hellfire radar and animated flames on the analog watch. The bounded animation uses no per-frame allocation and redraws only two small flame regions between clock ticks.
- Adds GhostStrats with glowing purple slit eyes, faceted grey details and “Safety is an Illusion.”, plus N3rdsec with its original skull, black/white/gold palette and broad gold/silver analog hands. They append themes 13–14 and faces 16–17.
- Adds AWOK (neon paint drips/mushrooms), Baph0Mau5 (violet sigils/moon ornament), and WDGWARS (cyan/magenta world-map and radio motifs), each with original supplied artwork, matching analog watch and full GUI palette. These append theme indices 10–12 and face indices 13–15; previous saved indices stay intact.
- Adds Covert Instruments × McNally: black/white/silver radar and Locksport Timer, original supplied logos, distinct silver Reaper McTickler minute and short-hook hour profiles, and a separate 24-hour digital window.
- Applies each saved theme across menus, settings, dialogs, lists, clock setup, standard stopwatch, BLE Lab and radio tools. Shared palettes enforce readable foregrounds, secondary labels and action buttons. Detection-family colors remain meaningful, with dark contrast plates on light menus.
- Uses the supplied BWM2 logo alone, centered on black with a fade at boot, regardless of the selected theme. No device or theme names appear on the boot screen. Indexed artwork reduces flash use; original supplied source images are retained. The compact vector hands rotate without per-frame framebuffer allocation. Covert radar replaces its line/wedge sweep with the silver McTickler, drawn behind signal dots and labels.

- Adds Church of Malware: a monochrome circuit radar and full-emblem binary watch. White weighted bits around the unobstructed emblem show 12-hour H plus M/S; AM/PM sits with the hour.
- Removes AM/PM from every analog date. Existing digital 12-hour/24-hour behavior is preserved. Twenty-four selectable logo watches and twenty-six complete themes preserve existing valid saved indices.
- Studio verifies each theme preference by reading it back, avoiding dependence on a short USB acknowledgment that some boards delay.

- Adds TOOOL Dark across radar, analog watch, Locksport Timer and split log: near-black surfaces, red accents and soft white details. Its original logo sits in a circular inset; the light TOOOL option remains available.
- Adds a white/red TOOOL radar theme with the supplied logo and pin-tumbler details. Selecting it also selects a new 12-hour analog watch with Arabic numerals, red/dark hands and a machined cylinder motif. Existing theme and watch indices stay unchanged.
- With TOOOL, TOOOL Dark or Covert Instruments × McNally selected, Stopwatch becomes Locksport Timer: START/PAUSE, SPLIT, OPEN to record and stop at one timestamp, and confirmed RESET for the next attempt. The latest 50 splits are available in LOG; elapsed time excludes pauses and continues across page/theme/time changes.
- Touch and existing hardware secondary keys use OPEN in either TOOOL mode. Covert Instruments and DMAC also use OPEN; the remaining themes retain ordinary lap behavior. Results remain in RAM and reset on reboot or confirmed reset.
- Studio recognizes theme indices 6–21 and gates them to 0.13.6+. All four full boards share the theme, watch and timer; Lite remains unchanged.

## 0.13.5 — Clearer ValleyTech dial and larger digital time

- Replaces ValleyTech's cogwheels with a quiet black dial, all twelve Arabic numerals, the original gold VTS logo, and wider beveled gold/silver hands. ValleyTech no longer runs gear animation.
- Both digital faces use crisp native-size time glyphs more than twice the previous size, with repositioned seconds, AM/PM, date and ornaments. JCMK remains the sole 24-hour face.
- Compact digit-only fonts cover all three display sizes and are excluded from Lite. The bundled Montserrat font license accompanies every board package.
- Biscuit, Steampunk and the remaining clock designs retain their existing appearance and behavior.

## 0.13.4 — Themed watchmaking and 12-hour time

- Nine swipeable faces. All analog dials and the neon digital face use 12-hour time; only the JCMK military face uses 24-hour time. Face-name captions are replaced by original logos or drawn emblems.
- ValleyTech gains black/gold/silver sculpted clockwork with engraved rosettes, Biscuit Shop gains dark monochrome rotating gears, and new fairytale, brass steampunk and galaxy faces use detailed analog dials. BWM and the two neon designs remain available.
- Manual time setup uses a 1–12 hour wheel and AM/PM. Midnight/noon conversion is tested for all 24 hours; RTC and UTC storage remain unchanged.
- Animated mechanisms redraw only their changing region between seconds. All four full boards share the renderer; Lite remains radar-only. Existing automatic USB time sync and monotonic stopwatch behavior remain available.

## 0.13.3 — Pocket-watch faces and effortless local time

- Five saved, swipeable clock faces on Waveshare AMOLED, M5Stack StopWatch, Spotpear and XIAO: Atelier, Porcelain, Nocturne, Neon Circuit and Afterhours. The last two use cyberpunk palettes; three faces are analog and two digital. Lite remains radar-only.
- Connecting in Studio automatically sets local date, time and the current timezone offset. A single Sync my local time button refreshes it after travel or daylight saving changes.
- Settings → Clock & date explains automatic setup; manual setup uses date/hour/minute wheels and an advanced timezone editor. Invalid month/day combinations clamp safely. Clock faces hide routine detection banners while listening continues.
- Restyle the stopwatch as a restrained chronograph while preserving monotonic elapsed time, pause/resume, laps and hardware keys.

## 0.13.2 — Hardware-aware storage and clearer installation

- Remove SD menus, settings, command handlers, card prompts and logging/browser allocation from Waveshare AMOLED and M5Stack builds. Skip CSV formatting when SD logging is unavailable or disabled.
- Utilities → Saved files opens internal-flash captures on those boards. All four full boards gain Settings shortcuts for Capture studio and Saved captures. Spotpear and XIAO keep their real SD features; Lite remains radar-only.
- Add build guards against unsupported SD UI strings, and extend live scan-continuity checks to Utilities, saved files and Capture Studio.
- Publish current Waveshare GUI pictures, a GUI tour, hardware feature/storage tables, current update compatibility instructions and board-specific information in the web flasher. Existing partitions, badge and preferences are unchanged.

## 0.13.1 — Branded radar themes

- Added Valleytech (silver/cyan), Biscuit Shop (warm amber) and Marauder (black/red), each with the owner-supplied radar logo, on all four full boards.
- Theme selection is available in Settings → Theme & radar labels and the USB web tools. New choices are gated on older firmware. Existing theme indices, badge, preferences and partitions are preserved.
- Native-resolution logo assets avoid runtime scaling and retain the original source PNGs. Device-family colors, labels, sweep animation and background scanning are unchanged. Lite stays fixed green.
- Web gallery and theme guide show CRC-verified Waveshare renders; attribution records identify the supplied artwork separately from MIT application code.

## 0.13.0 — Four-board recon lab and clearer tools

- Rename Daily to Utilities, remove the on-device Badge editor shortcut, and put Attacks in the first Recon row with separate Wi-Fi and BLE lists. Keep browser Badge Studio.
- Bring eight modes, 29 timed radio tools, GATT, portal, connected-network and CSI workflows to Waveshare, M5Stack, Spotpear and XIAO. Scale controls and RAM captures to each board; Lite stays radar-only.
- Add persistent recon sessions with checked RCN1/PRB1/REL1 ZIP export, historical AP archives, volatile source-MAC controls and bounded reviewed recipes. Home cancels recipes and restores their previous radio mode.
- Add independent HID field/usage decoding and a cached-read GATT bridge for explicit owned-device sessions. Register the GATT server while disconnected before reconnecting the original peer.
- Compress the complete 40,333-prefix registry with a bounded CRC-checked decoder into PSRAM, preserving flash reserve. Keep all existing partitions, badges and preferences.
- Extend the web lab to all four full boards with version-aware controls, session/survey downloads, recipes, HID and bridge tools. Document actual limits and attribution.

## 0.12.0 — Continuous recon and shared notebook

- Keep both radios scanning through Hunt; visit every Wi-Fi channel despite loop jitter, then give the target extra dwell. Replace ordinary Pause controls with radio health.
- Continue receiving during badge upload, screenshots, history export and SD USB file transfers; keep transfer streams free of unsolicited alert text.
- Add a passive notebook to Waveshare, M5Stack, Spotpear and XIAO: 96 probe pairs, 96 complete BLE payloads, protocol metadata/counters, CSV snapshots and on-screen inspection.
- Add 40,333 factual IEEE MA-L prefixes in a deduplicated flash-only directory; reject randomized/local-address manufacturer claims.
- Expand Waveshare's 24 tools to 29 with iBeacon, Eddystone UID/telemetry, BTHome and a checked custom legacy advertising payload.
- Enforce at least 21% free application-slot capacity in packaged builds and reserve 21% of writable flash stores for future writes. Preserve partition offsets and existing badge/settings.
- Fix manifest-version gates for 0.11 and 0.12 factory/update images across all boards. Add public memory budgets and targeted scan-continuity checks.


## 0.11.1 — Connected-network diagnostics

- Add eight explicitly selected Wi-Fi profiles, DHCP/IP/gateway/DNS display, gateway ping, bounded local-subnet scans and CSV capture-library export on the Waveshare AMOLED.
- Restore radar on Home, Disconnect, failed join, connection loss and session expiry; keep network credentials out of responses and disable automatic connection.
- Add native controls and version-gated web controls. Retain the 0.11 detection/BLE lab, original badge, display settings and both filesystem layouts.
- Validate ICMP response identity/checksums/nonce, subnet bounds and 25,000 malformed packets, alongside the existing detector and web suites.


## 0.11.0 — Detection audit and Waveshare recon expansion

- Audit all 158 current SquachWatch table facts; add BWCDEVICE/BodyWorn, WatchGuard/Digital Ally, ShotSpotter, Chipolo, newer Tile, Vuzix and vendor clues. Remove generic ESP32-as-camera, Flipper-company-only and Meta-Quest-as-glasses false positives.
- BLE Lab: central writes with/without response, unsubscribe and active subscriptions, MTU control, explicit pairing/passkey/numeric comparison, bond controls, friendly UUID/protocol details, HID report-layout parser, profile deletion, timed legacy advertisement/address replay and supported tracker sound requests.
- Wi-Fi analysis: raw-SSID probe inventory, EAPOL message/PMKID/SAE counters, eight capture filters, selected-AP scope and configurable channel plans. Stopping a capture restores the previous listening mode.
- Timed local portal with captive DNS, optional checked HTML template, explicit probe-SSID selection and bounded flash-saveable submission logs.
- Experimental passive CSI signal monitor: fixed LLTF layout, 30-second calibration, sample-rate/quality checks, bounded queue/history and Stop/Home cleanup.
- Desktop controls, round-screen menus, parser/transport tests, provenance ledger and explicit feature comparison. Flash layout and saved badge/settings are unchanged.

## 0.10.0 — Waveshare modes and radio toolkit

- Eight persistent listening modes with live switching, focused radar/audio and dedicated passive surveys.
- AP/client inventory, 24 bounded Wi-Fi/BLE transmission tools, freshness checks, explicit review, timeout and Stop/Home cleanup.
- 2 MB PSRAM capture studio; Radiotap Wi-Fi PCAP and raw BLE advertisement CSV; atomic flash saves and checked USB downloads.
- Persistent SSID playlists, local-source/CSA controls and on-device capture deletion. Menu allocations use PSRAM; advertised AP channels remain stable during adjacent-channel reception.
- Modes/toolkit touch screens and desktop controls. Existing GATT lab, badges, settings and storage layout preserved.
- Native packet/mode/capture tests and web protocol/0.10 compatibility checks. Hardware results and unavailable upstream features are explicit in the guides.


## 0.9.8 — Waveshare BLE Lab and 32 MB storage

- Original MIT BLE Lab: passive advertisement inspector, explicit GATT discovery/read, notification/indication monitoring, local event logs, saved DDG1 profiles, USB import/export and structural/value comparison.
- Local GATT peripheral emulation with readable/writable characteristics, notifications and descriptors; captured legacy advertisements when they fit. Stack-owned GAP/GATT/CCCD attributes are managed by NimBLE. Bond keys, protected access and proprietary behavior are not cloned.
- Waveshare AMOLED 1.75C now addresses 32 MB. The lower 16 MB partitions are unchanged; a separate 16 MB recon filesystem holds profiles and event logs. Update manifests exclude both filesystems and NVS. Fresh-install manifests initialize both.
- Touch Tools → Recon → BLE Lab and matching USB browser controls. Home/Stop releases the connection/emulator and restores the previous radar state. Other boards remain on their existing 8/16 MB layouts.
- Includes the 0.9.6 detection/radar and 0.9.7 badge improvements below.


## 0.9.7 — Badge upload repair and BWM stock badge

- Serialize Bluetooth scanner changes on its host task. Reproduced and fixed a NimBLE scan-result double-free when USB badge upload paused reception on the Waveshare AMOLED.
- Repeat cumulative USB acknowledgments during an idle transfer to recover Windows native USB stalls without resending image bytes. Keep checksum validation, the inactivity timeout, and atomic replacement of the previous badge.
- Include the centered BWM stock badge in full-board firmware and the studio. Size the visible silhouette for the round screen; preserve existing custom badges and the original source PNG.
- Add optional three-line text with size, vertical position, color and an outline, rendered into both the device upload and PNG download. A failed automatic clock sync no longer disconnects the badge editor.
- Accept the 0.9.6 single-screen radar-status response during studio connection, while retaining compatibility with older paged firmware. Retry stalled read-only USB settings queries.

## 0.9.6 — Axon priority and one-screen radar

- Route all AXEOFF company/service/address matches, including custom UUID and random-address matches, through the reserved Axon receive queue and first-hit alerts. Include SquachWatch Axon name/address signatures in that path. Preserve the evidence specificity in details.
- Use a full 100/100 ms passive BLE window and remove the active-scan result cap. Protect live priority contacts from lower-priority insertions under tracker crowds.
- Remove radar pagination on every board. Plot every retained visible contact, label Axon first and then cameras/ALPR/glasses/drones/Flipper. Keep trackers as small, silent dots; touch reveals their label/leader without displacing Axon.
- Use compact family-colored notification bubbles, a bounded priority queue, and Axon sound preemption. Trackers only alert when explicitly watched. Keep ignore, snooze, quiet, family switches and cooldowns.
- Add regression coverage for AXEOFF matching, first-hit alerts, 384-contact tracker crowds, tap selection, notification ordering and sound ordering.

## 0.9.5 — Refined Lite radar

- Increase Lite device names from 8 to 12 pixels, with slim two-line callouts and four targets per automatically cycling page. Enlarge radio status, simplify the footer and strengthen label-position stability.
- Replace the dense grid and thick bezel with fine range rings, segmented arcs, edge ticks and a center reticle. Cache the static artwork in 115 KB of PSRAM; raise the animation cap from eight to twelve updates per second.
- Keep the full boards' six-target layout, shared detection and device colors unchanged. Extend native checks to cover every contact across four-target pages and collision-free larger labels inside the round panel.

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
