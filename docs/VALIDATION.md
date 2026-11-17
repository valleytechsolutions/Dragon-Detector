# Release verification

Results apply to the version and hardware named in each section. Earlier
release records are not acceptance results for the current firmware.

## 0.14.8 · current release checks

Checked on November 17, 2026:

- **180 JavaScript tests and 30 native host suites passed.** These include the
  production node-session code and its USB command gates, simulated Biscuit,
  JCMK and Piglet links, role/channel assignment, retries, start/stop, loss and
  malformed-input handling.
- **84 node-screen cases across seven panel/input configurations passed with
  no layout violations.** Production Dial/Lite rendering and Dial menu checks
  also passed, covering node exit confirmation, refusals, resume and radio
  hand-back. Host input checks do not establish physical button acceptance.
- Core and SD-label suites passed with **UndefinedBehaviorSanitizer** on Windows
  Zig 0.14.1; an intentional signed overflow trapped. **AddressSanitizer was not
  active:** a heap-overflow positive control went undetected. ASan coverage
  therefore remains unverified on this host.
- All **14 board profiles compiled and fit their application slots**.

Hardware results for the current release candidate:

- **M5Stack StopWatch C152:** the packaged factory image was flashed and its
  write hash verified; 23 self-tests passed. Synchronized RTC time
  survived a software restart. The JCMK Node screen runs, but no real JCMK core
  has been tested. After Biscuit Manager enrollment, the C152 linked, received
  its own Wi-Fi channel list and increased its scan and sent counters without
  observed send failures, drops, scan errors or link losses. The final wardrive
  stop returned both boards to waiting with fixed scan and sent counters.
- **Waveshare AMOLED 1.75C:** the current release application was flashed and
  its write hash verified. A real Biscuit assigned Wi-Fi scanning; records
  continued with no observed send failures, drops, scan errors or link losses.
  The next status read after its stop message (`no_role type=8`) showed the node
  waiting, with scan and sent counters then fixed.
  Starting a new wardrive while it waited reassigned channels and resumed
  scanning and record transmission. The 1.75C took longer on the second stop
  before returning to waiting; no immediate-stop timing is claimed for that
  cycle. All 23 startup self-tests passed and synchronized clock status was valid.

The earlier development-build radar timings, observation-drop comparisons and
longer Biscuit listens are **supplied handoff evidence**, not measurements
repeated on these release binaries. Their dates, values and limits are retained
in the [release notes](RELEASE-0.14.8.md#earlier-development-measurements-supplied-handoff-evidence).
Real JCMK/Piglet communication, the remaining boards' node controls, room motion
and the nearby-BLE count remain field-validation work.

## Elecrow CrowPanel 2.1 Rotary · initial port record (historical)

The 480 × 480 profile compiles with the pinned Arduino_GFX driver. Native tests cover clockwise/counterclockwise detents, switch bounce, missing edges, short presses, holds and timestamp rollover. Rotation tests now cover every pixel at 480 × 480. The 100 browser-module tests include the new board's identity checks, flash layouts, full-size badge encoding, personal-theme installation and software-clock verification.

Physical display, touch, rotary and startup testing is **pending**. The unit has not enumerated over USB, and no firmware has been written to it. See the [port guide and hardware checklist](ELECROW-ROTARY-21.md).

## 0.13.20 · Find My Beeper

- The owner's AirTag was isolated by a battery-out/battery-in scan comparison. On Waveshare 1.75C, one direct sound write was acknowledged and the owner confirmed the audible beep. The BLE worker disconnected and returned to radar, with the stored badge unchanged.
- Controlled Windows peripherals verified the AirTag opcode, FMNA indication subscription before the write, and DULT acceptance/rejection responses. Windows peripheral reconnections were intermittent; DULT completion parsing is covered by native fixtures rather than a physical DULT accessory claim.
- Native checks cover UUID parsing, iBeacon wire encoding/decoding, malformed lengths, signed calibration values and DULT response distinctions, alongside the existing detector, packet-builder and fuzz suites.
- A separate BLE receiver observed the configured iBeacon UUID, major/minor and signed measured power; the timed run stopped and restored scanning.
- 96 web-module tests pass, including board/version gating, explicit address selection and iBeacon input validation.
- All six profiles compile. The non-C AMOLED 1.75, XIAO Round and Lite remain build-tested for this release. The real AirTag result applies to the tested 1.75C and AirTag combination.

The web update preserves NVS and filesystems. Active BLE tests pause scanning and release the radio when finished. No claim is made that every pairing advertisement produces a popup or crash on current phones. [Feature instructions](BLE-TOOLS-01320.md).

## 0.13.17

[Release and verification files](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases/tag/v0.13.17)

Historical GitHub evidence: passing build/test run `39159301105` and successful deployment `39159542534`. These run records are temporarily unavailable publicly; release verification files are linked above.

Firmware and web assets were built from `eb83437ad23f37851ee55089d0d0cfe8590d10ab`.

## 0.13.17 automated checks

- 89 browser-module tests, including custom format round trips, malformed input, upload/readback checksums, board identity checks and cancelled-install handling.
- Native and sanitizer suites, 150,000 detector fuzz inputs and 10,000 personal-theme header fuzz inputs.
- 168 compressed logo variants decoded against their original pixel CRC32.
- All five board builds and application-size checks. The release includes each profile’s memory budget.
- Gitleaks scans tracked source and Git history. This credential scan is not a comprehensive vulnerability assessment.

## 0.13.17 connected hardware

AMOLED 1.75C, M5Stack StopWatch C152 and Spotpear are checked over USB. Tests cover application write verification, 23/23 self-tests, current time and offset, software restart, personal-theme upload/readback and hidden-slot behavior. StopWatch also receives hardware RTC readback and ticking checks. The AMOLED transfer tests include a wrong checksum and incomplete upload, both preserving the preceding theme.

Clock, timer, lap history and Time/Date/Sync settings pause scanning while time advances. Timer controls and resumed Wi-Fi/BLE reception are checked. DZ_AZ and personal-theme screenshots are actual firmware framebuffer captures. The Custom theme editor is exercised in a browser for import/export, preview controls, tab navigation and opt-in state.

Run-specific results, released binary hashes and live-flasher checks are recorded in `RELEASE-VERIFICATION.json`. Device updates preserve partitions, saved files and preferences. Test-created personal designs are removed before restoring the original theme, watch and daily screen.

The live web flasher's five firmware packages and new web assets match the release builds. All 13 GitHub release attachments were downloaded using the authenticated GitHub CLI and matched both the local package hashes and GitHub's SHA-256 digests.

## 0.13.17 limits

- XIAO and Lite are build/package-tested; Lite hardware tests remain paused.
- The real web transfer and clock modules run through a local serial-stream adapter for hardware tests. A complete browser USB chooser/firmware-flash session is not hardware-tested.
- No battery-life or current-consumption percentage is claimed.
- AMOLED 1.75C and Spotpear require USB or configured Wi-Fi time recovery after full power loss. Wi-Fi setup remains deferred; live NTP recovery is not tested here.
- The StopWatch factory package is assembled from clean release segments. Factory installation through M5Burner itself and catalog publication are not verified. [M5Burner status](M5BURNER.md).
- Signed-out GitHub requests returned HTTP 404 for the release, repository and owner profile during final verification on October 18, 2026. Public GitHub download access could not be confirmed; the live web flasher and its firmware downloads were reachable and verified.

Earlier evidence remains attached to its [release](https://git.churchofmalware.org/Valleytech/Dragon-Detector/releases).

## 0.13.19 non-C AMOLED 1.75

The `waveshare_175` port is prepared from the official schematic and maintained BSP. Automated pin/partition/manifest checks and compilation are separate from physical validation. No physical unit has been tested for this port. Display, touch, RTC retention, buttons, SD, audio and battery operation remain on the [field checklist](WAVESHARE-AMOLED-175.md#new-feature-field-checklist).


## Seeed SenseCAP Watcher beta

The `watcher` environment targets 412×412, 32 MB flash and 8 MB PSRAM with full-board software, touch/rotary controls, RTC, speaker and FAT32 SD. Optional UART AT6668 GPS support uses expansion GPIO19/20, not the shared I²C Grove socket. Camera, microphone recording and Himax AI are not enabled. [Setup, wiring and current physical validation](SEEED-WATCHER.md).

## BLE radios near · no physical measurement recorded

The near count is covered by native tests and a host simulation of the radio schedule only. No board has been checked. Bench check to run on each profile before relying on it:

1. Put one known phone with Bluetooth on beside the board and keep other radios away or note them. Leave the board on Radar.
2. Send USB `radar near` once a minute for 30 minutes and keep the lines. They contain counts only, no addresses.
3. Expected: `state=ready` after the first seconds; `near` settles and `steady` follows after about five minutes; `new_addresses` stays flat while `handovers` rises about twice, once per address change and about 40 seconds after it; `gone` and `dropped` stay flat. This holds only with at most five radios near; with more, address changes are deliberately not joined.
4. Move the phone away: `near` should fall within about 40 seconds. Bring it back within five minutes: `returns` rises and `new_addresses` does not.
5. Record where `near` starts and stops counting the phone as it is carried away. That is the only evidence for or against the -70 dBm default on that board.

A rising `new_addresses` and `gone` with a flat `handovers` means address changes are not being joined on that phone: the count overstates by one for about 40 seconds and `steady` drops for about five minutes at each change. Repeat with the phone resting where it reads between the level and 12 dB below it, and after a 20 to 50 second pause (open Clock, then return): `state` should read `warming` for about 20 seconds and `near` should return without `new_addresses` rising.

## Waveshare LCD 1.85B · 0.14.1

The new dedicated port has a separate [release validation record](RELEASE-0.14.1.md) and [hardware guide](WAVESHARE-LCD-185B.md). Do not infer 1.85B physical results from the similarly sized Spotpear or AMOLED models.

## Waveshare AMOLED 1.43C · 0.14.7

The new port has a separate [release validation record](RELEASE-0.14.7.md) and [hardware guide](WAVESHARE-AMOLED-143C.md). Do not infer 1.43C physical results from the 1.75C, which shares only the panel family.

## BISCUIT NODE and JCMK Node

Current-release results are recorded above: both the Waveshare 1.75C and
StopWatch C152 have linked to a real Biscuit, received Wi-Fi channel assignments
and sent records. The 1.75C has also completed a real stop-to-wait and
start-from-wait cycle. Earlier longer listens and the previous fallback stop
test remain supplied handoff evidence in the release notes. JCMK and Piglet
communication remains tested with simulations, not real cores.

The node link runs against simulated JCMK, Biscuit and Piglet cores in the native tests (`tests/node_link_test.h`: wire format, handshakes and their retries, channel plans, the JCMK core's node table, channel split and external-node check, repeats, keyed links, delivery, link loss, clock rollover and random frames). The simulated cores follow the published JCMK and Piglet sources: the Piglet core cannot decrypt and logs every record as Wi-Fi. `tests/node_mode_test.cpp` runs the session code itself: the USB commands and the list of commands allowed while a node runs, encryption and its keys, the remembered node and the restart guard. `tools/render_node_screen.py` draws the node screen at every panel size, checks the wording of each link state, and reports anything that would spill outside the round glass at any drift step. `tools/render_m5dial_ui.py` drives the Dial and Lite buttons: taps and quick double presses that must not leave, holds that do, and the confirm gap and windows. The checklist below includes complete scenarios still to repeat or extend across boards; a partial result above does not mark the whole scenario complete ([node guide](NODE-MODES.md)):

- [ ] On a touch board, a board with a knob (CrowPanel 2.1 or Watcher), M5Stack Dial and Waveshare Lite: start both modes from Modes (on Lite, the MODES page). The node screen fills the panel, the logo is centred and no text is cut off.
- [ ] Biscuit in Extended mode, no wardrive running: WAITING FOR WARDRIVE, and `scans=` in `node status` stays put. A Wi-Fi wardrive started in the app: LINKED TO BISCUIT and scanning; the Biscuit Manager lists the node once, under `DragonDetector:<version>` (or the `node identity` name); `node status` shows `registered=1`, its role and channel list; records reach the Biscuit's log with its position.
- [ ] The wardrive stopped in the app: the node shows WAITING FOR WARDRIVE again at once and `scans=` stops rising; the USB log shows `NODE_EVENT no_role type=8` (the Biscuit's stop message). Without `type=8`, the stop message was missed and two rounds without a role ended it, within about two minutes.
- [ ] Biscuit left alone for several minutes while wardriving: the node stays LINKED TO BISCUIT throughout (no flicker back to waiting), and the Biscuit Manager does not list it twice. Biscuit restarted while linked: the node joins it again without being restarted, within about 90 seconds, and networks found afterwards reach its log.
- [ ] Biscuit in Secure mode: SET BISCUIT TO EXTENDED MODE and no records. Switched to Extended mode, it is joined without restarting the node.
- [ ] JustCallMeKoko ESP32 Dual Band Wardriver core without encryption: LINKED TO CORE; the core's Nodes menu lists the node as 6G EXT; `node status` shows `assigned=0`, `node=0/0` and `plan=1,2,3,4,5,6,7,8,9,10,11`; the core's other nodes keep their full channel shares; Wi-Fi and Bluetooth records in the core's log; `node ble off` stops the Bluetooth records.
- [ ] The same core with encryption: a matching `node key` works; with Use Encryption ticked and no ENOW Key, `node key ""` works, and so does `node encrypt on` after `node key clear`; a wrong key does not deliver records; `node config` shows only `keyed=1`, and `node status` shows `keyed=1` while the keyed JCMK Node runs.
- [ ] Piglet acting as a JCMK-style core: LINKED TO PIGLET CORE, Wi-Fi records arrive, no BLE count on the screen and no Bluetooth rows in the Piglet's log. With encryption on: PIGLET CORE: CLEAR NODE KEY and no link; `node encrypt off` then joins it.
- [ ] Wrong mode: JCMK Node next to a Biscuit, in either mode, shows BISCUIT FOUND: USE BISCUIT NODE; BISCUIT NODE next to a JCMK core shows JCMK CORE FOUND: USE JCMK Node.
- [ ] Two cores in range: `node bind <address>` joins only the bound core.
- [ ] Core switched off: SEARCHING within about 30 seconds. Switched on again: linked, and networks are reported again.
- [ ] Leaving: tap EXIT, then CONFIRM half a second to 3 seconds later; a quick double tap does not leave; a single tap goes back to EXIT after 3 seconds. Short presses and quick double presses of BOOT, PWR, Home, Back and the Dial and Lite buttons never leave and show TAP EXIT TO LEAVE (HOLD KNOB TO EXIT with a knob, HOLD BUTTON TO EXIT on Lite) for 3 seconds. Knob clicks on the CrowPanel 2.1 and Watcher never leave either. A hold counts as one exit request: BOOT 2 s on the 1.75C (counted when released), BOOT 0.7 s on the 1.85B and 1.43C, the knob hold on the CrowPanel 2.1 and Watcher, the Dial and Lite button hold; then CONFIRM, or a second hold within 5 seconds. A burst of quick taps on EXIT does not leave. USB `node stop`. Afterwards Modes is shown, and detection, alerts and audio work again.
- [ ] While a node runs, alerts, notifications and the clock setup prompt do not cover the node screen. USB Studio connects, reads the settings and can set the clock; other changes, uploads and downloads stop at once with the wardriving-node message (the firmware answers `NODE_BUSY`).
- [ ] Refusals: with the stopwatch running, Stop the stopwatch first is shown and the device stays on Modes. During a GPS wardrive or a wardrive upload, the device stays on the Wardrive or upload page, Modes cannot be opened, and USB `node biscuit` answers `WARDRIVE_BUSY` or `UPLOAD_BUSY`. A capture recording to SD is finished on the card when a node starts.
- [ ] Power unplugged and USB `system restart` while a node runs: `NODE_RESUMED` and the node screen at boot.
- [ ] A supply that browns out within a minute of each resume: the first one or two restarts resume, the next prints `NODE_RESUME_SKIPPED ... check the battery or USB supply` and no node starts.
- [ ] A one-hour drive on each board, including M5Stack Dial: `heap` and `min_heap` in `node status` stay steady, `rx_drops` and `failed` stay low, and the screen count is plausible against the core's log.
- [ ] AMOLED boards: the name, logo, counts and EXIT shift a few pixels each minute, and the rim stays centred.
