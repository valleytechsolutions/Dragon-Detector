# BISCUIT NODE and JCMK Node

**BISCUIT NODE** and **JCMK Node** turn a Dragon Detector into a scan-only wardriving node for a separate wardriving core. The node scans Wi-Fi (and, for JCMK Node, Bluetooth LE) and sends each sighting to the core over ESP-NOW. The core adds the time and its own GPS position and keeps the log. While a node runs, the Dragon Detector does nothing else.

Both modes are on every board profile: the full-UI boards, Waveshare Lite and M5Stack Dial.

**Status: field testing.** BISCUIT NODE has run with a real Biscuit on a Waveshare 1.75C (see the [hardware checklist](#hardware-checklist)). JCMK Node and the other boards are not yet verified on hardware. The link has native tests against simulated JCMK, Biscuit and Piglet cores, the session and its USB commands have their own native test, and the screen has a host render check at every panel size.

## Which mode for which core

| Mode | Joins | Sends to the core |
|---|---|---|
| **BISCUIT NODE** | A Biscuit core (Biscuit Manager) set to **Extended** mode | Wi-Fi networks |
| **JCMK Node** | A JustCallMeKoko **ESP32 Dual Band Wardriver** core, or a Piglet acting as a JCMK-style core | Wi-Fi networks and Bluetooth LE devices; Wi-Fi only to a Piglet |

- A Biscuit in Secure (encrypted) mode cannot be joined. Switch it to Extended mode in the Biscuit app's Node Management settings; the Biscuit does not change modes while a wardrive or another operation runs. The app can list the node as connected while the Biscuit is still in Secure mode, but nothing reaches it, and the node screen says **SET BISCUIT TO EXTENDED MODE**.
- BISCUIT NODE works only during a Biscuit wardrive. Once linked it shows **WAITING FOR WARDRIVE** and only checks in on channel 6; it starts scanning when a wardrive started in the Biscuit app gives it its role, as the Biscuit's own nodes do, and goes back to waiting as soon as the wardrive is stopped.
- BISCUIT NODE never joins a JCMK core, and JCMK Node never joins a Biscuit. If the wrong kind of core answers, the screen tells you which mode to use.
- A Biscuit receives Wi-Fi only. A JCMK core receives Wi-Fi and Bluetooth. Bluetooth reports can be switched off with `node ble off`.
- A Piglet marks its answer, so the screen shows **LINKED TO PIGLET CORE**. Records then use the JCMK wording. A Piglet core receives Wi-Fi only: it would log each Bluetooth device as a Wi-Fi network, so the node sends it none and the screen leaves out the BLE count.
- A Piglet core has no ESP-NOW encryption. With [encryption](#jcmk-encryption-optional) on, JCMK Node does not join a Piglet and the screen says **PIGLET CORE: CLEAR NODE KEY**. Turn encryption off with `node encrypt off` to use a Piglet.

Set up the core for ESP-NOW nodes as its own documentation describes. The Dragon Detector needs no pairing step: it finds the core by itself.

## Start a node

- **Full boards:** Tools → Modes → **BISCUIT NODE** or **JCMK Node**, the first two entries in the list.
- **M5Stack Dial:** Menu → Tools → Modes, then BISCUIT NODE or JCMK Node.
- **Waveshare Lite:** Lite has no Tools menu. Its **MODES** page, in the ring of pages that a BOOT hold steps through, is where the two modes live. Hold BOOT through the pages to MODES (six holds from Radar), Tap to pick a node, then Double tap to start it.
- **USB:** `node biscuit` or `node jcmk`.

The node screen replaces everything else. If the node cannot start, the reason is shown and the device stays on Modes; on Waveshare Lite the reason shows on the MODES page for a few seconds. See [start refusals](#start-refusals).

Starting a node pauses the radar, detectors, alerts, notifications, audio, history writing and the badge. It ends running radio tools and tests, BLE Lab (a repeating Find My beeper included), captures (a capture recording to SD is finished on the card), the local portal, Room motion, a joined Wi-Fi network, recon sessions and recipes, and it switches off the training fox beacon, which stays off afterwards.

A few things are never cut off. The node refuses to start, and changes nothing, while the stopwatch runs. During a GPS wardrive or a wardrive upload, Modes cannot be opened and USB node commands answer `WARDRIVE_BUSY` or `UPLOAD_BUSY`. The node waits for only two things: a capture file that is still being saved, and BLE Lab letting go of the radio. In both cases the other tools have already been stopped, and you are asked to try again in a moment.

## The node screen

The screen is black with white artwork on every theme, so it reads the same on LCD and AMOLED panels. From top to bottom:

- The mode name: **BISCUIT NODE** or **JCMK Node**.
- The link state in a few words: green when linked (or linked and waiting for a Biscuit wardrive), amber while searching, red when something needs your attention or an exit waits for CONFIRM, and white for 3 seconds after a short press, to show how to leave. Small panels use shorter wording. The meanings are listed under [troubleshooting](#troubleshooting).
- The **Biscuit Shop** or **JCMK** logo in the middle.
- The number of **WI-FI NETWORKS** found, in large digits.
- A line of smaller counts: **BLE** (JCMK Node with Bluetooth reports on, not shown with a Piglet core), **SENT**, and **CH**, the channel last scanned. Where the line does not fit, the channel is left out first.
- **EXIT** at the bottom. Waveshare Lite shows HOLD BUTTON TO EXIT instead.

A short mark on the rim, in the colour of the link state, shows where the channel last scanned sits in the current channel plan. It stays there between scans, while the radio visits channel 6 to talk to the core. While the node searches, and after it links until the first scan, the mark circles the rim once every 9 seconds.

The name, status, logo, counts and EXIT (on Waveshare Lite, the hint) shift a few pixels each minute to spare AMOLED panels. The rim stays centred on the glass. On full boards the display's dim and screen-off timeouts still apply while a node runs.

Alerts, notifications, the clock setup prompt and other pages do not open over the node screen.

### What the counts mean

- **WI-FI NETWORKS** is the number of different access points (BSSIDs) heard since this node started. A router that broadcasts several networks counts once for each. Hearing a network again does not add to it while the node's table has room (see below).
- **BLE** is the number of different Bluetooth LE addresses. A phone that changes its address counts again.
- **SENT** counts records handed to the core, repeats included, so it can be larger than the other two counts together. On a direct link the core's radio confirmed each one. When the screen says LINKED (BROADCAST), nothing confirms a broadcast, so SENT counts records sent out.
- **CH** is the channel last scanned. It holds between scans. While the node searches, and after it links until the first scan, it shows 6, the home channel.

The counts start from zero each time the node starts, including after a restart or a settings change. Networks are only counted while the node is linked: without a core the node does not scan at all.

The node remembers what it has heard in one table shared by Wi-Fi networks and Bluetooth addresses. The M5Stack Dial has no PSRAM and has room for about 2,000 entries; the other boards have room for about 32,000. The counts stay exact until the table is roughly half full. After that, a network heard again can now and then be counted and sent again, about 1 in 7 by the time the table is full. When room runs out, a Bluetooth address is replaced before any network, so on JCMK Node with Bluetooth reports on it is mostly the BLE count that rises again.

## Leave a node

Leaving takes two deliberate steps, so a bump, a thumb on a side button or a bouncing contact cannot end a drive.

- **Touch:** tap **EXIT**. It turns red and reads **CONFIRM**. Tap **CONFIRM** between half a second and 3 seconds later. If you do not, it goes back to EXIT and the node keeps running. A quick double tap counts as one tap, and so does a burst of quick taps: CONFIRM counts only after half a second without a tap.
- **Knob (CrowPanel 2.1, SenseCAP Watcher):** a click of the knob never leaves; it shows how to leave. Hold the knob (about 0.8 seconds), then hold it again or tap **CONFIRM** within 5 seconds. Touch EXIT then CONFIRM works too.
- **Side buttons on full boards** (BOOT, PWR, Home, Back): a short press never leaves. It wakes a dark display, or shows how to leave on the status line for 3 seconds. A **hold** counts as one tap on EXIT where the board reports holds: BOOT held 2 seconds and released on the Waveshare 1.75C (the 1.75C sees the hold only when you let go, and the 5 seconds start then), BOOT held 0.7 seconds on the Waveshare 1.85B and 1.43C. After a hold, tap CONFIRM or hold again within 5 seconds.
- **M5Stack Dial:** tap EXIT, then CONFIRM, or hold the front button twice within 5 seconds. A tap or a quick double press of the front button only shows how to leave.
- **Waveshare Lite:** Hold BOOT, then Hold it again within 5 seconds. Taps only show how to leave.
- **USB:** `node stop`.

After the first step the status line reads **TAP CONFIRM TO LEAVE** on touch boards (CrowPanel 2.1 and SenseCAP Watcher included) and the M5Stack Dial, and **HOLD AGAIN TO LEAVE** on Waveshare Lite. After a short key press it reads **TAP EXIT TO LEAVE** (**HOLD KNOB TO EXIT** on CrowPanel 2.1 and SenseCAP Watcher, **HOLD BUTTON TO EXIT** on Waveshare Lite).

On full boards, a touch or press that only wakes a dark display does not count.

After leaving, the device returns to Modes. The radios go back to the radar as they were before the node started, and the node choice is forgotten.

## How a node works

1. **Search.** The node waits on Wi-Fi channel 6, the home channel every core listens on, and broadcasts a request: the first within 0.3 seconds, then after 0.3, 0.6, 1.2, 2.4 and 4.8 seconds, then every 5 seconds. Nodes powered on together do not all call at the same moment. A bound node sends its requests to its own core only.
2. **Join.** The first core of the right kind that answers is linked. A JCMK core lists the node as external and assigns it nothing (see [channels](#channels)); an older JCMK core may assign a range of channels and tell the node its number among the core's nodes, and a Piglet core sends a share of its channel list. A Biscuit receives the node's name (`DragonDetector:<version>` by default), assigns a role and sends a channel list and dwell time. The node confirms each step, and a handshake message the Biscuit's radio did not confirm is sent again, up to three times in all. A Biscuit gives the role when a wardrive starts in the Biscuit app, not before, so until then the screen says **WAITING FOR WARDRIVE** and the node does not scan: it stays on channel 6 and checks in with a heartbeat about every 2 seconds. If no role or no channel list comes, the node sends its name again about every 2 seconds, for up to five tries in all; after that it stays linked and waits. If the node's own confirmation of either does not get through, it asks the Biscuit again at once. When the wardrive is stopped in the app, the Biscuit tells its nodes so, and the node goes back to waiting at once. A role of 0 does the same, and a wardrive that ends with neither is noticed within about two minutes (see [link loss](#how-a-node-works)).
3. **Scan.** Linked to a JCMK or Piglet core, or to a Biscuit that is wardriving, the node scans each channel of its plan in turn, 80 ms per channel by default (a Biscuit can set 40 to 300 ms). This is an active Wi-Fi scan: the node sends probe requests on each channel, while the Dragon Detector's normal radar only listens. Linked to a JCMK core with Bluetooth reports on, JCMK Node then listens for Bluetooth LE for half a second without sending anything.
4. **Report.** After a channel that found something, and at the end of each sweep, the node returns to channel 6 and sends what it found. At the end of a sweep it also sends a heartbeat and listens for new instructions from the core.

Each record is one line of text: `BSSID,SSID,SECURITY,CHANNEL,RSSI,W` for Wi-Fi and `address,,[BLE],0,RSSI,B` for Bluetooth. Commas and line breaks in a network name are replaced by `_`. The core adds the time and position.

**Repeats.** A network is sent once, then again when it is heard at least 8 dB stronger, or after about a minute in view. Bluetooth devices are repeated after about a minute only. Repeats do not raise the counts. What a core does with a repeat depends on the core. A JCMK core logs an address only when it is not among the last 200 addresses it has taken in from all its nodes. It normally keeps the first report and ignores the repeats, though it still acknowledges them. A first report that reaches it before it has a GPS fix takes up one of those 200 places too, so that network is logged only once the core's list has moved past it. A core that logs every record, such as a Piglet, stamps its own position on each repeat as well, so there the later, closer reports are kept too.

**Delivery.** Each record gets up to three tries. If all three fail, it is sent again the next time that network is heard. If the core's radio never confirms direct messages, the node sends its reports by broadcast instead and the screen says **LINKED (BROADCAST)**. A keyed JCMK link never does this. Any core in range can hear a broadcast.

**Link loss.** After 10 seconds without hearing the core, the node asks it to answer. While linked, this request goes to the linked core alone (by broadcast only on a LINKED (BROADCAST) link), so no other core in range takes the node on. On an open link to a JCMK or Piglet core, the core's radio confirming a message counts as hearing the core. On a keyed link and on a Biscuit link only the core's own messages count, because the radio confirms before the core has read the message, and a Biscuit that restarted or forgot the node still confirms. A Biscuit is expected to treat every request as coming from a node that is new to it. So when it answers one, the node introduces itself again. A Biscuit that keeps sending is never silent for 10 seconds, so it is also asked about every 90 seconds: if it restarted and forgot the node, the node joins it again. A wardriving Biscuit answers the first introduction after each question with the role again and leaves the repeats unanswered, so the node stays registered and the screen does not change while it asks. A Biscuit that is not wardriving gives no role. Its stop message normally ends the wardrive for the node at once, but the node can miss it while it scans away from channel 6. So when a whole round of introductions brings no role, the node asks again 15 seconds later, and if that round brings none either, the wardrive has ended: the node goes back to **WAITING FOR WARDRIVE**, still linked, about two minutes after the wardrive stopped at most. A single lost answer does not stop it. After 30 seconds of silence the link is lost and the node searches again. Once it links again, to the same core or another one, each network is sent again the next time it is heard. The counts are not doubled.

### Channels

The ESP32-S3 radio covers 2.4 GHz only. The node scans channels 1 to 11, never 12 to 14 and never 5 GHz, even when the core asks for them. If the core assigns no channel the node can reach, for example only 5 GHz channels, the node covers channels 1 to 11 itself.

JCMK and Piglet cores do not scan: they only log what their nodes send. Such a core shares out its channel list (channels 1 to 14 and 26 channels at 5 GHz) evenly among its nodes, so 5 GHz needs dual-band nodes.

- **JCMK core.** Every request and heartbeat from the node carries the JCMK core's own capability text for an external node. The core then leaves the Dragon Detector out of its channel split, lists it as **6G EXT** in its Nodes menu and assigns it no channels. The node scans channels 1 to 11 itself, and the core's other nodes share the whole channel list between them. `node status` then shows `assigned=0`, `node=0/0` and `plan=1,2,3,4,5,6,7,8,9,10,11`. BISCUIT NODE sends the same text, so a BISCUIT NODE that is searching near a JCMK rig takes no channels from it either. An older JCMK core without this capability gives the node a share like any other node; the part of that share above channel 11 is then scanned by no one.
- **Piglet core.** It has no such capability. It gives a Dragon Detector it has heard from an even share like any other node, and the 5 GHz channels and channels 12 to 14 in that share are scanned by no one. This also happens while a node in either mode is searching near a Piglet without joining it. A [bound](#bind-to-one-core-optional) node sends its requests to its own core only.

**No GPS.** The node has no position of its own; the core stamps time and location on every record. A GPS receiver attached to the Dragon Detector is not used, and a GPS wardrive cannot run at the same time.

**Nothing is saved on the Dragon Detector.** The node keeps no log, history or files. Without a linked core, nothing is recorded.

## Restart and power loss

The running node is remembered. If the device loses power or restarts (including USB `system restart`), it starts the same node again at boot, opens the node screen and prints `NODE_RESUMED biscuit` or `NODE_RESUMED jcmk`. The counts start from zero.

So that a fault cannot trap the device in a restart loop, the node is not resumed, and the choice is forgotten, in two cases:

- **A crash or a watchdog reset.** USB prints `NODE_RESUME_SKIPPED <mode> reset=<n>: the device crashed or a watchdog fired; choose it again from Modes`.
- **Two quick restarts in a row**, each within a minute of resuming. This is often a battery or USB supply that sags when the node transmits. USB prints `NODE_RESUME_SKIPPED <mode> reset=<n>: it restarted twice within a minute of resuming; check the battery or USB supply, then choose it again from Modes`. A single quick restart still resumes. So a node that has already run for a minute rides out two restarts close together, such as brownouts while a car engine starts, and a third is skipped. If the node was itself resumed less than a minute earlier, for example when the device powered up with the ignition, only the first such restart resumes. Once a resumed node has run for a minute, the count starts again from zero. The count is kept in memory that survives a restart or a brownout but not a complete loss of power, so a supply that cuts out completely each time is not caught.

Start the node again by hand from Modes (on Waveshare Lite, the MODES page). If a resumed node cannot start, USB prints `NODE_ERROR resume <mode>: <reason>` and the choice is kept, so the next boot tries again.

Leaving with EXIT and CONFIRM, or with `node stop`, forgets the node; the next boot starts as usual.

The node settings are stored on the device in the `dd-node` settings namespace (NVS). An Update from the web flasher keeps them; a Fresh installation erases them.

## USB commands

115200 baud, one command per line. While a node runs, the device accepts `node` commands (`node` alone included) and a short list of clock, restart and read-only commands: `status`, `memory status`, `scan status`, `loop diagnostics`, `ui status`, `touch status`, `display status`, the board status commands, `badge info`, `screenshot`, `screen node`, `clock status`, `clock hardware`, `orientation status`, `radar status`, `tone settings`, `tone get <n>`, `theme info`, `theme read <offset> <count>`, `time <epoch>`, `clock offset <minutes>` and `system restart`. USB Studio's read-only queries are among them, so Studio can connect, read the settings and set the clock while a node runs; other changes are refused until the node stops, and Studio says a wardriving node is running. Anything else, the wardrive and GPS commands included, answers `NODE_BUSY stop the node first: node stop`.

| Command | What it does |
|---|---|
| `node biscuit` | Start BISCUIT NODE, or switch to it from JCMK Node |
| `node jcmk` | Start JCMK Node, or switch to it from BISCUIT NODE |
| `node stop` | Stop the node, give the radios back to the radar and forget the choice. With no node running it only forgets a remembered node, and the screen stays where it is |
| `node status` (or `node`) | One `NODE_STATUS` line with the link state and counters |
| `node config` | One `NODE_CONFIG` line with the saved settings and this device's Wi-Fi address |
| `node key <text>`, `node key "<text>"`, `node key clear` | Set the [JCMK encryption key](#jcmk-encryption-optional) and turn encryption on, or remove the key and turn encryption off |
| `node encrypt on`, `node encrypt off` | Turn JCMK encryption on or off and keep the stored key as it is; on with no key stored matches a core whose ENOW Key is blank |
| `node bind <core address>`, `node bind off` | Join [only one core](#bind-to-one-core-optional), or any core again |
| `node identity <text>`, `node identity default` | The name a Biscuit Manager shows for this node, 1 to 40 printable characters |
| `node ble on`, `node ble off` | JCMK Node: also send Bluetooth devices to a JCMK core (on by default) |
| `node trace on`, `node trace off` | For troubleshooting: print a `NODE_FRAME` line for every node message heard and a `NODE_SENT` line for every message sent, until `node trace off` or a restart |

Commands that start, stop or change something answer `NODE_OK ...`; a refusal answers `NODE_ERROR <reason>`. Settings changed while a node runs take effect at once: the node restarts with them, and its counts start from zero. The key, encryption and Bluetooth settings restart only a running JCMK Node. If that restart fails, or a switch from one node to the other fails, the old node has already stopped: USB prints `NODE_STOPPED radar restored` and then `NODE_ERROR <reason>`, the device returns to Modes and the choice is forgotten. A start that fails with no node running changes nothing that was remembered.

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
| `other` | A core that answered but cannot be joined in this mode, for example `BISCUIT SECURE`, or `PIGLET` for JCMK Node with encryption on |
| `core_mac`, `core_rssi` | Address and signal of the linked core |
| `registered` | 1 once the core has taken the node on: as soon as a JCMK or Piglet core is linked, and for a Biscuit once it has given this link a role. It then stays 1 until the link ends |
| `waiting` | 1 while linked to a Biscuit that is not wardriving (no role yet, or role 0): the node checks in and does not scan |
| `assigned`, `plan`, `dwell_ms` | Whether the core assigned channels (0 with a JCMK core that lists the node as external), the channels being scanned, and the time per channel |
| `role`, `node` | The role a Biscuit assigned; the node's number among a JCMK-style core's nodes (`0/0` when the core gave none) |
| `wifi`, `ble`, `sent` | The counts on the screen |
| `failed` | Records given up after three tries |
| `dropped` | Sightings that did not fit in the send queue; they are sent at a later sighting |
| `acked` | Records the core confirmed itself (cores that do this) |
| `links`, `losses` | Links made and lost since the node started |
| `broadcast` | 1 when reports go out by broadcast |
| `bound`, `keyed`, `ble_reports` | What the running node uses, all 0 when no node runs: bound to one core, encryption on, Bluetooth reports on. `keyed` and `ble_reports` are always 0 for BISCUIT NODE, and a Piglet core gets no Bluetooth even with `ble_reports=1`. The key itself is never shown. `node config` shows the saved settings |
| `heard`, `heard_core` | Node messages heard from any core, and from the linked core, since the node started. `heard` rising while `linked=0` means a core is answering that this mode cannot join (see `other`) |
| `remembered` | The node that resumes after a restart |
| `heap`, `min_heap`, `largest` | Free memory, for long-run checks |

### Event lines

A running node also prints a line by itself when its link changes, so a USB log shows what happened and when:

| Line | When |
|---|---|
| `NODE_EVENT linked core=...` | A core was joined |
| `NODE_EVENT role ... text="1"` | A Biscuit gave this link its first role, or changed it; `text` is the role number (a Wi-Fi wardrive gives 1, 0 means no wardrive) |
| `NODE_EVENT channels ...` | A Biscuit sent a channel list different from the last one on this link; `text` holds it as sent, for example `channels=1,2,3,...,161;dwell=110` |
| `NODE_EVENT no_role type=8 core=BISCUIT text="0"` | The Biscuit said its wardrive was stopped (message type 8), so the node waits again. Without `type=`: two rounds of introductions in a row brought no role, because the stop message was missed, with the same result |
| `NODE_EVENT message type=... text="..."` | The linked core sent a message type this node does not use; logged once per type and link, so a field log shows it |
| `NODE_EVENT core_lost ...` | The core was silent for 30 seconds; the node searches again |
| `NODE_EVENT mode_changed core=BISCUIT SECURE text="..."` | The linked core's answer says it can no longer be joined, for example a Biscuit switched to Secure mode; `text` is the core's answer, word for word, such as `encrypt=1;wdbatch=2;nodecfg=2;ch=6;ap6=1` |
| `NODE_EVENT other_core core=... text="..."` | A core this mode cannot join answered; logged once per kind while its hint is on the screen |
| `NODE_EVENT key_lost ...` | JCMK Node with encryption: the radio would not take the key back, so the link was dropped |
| `NODE_EXIT_PRESS source=... dark=... counted=...` | A key, the knob or the screen's EXIT was pressed while a node runs. `source` names the input: `boot_key`, `pwr_key`, `boot_hold`, `menu_key`, `home_key`, `cycle_key`, `knob_click`, `knob_hold` (full boards), `button`, `button_double`, `button_hold` (Lite and Dial) or `screen`. `dark=1` (full boards only) means the press only woke a dark display. `counted=1` means a hold made while the node screen was shown and lit, which counts as an exit request; a short press has `counted=0` and only shows how to leave. `source=screen` lines carry no other fields and are always an exit request |
| `NODE_EXIT_ARMED`, `NODE_EXIT_CONFIRMED`, `NODE_EXIT_IGNORED reason=too_soon` | The first exit request arms, a second one leaves; one less than half a second after the previous request, counted or not, is ignored as a bounce or double tap, so a burst of quick taps never leaves |
| `NODE_SCREEN_CLOSED busy=...` | The node screen closed because the node was stopped or switched another way, such as over USB |
| `NODE_FRAME from=... type=... counter=... declared=... text="..." payload=... hex=...`, `NODE_SENT to=... ...` | Only after `node trace on`: every node message heard (`from=`, with its signal strength) or sent (`to=`), with its type, counter, the text length it declares and its text, and in hex the bytes after the 11-byte header up to the last one that is not zero (`payload=` counts them); for a message shorter than 11 bytes, such as a JCMK channel assignment, every byte after the type. The node's own records show `hex=-`. Lines that could not be printed during a binary transfer are counted in a later `NODE_FRAME dropped=...` line. If the device has no memory to spare for the trace, `node trace on` answers `NODE_OK trace=0` |

`text` shows printable characters only (others, and double quotes, as `.`), at most 200 of them (the whole message), and `length` gives the full length. For `role` and `no_role` the text is the role number and `length` is 1, whatever the frame itself carried. Event lines are held back while a screenshot or other binary transfer is on the cable and follow it.

## JCMK encryption (optional)

If the JCMK core has **Use Encryption** ticked in its settings, turn encryption on for the node with the same key:

- **The core has an ENOW Key:** send `node key <the same text>`. It answers `NODE_OK key=1` and turns encryption on.
- **The core has no ENOW Key typed:** send `node key ""` (two double quotes). It answers `NODE_OK key=1`, and the node uses the empty key, as the core does. With no key stored on the node, `node encrypt on` does the same and answers `NODE_OK keyed=1`.
- **The key starts or ends with spaces:** put it in double quotes, for example `node key "my key "`. Without quotes, spaces at the ends of the USB line are lost.

```text
node key <the same text as on the core>
NODE_OK key=1
```

- The key is up to 64 printable characters, at least 1 unless it is in quotes. Both ESP-NOW keys are derived from this text the same way the JustCallMeKoko core derives them, the empty key included.
- To check what the core holds, read its own USB log at boot: it prints `ENOW Key:` followed by the stored key, which is empty when none was typed.
- `node encrypt off` turns encryption off and keeps the key for later; `node encrypt on` uses it again. `node key clear` removes the key and turns encryption off. To change the key, set it again.
- The key is stored on the device and never printed. `node config` shows the saved setting as `keyed=0` or `keyed=1`; `node status` shows `keyed=1` only while a running JCMK Node uses encryption.
- Only JCMK Node uses it. BISCUIT NODE works with Extended mode, which is not encrypted.
- A Piglet core has no ESP-NOW encryption. With encryption on, JCMK Node does not join a Piglet and the screen says PIGLET CORE: CLEAR NODE KEY. Send `node encrypt off` to use the Piglet.
- On a keyed link only answers from the core itself keep the link alive, so the node asks its core, directly, to answer about every 10 seconds. Reports never fall back to broadcast.
- The setting must match the core: the same key with encryption on both sides, or encryption off on both. With a mismatch the node can show LINKED and a rising SENT count while the core saves nothing.
- The key travels over the USB cable as plain text and is kept in the device's settings storage without further protection. Treat the Dragon Detector as holding the key.

## Bind to one core (optional)

Where more than one core is in range, `node bind <address>` makes the node send its requests to that core alone and join no other. Read the address from `core_mac` in `node status` while linked. For example, `node bind 24:6F:28:10:20:30` answers `NODE_OK bound=1 core=24:6F:28:10:20:30`. `node bind off` allows any core again.

## Troubleshooting

| Screen says | Meaning | What to do |
|---|---|---|
| SEARCHING FOR BISCUIT, SEARCHING FOR CORE, SEARCHING | No core of this kind has answered on channel 6 | Check that the core is on, set up for ESP-NOW nodes and in range. If the node is bound, check the address with `node config`. |
| WAITING FOR WARDRIVE, WAITING FOR DRIVE, WAITING | Linked to a Biscuit that is not wardriving; the node checks in and does not scan | Start a Wi-Fi wardrive in the Biscuit app. The node starts scanning when the Biscuit gives it its role. When the wardrive is stopped the node shows this again at once, or within about two minutes if it missed the Biscuit's stop message. |
| LINKED TO BISCUIT, and the count rises although the app shows no wardrive | The Biscuit still gives its nodes the Wi-Fi role, so for its nodes a wardrive is still running (seen once in the field, after a day of tests) | Start a wardrive in the Biscuit app and stop it again. At the stop the node goes back to WAITING FOR WARDRIVE. |
| SET BISCUIT TO EXTENDED MODE | BISCUIT NODE is running and a Biscuit in Secure mode answered | Switch the Biscuit to Extended mode. |
| BISCUIT FOUND: USE BISCUIT NODE | JCMK Node is running and a Biscuit answered, in either mode | Leave and start BISCUIT NODE, or send `node biscuit`. A Biscuit in Secure mode also needs Extended mode. |
| JCMK CORE FOUND: USE JCMK Node | BISCUIT NODE is running and a JCMK core or a Piglet answered | Leave and start JCMK Node, or send `node jcmk`. |
| PIGLET CORE: CLEAR NODE KEY | JCMK Node has encryption on and a Piglet core answered; a Piglet has no encryption | Send `node encrypt off` (or `node key clear`) to use the Piglet. A keyed JCMK core in range is still joined when it answers. |
| LINKED TO BISCUIT, LINKED TO CORE, LINKED TO PIGLET CORE, LINKED | Linked; records are being sent | Nothing. |
| LINKED (BROADCAST) | Linked, but the core's radio did not confirm direct messages, so reports go out by broadcast | Nothing, as long as the core is in range. Broadcast records are not confirmed, so check the core's own log if in doubt. |
| TAP CONFIRM TO LEAVE, HOLD AGAIN TO LEAVE | One exit request was made: TAP CONFIRM on touch boards and the M5Stack Dial, HOLD AGAIN on Waveshare Lite | Confirm within 3 seconds (5 after a hold) to leave, or wait to keep the node running. |
| TAP EXIT TO LEAVE, HOLD KNOB TO EXIT, HOLD BUTTON TO EXIT | A side button or the knob was pressed briefly; that never leaves | Nothing, unless you want to leave: see [leave a node](#leave-a-node). It goes after 3 seconds. |

Small panels may show shorter forms, such as BISCUIT: SET EXTENDED MODE, USE EXTENDED MODE, BISCUIT: USE BISCUIT NODE, USE BISCUIT NODE, JCMK CORE: USE JCMK Node, USE JCMK Node, PIGLET: CLEAR NODE KEY or CLEAR NODE KEY. A wrong-core hint fades about 45 seconds after that core stops answering, because a Biscuit can stay quiet for half a minute.

Other problems:

- **The count stays at 0 while linked.** On BISCUIT NODE, check that a wardrive is running in the Biscuit app (`waiting=1` in `node status` means none is). Otherwise the node only scans the channels in its plan; check `plan=`. Networks on 5 GHz alone are never heard.
- **The Biscuit app shows the node connected, but the node says SEARCHING FOR BISCUIT or SET BISCUIT TO EXTENDED MODE and finds nothing.** The Biscuit is in Secure mode. Switch it to Extended mode in the app's Node Management settings while no wardrive runs.
- **The node left by itself.** It did not: look for `NODE_EXIT_PRESS` lines in the USB log. Since this version a short press of a side button never leaves.
- **The core saves nothing, yet the node says LINKED and SENT rises.** Check that the [encryption](#jcmk-encryption-optional) setting matches the core.
- **USB answers `NODE_BUSY`.** A node is running. Use `node stop`, or only the commands listed above.
- **USB answers `WARDRIVE_BUSY` or `UPLOAD_BUSY` to a node command.** A GPS wardrive or a wardrive upload owns the radio. Stop the wardrive and wait for its save, or let the upload finish or cancel it, then start the node.
- **The node did not come back after a restart.** USB printed `NODE_RESUME_SKIPPED`: the restart was a crash or a watchdog reset, or the device restarted twice within a minute of resuming. Check the supply, start the node again by hand and keep the USB log.

### Start refusals

| Message | What to do |
|---|---|
| Stop the stopwatch first | Pause or reset the stopwatch, then start the node. |
| Saving the capture; try again in a moment | A capture file is still being saved. Start the node again in a moment. |
| BLE Lab is closing; try again | BLE Lab was stopped but has not yet let go of the radio. Start the node again. |
| Not enough memory | Restart the device and start the node before anything else. |
| Radio did not start, Wi-Fi events unavailable, Wi-Fi could not switch to node mode | Restart the device and try again. If it repeats, keep the output of `node status` and `memory status`. |

While a GPS wardrive or a wardrive upload is running, Modes cannot be opened and USB node commands answer `WARDRIVE_BUSY` or `UPLOAD_BUSY`, so the node never gets as far as a refusal of its own.

## Hardware checklist

Field results on a Waveshare 1.75C with a real Biscuit, 2026-11-17: in Secure mode the Biscuit answered with `encrypt=1;wdbatch=2;nodecfg=2;ch=6;ap6=1` and was not joined; switched to Extended mode it was joined within half a second, gave role 1 and the channel list `channels=1,...,14,36,...,161;dwell=110` during a wardrive, and Wi-Fi records were sent with no failures and no link losses over 20 minutes. Every unexpected exit in that run was a side-button double press, which this version no longer counts. In two later listens of 10 and 15 minutes during a running wardrive the link never dropped, and the Biscuit answered every one of 15 questions with the role again, about every 99 seconds. Later that day, on a build of this version that did not yet act on the Biscuit's stop message: stopping a wardrive in the app made the Biscuit send message type 8, with no text, which it never sent while the wardrive ran; after it the Biscuit gave no role, and the node went back to waiting 84 seconds later by the two-round rule. A node that then joined the Biscuit with no wardrive running waited for 3 minutes without a single scan. Earlier the same afternoon the Biscuit had given its role at once to a node that joined while the app showed no wardrive; a start and stop in the app ended that. Quick BOOT presses on the 1.75C were logged and ignored. The rest of the list, the immediate stop on message type 8, a wardrive started while the node waits, and the hold-to-leave rules of this version, are not yet checked on hardware. The fuller per-board list is in [validation status](VALIDATION.md#biscuit-node-and-jcmk-node).

- [x] BISCUIT NODE with a Biscuit in Extended mode (1.75C): linked, role and channel list received, Wi-Fi records sent. Not yet checked: records in the Biscuit's saved log with its location.
- [x] BISCUIT NODE joining a Biscuit with no wardrive running (1.75C): WAITING FOR WARDRIVE, no role and no scans for 3 minutes.
- [x] Stopping a wardrive in the app (1.75C): the Biscuit sends message type 8 and gives no role after it; the node goes back to waiting and `scans=` stops rising. Not yet checked: the stop at once on that message (this version), rather than by the two-round rule.
- [ ] Starting a Wi-Fi wardrive in the app while the node waits starts the scans.
- [x] BISCUIT NODE with a Biscuit in Secure mode (1.75C): no link, and `other=BISCUIT SECURE`.
- [ ] Side buttons on each board: short presses and quick double presses never leave; the documented hold, then CONFIRM, does.
- [ ] JCMK Node with an ESP32 Dual Band Wardriver core: LINKED TO CORE; the core's Nodes menu lists the node as 6G EXT; `assigned=0` and `plan=1,2,3,4,5,6,7,8,9,10,11`; the core's other nodes keep their full channel shares; Wi-Fi and Bluetooth records reach the core's log.
- [ ] JCMK Node with a matching key, with a wrong key, and with `node key ""` against a core with Use Encryption ticked and no ENOW Key; `node config` never prints the key.
- [ ] JCMK Node with a Piglet: LINKED TO PIGLET CORE, Wi-Fi records arrive, and no Bluetooth rows appear in the Piglet's log. With encryption on: PIGLET CORE: CLEAR NODE KEY, and no link.
- [ ] Switch the core off: SEARCHING within about 30 seconds. Switch it on: linked again, and records resume. Restart a Biscuit while linked: the node joins it again within about 90 seconds.
- [ ] Leave by touch, by the documented holds and with `node stop`. One press, a quick double press (the M5Stack Dial's front button included) and a knob click do not leave. The radar, alerts and audio work again afterwards.
- [ ] Unplug power while a node runs: the node resumes at boot.
- [ ] A one-hour drive on each board, including M5Stack Dial and Waveshare Lite: the screen keeps updating, and `node status` memory stays steady.

## Credits

BISCUIT NODE and JCMK Node are original Dragon Detector code, not official firmware of the Biscuit, JustCallMeKoko or Piglet projects, and imply no endorsement. The JCMK message layout, numbering, channel table, timing, external-node capability text and key derivation follow the MIT-licensed ESP32DualBandWardriver by Just Call Me Koko ([licence](licenses/ESP32DualBandWardriver-LICENSE.txt)). The Biscuit handshake and the way a Piglet core is recognised were learned from reading Piglet's published source and reimplemented independently; no Piglet code or text is included. The logos are owner-supplied artwork. See [credits](../CREDITS.md#biscuit-node-and-jcmk-node).
