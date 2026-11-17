# Release verification

## Elecrow CrowPanel 2.1 Rotary · experimental

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

## Automated checks

- 89 browser-module tests, including custom format round trips, malformed input, upload/readback checksums, board identity checks and cancelled-install handling.
- Native and sanitizer suites, 150,000 detector fuzz inputs and 10,000 personal-theme header fuzz inputs.
- 168 compressed logo variants decoded against their original pixel CRC32.
- All five board builds and application-size checks. The release includes each profile’s memory budget.
- Gitleaks scans tracked source and Git history. This credential scan is not a comprehensive vulnerability assessment.

## Connected hardware

AMOLED 1.75C, M5Stack StopWatch C152 and Spotpear are checked over USB. Tests cover application write verification, 23/23 self-tests, current time and offset, software restart, personal-theme upload/readback and hidden-slot behavior. StopWatch also receives hardware RTC readback and ticking checks. The AMOLED transfer tests include a wrong checksum and incomplete upload, both preserving the preceding theme.

Clock, timer, lap history and Time/Date/Sync settings pause scanning while time advances. Timer controls and resumed Wi-Fi/BLE reception are checked. DZ_AZ and personal-theme screenshots are actual firmware framebuffer captures. The Custom theme editor is exercised in a browser for import/export, preview controls, tab navigation and opt-in state.

Run-specific results, released binary hashes and live-flasher checks are recorded in `RELEASE-VERIFICATION.json`. Device updates preserve partitions, saved files and preferences. Test-created personal designs are removed before restoring the original theme, watch and daily screen.

The live web flasher's five firmware packages and new web assets match the release builds. All 13 GitHub release attachments were downloaded using the authenticated GitHub CLI and matched both the local package hashes and GitHub's SHA-256 digests.

## Limits

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

## BLE radios near · unreleased, not yet measured

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

Unreleased, not yet verified on hardware. The node link runs against simulated JCMK, Biscuit and Piglet cores in the native tests (`tests/node_link_test.h`: wire format, handshakes and their retries, channel plans, the JCMK core's node table, channel split and external-node check, repeats, keyed links, delivery, link loss, clock rollover and random frames). The simulated cores follow the published JCMK and Piglet sources: the Piglet core cannot decrypt and logs every record as Wi-Fi. `tests/node_mode_test.cpp` runs the session code itself: the USB commands and the list of commands allowed while a node runs, encryption and its keys, the remembered node and the restart guard. `tools/render_node_screen.py` draws the node screen at every panel size, checks the wording of each link state, and reports anything that would spill outside the round glass at any drift step. `tools/render_m5dial_ui.py` drives the Dial and Lite buttons, including the Dial's quick double press. No real core and no board has been tested. Checks to run before relying on it ([node guide](NODE-MODES.md)):

- [ ] On a touch board, a board with a knob (CrowPanel 2.1 or Watcher), M5Stack Dial and Waveshare Lite: start both modes from Modes (on Lite, the MODES page). The node screen fills the panel, the logo is centred and no text is cut off.
- [ ] Biscuit in Extended mode: JOINING BISCUIT, then LINKED TO BISCUIT; the Biscuit Manager lists the node once, under `DragonDetector:<version>` (or the `node identity` name); `node status` shows `registered=1`, its role and channel list; records reach the Biscuit's log with its position.
- [ ] Biscuit left alone for several minutes while linked: the node stays LINKED TO BISCUIT throughout (no JOINING BISCUIT flash every 10 seconds), and the Biscuit Manager does not list it twice. Biscuit restarted while linked: the node joins it again without being restarted.
- [ ] Biscuit in Secure mode: SET BISCUIT TO EXTENDED MODE and no records. Switched to Extended mode, it is joined without restarting the node.
- [ ] JustCallMeKoko ESP32 Dual Band Wardriver core without encryption: LINKED TO CORE; the core's Nodes menu lists the node as 6G EXT; `node status` shows `assigned=0`, `node=0/0` and `plan=1,2,3,4,5,6,7,8,9,10,11`; the core's other nodes keep their full channel shares; Wi-Fi and Bluetooth records in the core's log; `node ble off` stops the Bluetooth records.
- [ ] The same core with encryption: a matching `node key` works; with Use Encryption ticked and no ENOW Key, `node key ""` works, and so does `node encrypt on` after `node key clear`; a wrong key does not deliver records; `node config` shows only `keyed=1`, and `node status` shows `keyed=1` while the keyed JCMK Node runs.
- [ ] Piglet acting as a JCMK-style core: LINKED TO PIGLET CORE, Wi-Fi records arrive, no BLE count on the screen and no Bluetooth rows in the Piglet's log. With encryption on: PIGLET CORE: CLEAR NODE KEY and no link; `node encrypt off` then joins it.
- [ ] Wrong mode: JCMK Node next to a Biscuit, in either mode, shows BISCUIT FOUND: USE BISCUIT NODE; BISCUIT NODE next to a JCMK core shows JCMK CORE FOUND: USE JCMK Node.
- [ ] Two cores in range: `node bind <address>` joins only the bound core.
- [ ] Core switched off: SEARCHING within about 30 seconds. Switched on again: linked, and networks are reported again.
- [ ] Leaving: tap EXIT, then CONFIRM; a single press goes back to EXIT after 3 seconds; Home, Back, BOOT, the knob and a long press never leave on one press; the Dial front button, twice or as a quick double press; Lite Hold twice; USB `node stop`. The first press shows TAP CONFIRM TO LEAVE on touch boards and the Dial, PRESS CONFIRM TO LEAVE on CrowPanel 2.1 and Watcher, HOLD AGAIN TO LEAVE on Lite. Afterwards Modes is shown, and detection, alerts and audio work again.
- [ ] While a node runs, alerts, notifications and the clock setup prompt do not cover the node screen. USB Studio connects and reads the settings; changes answer `NODE_BUSY`.
- [ ] Refusals: with the stopwatch running, Stop the stopwatch first is shown and the device stays on Modes. During a GPS wardrive or a wardrive upload, the device stays on the Wardrive or upload page, Modes cannot be opened, and USB `node biscuit` answers `WARDRIVE_BUSY` or `UPLOAD_BUSY`. A capture recording to SD is finished on the card when a node starts.
- [ ] Power unplugged and USB `system restart` while a node runs: `NODE_RESUMED` and the node screen at boot.
- [ ] A supply that browns out within a minute of each resume: the first one or two restarts resume, the next prints `NODE_RESUME_SKIPPED ... check the battery or USB supply` and no node starts.
- [ ] A one-hour drive on each board, including M5Stack Dial: `heap` and `min_heap` in `node status` stay steady, `rx_drops` and `failed` stay low, and the screen count is plausible against the core's log.
- [ ] AMOLED boards: the name, logo, counts and EXIT shift a few pixels each minute, and the rim stays centred.
