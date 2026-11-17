# BISCUIT NODE and JCMK Node

**BISCUIT NODE** and **JCMK Node** turn a Dragon Detector into a scan-only wardriving node for a separate wardriving core. The node scans Wi-Fi (and, for JCMK Node, Bluetooth LE) and sends each sighting to the core over ESP-NOW. The core adds the time and its own GPS position and keeps the log. While a node runs, the Dragon Detector does nothing else.

Both modes are on every board profile: the full-UI boards, Waveshare Lite and M5Stack Dial.

**Status: not yet verified on hardware.** The link has native tests against simulated JCMK, Biscuit and Piglet cores, and the screen has a host render check at every panel size. No real core has been tested with it yet. See the [hardware checklist](#hardware-checklist-not-yet-verified).

## Which mode for which core

| Mode | Joins | Sends to the core |
|---|---|---|
| **BISCUIT NODE** | A Biscuit core (Biscuit Manager) set to **Extended** mode | Wi-Fi networks |
| **JCMK Node** | A JustCallMeKoko **ESP32 Dual Band Wardriver** core, or a Piglet acting as a JCMK-style core | Wi-Fi networks and Bluetooth LE devices |

- A Biscuit in Secure (encrypted) mode cannot be joined. Switch it to Extended mode.
- BISCUIT NODE never joins a JCMK core, and JCMK Node never joins a Biscuit. If the wrong kind of core answers, the screen tells you which mode to use.
- A Biscuit receives Wi-Fi only. A JCMK core receives Wi-Fi and Bluetooth. Bluetooth reports can be switched off with `node ble off`.
- A Piglet marks its answer, so the screen shows **LINKED TO PIGLET CORE**. Records then use the JCMK wording.

Set up the core for ESP-NOW nodes as its own documentation describes. The Dragon Detector needs no pairing step: it finds the core by itself.

## Start a node

- **Full boards:** Tools → Modes → **BISCUIT NODE** or **JCMK Node**.
- **M5Stack Dial:** Menu → Tools → Modes, then BISCUIT NODE or JCMK Node.
- **Waveshare Lite:** Hold BOOT through the pages to **MODES**, Tap to pick a node, then Double tap to start it.
- **USB:** `node biscuit` or `node jcmk`.

The node screen replaces everything else. If the node cannot start, the reason is shown and the device stays on Modes. See [start refusals](#start-refusals).

Starting a node pauses the radar, detectors, alerts, notifications, audio, history writing and the badge. It ends running radio tools and tests, captures, the local portal, Room motion, a joined Wi-Fi network, recon sessions and recipes, and it switches off the training fox beacon, which stays off afterwards. A few things are never cut off: the node refuses to start while the stopwatch runs, during a GPS wardrive or a wardrive upload, and while a recon save is still being written. An open BLE Lab session is closed, and you are asked to try again.

## The node screen

The screen is black with white artwork on every theme, so it reads the same on LCD and AMOLED panels. From top to bottom:

- The mode name: **BISCUIT NODE** or **JCMK Node**.
- The link state in a few words: green when linked, amber while searching, red when something needs your attention. Small panels use shorter wording. The meanings are listed under [troubleshooting](#troubleshooting).
- The **Biscuit Shop** or **JCMK** logo in the middle.
- The number of **WI-FI NETWORKS** found, in large digits.
- A line of smaller counts: **BLE** (JCMK Node with Bluetooth reports on), **SENT**, and **CH**, the channel the radio is on. Where the line does not fit, the channel is left out first.
- **EXIT** at the bottom. Waveshare Lite shows HOLD BUTTON TO EXIT instead.

An arc on the rim marks the channel being scanned within the current channel plan. While the node searches it turns slowly. Everything shifts a few pixels each minute to spare AMOLED panels.

Alerts, notifications, the clock setup prompt and other pages do not open over the node screen.

### What the counts mean

- **WI-FI NETWORKS** is the number of different access points (BSSIDs) heard since this node started. A router that broadcasts several networks counts once for each. Hearing a network again does not add to it.
- **BLE** is the number of different Bluetooth LE addresses. A phone that changes its address counts again.
- **SENT** counts records the core's radio confirmed, repeats included, so it can be larger than the other two counts together.
- **CH** is the channel being scanned, or 6 while the node talks to the core.

The counts start from zero each time the node starts, including after a restart or a settings change. Networks are only counted while the node is linked: without a core the node does not scan at all.

The M5Stack Dial has no PSRAM and remembers about 2,000 networks and devices. Once that is full, older entries are replaced, so a network heard again much later can be counted and sent again. The other boards remember 32,768.

## Leave a node

Leaving takes two presses, so a bump cannot end a drive.

- **Touch:** tap **EXIT**. It turns red and reads **CONFIRM**. Tap **CONFIRM** within 3 seconds. If you do not, it goes back to EXIT and the node keeps running.
- **Buttons and knobs:** press twice. The first press arms the exit and the status line asks for a second press; a second press within 3 seconds stops the node. On full boards, keys that normally leave a page (Home, Back, BOOT, the knob, a long press) count as one press here. On the M5Stack Dial, the front button counts as a press on EXIT. On Waveshare Lite, Hold twice leaves the node.
- **USB:** `node stop`.

On full boards, a touch or press that only wakes a dark display does not count as an exit press.

After leaving, the device returns to Modes. The radios go back to the radar as they were before the node started, and the node choice is forgotten.

## How a node works

1. **Search.** The node waits on Wi-Fi channel 6, the home channel every core listens on, and broadcasts a request: the first within 0.3 seconds, then after 0.3, 0.6, 1.2, 2.4 and 4.8 seconds, then every 5 seconds. Nodes powered on together do not all call at the same moment.
2. **Join.** The first core of the right kind that answers is linked. A JCMK core may assign a range of channels and tell the node its number among the core's nodes. A Biscuit receives the node's name (`DragonDetector:<version>` by default), assigns a role and sends a channel list and dwell time. The node confirms each step.
3. **Scan.** The node scans each assigned channel in turn, 80 ms per channel by default (a Biscuit can set 40 to 300 ms). This is an active Wi-Fi scan: the node sends probe requests on each channel, while the Dragon Detector's normal radar only listens. JCMK Node then listens for Bluetooth LE for half a second without sending anything.
4. **Report.** After a channel that found something, and at the end of each sweep, the node returns to channel 6 and sends what it found. At the end of a sweep it also sends a heartbeat and listens for new instructions from the core.

Each record is one line of text: `BSSID,SSID,SECURITY,CHANNEL,RSSI,W` for Wi-Fi and `address,,[BLE],0,RSSI,B` for Bluetooth. Commas and line breaks in a network name are replaced by `_`. The core adds the time and position.

**Repeats.** A network is sent once, then again when it is heard at least 8 dB stronger, or after about a minute in view. The core stamps its own position on every record, so the later reports are the ones taken closest to the network. Bluetooth devices are repeated after about a minute only. Repeats never raise the counts.

**Delivery.** Each record gets up to three tries. If all three fail, it is sent again the next time that network is heard. If the core's radio never confirms direct messages, the node sends its reports by broadcast instead and the screen says **LINKED (BROADCAST)**. A keyed JCMK link never does this.

**Link loss.** After 10 seconds without hearing the core, the node asks it to answer. After 30 seconds of silence the link is lost and the node searches again. Once it links again, to the same core or another one, each network is sent again the next time it is heard. The counts are not doubled.

**Channels.** The ESP32-S3 radio covers 2.4 GHz only. The node scans channels 1 to 11, never 12 to 14 and never 5 GHz, even when the core asks for them. If the core assigns no channel the node can reach, for example only 5 GHz channels, the node covers channels 1 to 11 itself. A dual-band core scans 5 GHz with its own radio.

**No GPS.** The node has no position of its own; the core stamps time and location on every record. A GPS receiver attached to the Dragon Detector is not used, and a GPS wardrive cannot run at the same time.

**Nothing is saved on the Dragon Detector.** The node keeps no log, history or files. Without a linked core, nothing is recorded.

## Restart and power loss

The running node is remembered. If the device loses power or restarts (including USB `system restart`), it starts the same node again at boot, opens the node screen and prints `NODE_RESUMED biscuit` or `NODE_RESUMED jcmk`. The counts start from zero.

If the restart was caused by a crash or a watchdog reset, the node is not resumed and the choice is forgotten, so a fault cannot trap the device in a restart loop. USB prints `NODE_RESUME_SKIPPED ... choose it again from Tools > Modes`. Start the node again by hand.

Leaving with EXIT and CONFIRM, or with `node stop`, forgets the node; the next boot starts as usual.

The node settings are stored on the device in the `dd-node` settings namespace (NVS). An Update from the web flasher keeps them; a Fresh installation erases them.

## USB commands

115200 baud, one command per line. While a node runs, the device accepts `node` commands and a short list of read-only and clock commands: `status`, `memory status`, `scan status`, `loop diagnostics`, `ui status`, `display status`, the board status commands, `badge info`, `screenshot`, `screen node`, `clock status`, `clock hardware`, `orientation status`, `time <epoch>`, `clock offset <minutes>` and `system restart`. Anything else answers `NODE_BUSY stop the node first: node stop`.

| Command | What it does |
|---|---|
| `node biscuit` | Start BISCUIT NODE, or switch to it from JCMK Node |
| `node jcmk` | Start JCMK Node, or switch to it from BISCUIT NODE |
| `node stop` | Stop the node, give the radios back to the radar and forget the choice |
| `node status` (or `node`) | One `NODE_STATUS` line with the link state and counters |
| `node config` | One `NODE_CONFIG` line with the saved settings and this device's Wi-Fi address |
| `node key <text>`, `node key clear` | Set or remove the [JCMK encryption key](#jcmk-encryption-key-optional) |
| `node bind <core address>`, `node bind off` | Join [only one core](#bind-to-one-core-optional), or any core again |
| `node identity <text>`, `node identity default` | The name a Biscuit Manager shows for this node, 1 to 40 printable characters |
| `node ble on`, `node ble off` | JCMK Node: also send Bluetooth devices (on by default) |

Commands that start, stop or change something answer `NODE_OK ...`; a refusal answers `NODE_ERROR <reason>`. Settings changed while a node runs take effect at once: the node restarts with them, and its counts start from zero. The key and Bluetooth settings restart only a running JCMK Node. If that restart fails, the node stops, the device returns to Modes and USB prints `NODE_ERROR <reason>`.

A short session, with most `NODE_STATUS` fields left out where `...` is shown:

```text
node jcmk
NODE_STARTED jcmk
NODE_OK jcmk
node status
NODE_STATUS mode=jcmk active=1 phase=scan core=JCMK linked=1 core_mac=24:6F:28:10:20:30 ... wifi=152 ble=31 sent=207 failed=0 ...
node stop
NODE_STOPPED radar restored
NODE_OK stopped
```

The most useful `NODE_STATUS` fields:

| Field | Meaning |
|---|---|
| `phase` | `search`, `join`, `scan`, `ble`, `report`, `listen`, or `off` when no node runs |
| `core` | The linked core: `JCMK`, `PIGLET`, `BISCUIT` or `NONE` |
| `other` | A core that answered but cannot be joined in this mode, for example `BISCUIT SECURE` |
| `core_mac`, `core_rssi` | Address and signal of the linked core |
| `assigned`, `plan`, `dwell_ms` | Whether the core assigned channels, the channels being scanned, and the time per channel |
| `role`, `node` | The role a Biscuit assigned; the node's number among a JCMK core's nodes |
| `wifi`, `ble`, `sent` | The counts on the screen |
| `failed` | Records given up after three tries |
| `dropped` | Sightings that did not fit in the send queue; they are sent at a later sighting |
| `acked` | Records the core confirmed itself (cores that do this) |
| `links`, `losses` | Links made and lost since the node started |
| `broadcast` | 1 when reports go out by broadcast |
| `bound`, `keyed`, `ble_reports` | The settings in use. `keyed` is 0 or 1; the key itself is never shown |
| `remembered` | The node that resumes after a restart |
| `heap`, `min_heap`, `largest` | Free memory, for long-run checks |

## JCMK encryption key (optional)

If the JCMK core uses ESP-NOW encryption, give the node the same key:

```text
node key <the same text as on the core>
NODE_OK key=1
```

- The key is 1 to 64 printable characters. Both ESP-NOW keys are derived from this text the same way the JustCallMeKoko core derives them.
- The key is stored on the device and never printed. `node config` and `node status` only show `keyed=1`. To change it, set it again; `node key clear` removes it.
- Only JCMK Node uses it. BISCUIT NODE works with Extended mode, which is not encrypted.
- On a keyed link only answers from the core itself keep the link alive, so the node asks the core to answer about every 10 seconds. Reports never fall back to broadcast.
- The key must match on both sides, or be off on both. With a mismatch the node can show LINKED and a rising SENT count while the core saves nothing.
- The key travels over the USB cable as plain text and is kept in the device's settings storage without further protection. Treat the Dragon Detector as holding the key.

## Bind to one core (optional)

Where more than one core is in range, `node bind <address>` makes the node send its requests to that core alone and join no other. Read the address from `core_mac` in `node status` while linked. For example, `node bind 24:6F:28:10:20:30` answers `NODE_OK bound=1 core=24:6F:28:10:20:30`. `node bind off` allows any core again.

## Troubleshooting

| Screen says | Meaning | What to do |
|---|---|---|
| SEARCHING FOR BISCUIT, SEARCHING FOR CORE, SEARCHING | No core of this kind has answered on channel 6 | Check that the core is on, set up for ESP-NOW nodes and in range. If the node is bound, check the address with `node config`. |
| SET BISCUIT TO EXTENDED MODE | A Biscuit in Secure mode answered | Switch the Biscuit to Extended mode. |
| BISCUIT FOUND: USE BISCUIT NODE | JCMK Node is running and a Biscuit answered | Leave and start BISCUIT NODE, or send `node biscuit`. |
| JCMK CORE FOUND: USE JCMK Node | BISCUIT NODE is running and a JCMK core or a Piglet answered | Leave and start JCMK Node, or send `node jcmk`. |
| LINKED TO BISCUIT, LINKED TO CORE, LINKED TO PIGLET CORE, LINKED | Linked; records are being sent | Nothing. |
| LINKED (BROADCAST) | Linked, but the core's radio did not confirm direct messages, so reports go out by broadcast | Nothing; records still arrive. |
| TAP CONFIRM TO LEAVE, PRESS AGAIN TO LEAVE, HOLD AGAIN TO LEAVE | One exit press was made | Press again within 3 seconds to leave, or wait to keep the node running. |

Small panels may show shorter forms, such as BISCUIT: SET EXTENDED MODE, USE EXTENDED MODE, USE BISCUIT NODE or USE JCMK Node. A wrong-core hint fades about 15 seconds after that core stops answering.

Other problems:

- **The count stays at 0 while linked.** The node only scans the channels it was given; check `plan=` in `node status`. Networks on 5 GHz alone are never heard.
- **The core saves nothing, yet the node says LINKED and SENT rises.** Check that the [encryption key](#jcmk-encryption-key-optional) matches on both sides, or is off on both.
- **USB answers `NODE_BUSY`.** A node is running. Use `node stop`, or only the commands listed above.
- **The node did not come back after a restart.** USB `NODE_RESUME_SKIPPED` means the restart was a crash or watchdog reset. Start the node again by hand and keep the USB log.

### Start refusals

| Message | What to do |
|---|---|
| Stop the stopwatch first | Pause or reset the stopwatch, then start the node. |
| Finish the GPS wardrive first | Stop the wardrive and wait for its save. |
| Wait for the upload to finish | Let the wardrive upload finish or cancel it. |
| Wait for the save to finish | A recon file is still being written; try again in a moment. |
| BLE Lab is closing; try again | Start the node again. |
| Not enough memory | Restart the device and start the node before anything else. |
| Radio did not start, Wi-Fi events unavailable, Wi-Fi could not switch to node mode | Restart the device and try again. If it repeats, keep the output of `node status` and `memory status`. |

## Hardware checklist (not yet verified)

None of these has been checked on hardware yet. The fuller per-board list is in [validation status](VALIDATION.md#biscuit-node-and-jcmk-node).

- [ ] BISCUIT NODE with a Biscuit in Extended mode: LINKED TO BISCUIT, the node is listed in the Biscuit Manager under its name, and records reach the Biscuit's log with its location.
- [ ] BISCUIT NODE with a Biscuit in Secure mode: SET BISCUIT TO EXTENDED MODE, and no records.
- [ ] JCMK Node with an ESP32 Dual Band Wardriver core: LINKED TO CORE, the assigned channels appear in `plan=`, and Wi-Fi and Bluetooth records reach the core's log.
- [ ] JCMK Node with a matching key and with a wrong key; `node config` never prints the key.
- [ ] JCMK Node with a Piglet: LINKED TO PIGLET CORE, and records arrive.
- [ ] Switch the core off: SEARCHING within about 30 seconds. Switch it on: linked again, and records resume.
- [ ] Leave by touch, by buttons and with `node stop`. One press alone does not leave. The radar, alerts and audio work again afterwards.
- [ ] Unplug power while a node runs: the node resumes at boot.
- [ ] A one-hour drive on each board, including M5Stack Dial and Waveshare Lite: the screen keeps updating, and `node status` memory stays steady.

## Credits

BISCUIT NODE and JCMK Node are original Dragon Detector code, not official firmware of the Biscuit, JustCallMeKoko or Piglet projects, and imply no endorsement. The JCMK message layout, numbering, channel table, timing and key derivation follow the MIT-licensed ESP32DualBandWardriver by Just Call Me Koko ([licence](licenses/ESP32DualBandWardriver-LICENSE.txt)). Piglet's public behaviour was the reference for the Biscuit handshake; no Piglet code is included. The logos are owner-supplied artwork. See [credits](../CREDITS.md#biscuit-node-and-jcmk-node).
