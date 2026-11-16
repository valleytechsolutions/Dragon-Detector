> **0.13.0:** All four full boards now expose this toolkit. **Tools → Recon → Attacks** opens the Wi-Fi and BLE lists; **Radio tools** opens surveys and captures. [New sessions, archives, recipes, HID and bridge workflows](RECON-013.md). Lite remains radar-only.

> **0.12.0:** Normal Hunt keeps both radios scanning, with complete Wi-Fi sweeps and additional target dwell. The toolkit now has 29 tools; new iBeacon, Eddystone UID/telemetry, BTHome and custom-payload workflows are documented in [Recon Notebook](RECON-NOTEBOOK.md). The older release details below describe the original 24-tool set.

# Waveshare modes and radio toolkit · 0.11.1

Available on **ESP32-S3-Touch-AMOLED-1.75C**. Open **Tools → Recon → Modes** or **Radio tools**. Desktop controls live in [Radio tools on the web flasher](https://valleytech-dragon-detector.pages.dev/#radio-tools). The historical Waveshare release notes below are retained for reference.

## Listening modes

| Mode | Radio / display | Audio focus |
|---|---|---|
| Dragon radar | Balanced BLE/Wi-Fi; configured detector families | Saved preference |
| Axon watch | Balanced passive BLE/Wi-Fi; Axon/TASER evidence | Axon double heartbeat / lost cue |
| ALPR watch | Balanced passive BLE/Wi-Fi; Flock, Raven and ALPR vendor evidence | Strongest eligible Flock/ALPR/Raven |
| Sky watch | Balanced passive BLE/Wi-Fi; Remote ID | Drone candidates |
| Wi-Fi survey | Continuous passive Wi-Fi, all contacts; channel hopping or manual lock | Saved preference |
| BLE survey | Continuous passive BLE, all advertisements | Saved preference |
| Wi-Fi capture | Continuous Wi-Fi; recording is separately started | Saved preference |
| BLE capture | Continuous passive BLE; recording is separately started | Saved preference |

The chosen listening mode survives reboot. It does not change the saved family mask, sound focus, theme, badge or display timers. Quiet mode and disabled sound remain respected. Focused modes hide unrelated contacts and suppress unrelated automatic alerts; underlying evidence still enters history. Watch-list alerts retain priority. Manual Hunt takes precedence over a mode's radio schedule and audio focus. Dedicated modes force passive BLE; returning to Dragon radar restores the saved active/passive preference.

Modes switch without reflashing or rebooting. The ESP32-S3 has one shared 2.4 GHz radio, so balanced scanning alternates BLE and Wi-Fi. Continuous Wi-Fi/BLE modes intentionally give up coverage of the other radio. RSSI remains relative proximity, with decorative radar bearings.

## Inventory and target selection

**Survey / select network** keeps up to 48 APs, including a complete bounded beacon snapshot, and 96 AP/client relationships learned from ordinary ToDS/FromDS data headers. Select a fresh AP, then an observed client if the tool needs one. Multicast and WDS headers cannot become selectable clients. Hidden SSIDs are displayed explicitly. AP and client selections are rechecked against a 30-second freshness window; a channel change requires reselection. USB selection uses the observed MAC and channel, not a recyclable list index.

## Timed transmission tools

Review the actual selected target, channel, duration and rate before Start. Sessions are limited to 1–60 seconds, default 15. Requested Wi-Fi pacing is 1–100 frames/second, default 10. BLE payload updates are capped at 10/second; controller advertisement cadence is distinct from payload-update count. These are testing tools for equipment and networks you are authorized to assess.

| Wi-Fi tools | Requirements / meaning |
|---|---|
| Named beacon, random SSID, SSID playlist, Rickroll and funny playlists | Local test advertisements; a saved playlist holds up to 32 unique printable SSIDs |
| Clone full AP beacon; AP clone with random BSSIDs | A fresh, complete beacon; preserves captured IEs |
| Probe requests | Configured lab SSID / channel |
| Authentication requests | Selected AP |
| Deauthentication; disassociation | Selected AP; optional individually observed client |
| Active EAPOL capture | Selected AP; records matching EAPOL frames while sending deauthentication |
| Bad EAPOL message; association sleep | Selected AP and recently observed client |
| SAE commit | Selected AP; generated group-19 scalar/point; bounded anti-clogging token handling |
| Channel switch announcement; quiet announcement | Fresh full beacon; CSA destination must differ from source channel |

BLE tools generate Swift Pair, Google Fast Pair, Samsung pairing, Flipper, Apple Juice, Sour Apple and mixed pairing advertisements. They do not connect to a selected BLE device. A nearby Flipper does not make all advertisement formats target that Flipper. GATT connections to a specific device remain in [BLE Lab](BLE-LAB.md).

**Stop**, **Home** and timeout end transmission and restore the previous listening state. Tests never start automatically at boot. GATT work, capture saves and timed tests exclude one another. Driver errors stop the run. Test traffic queued during a run is discarded before normal radar processing resumes.

Counters distinguish driver-accepted transmissions, driver errors, observed authentication replies and observed EAPOL frames. Acceptance does not establish reception, client disconnection, successful pairing, vulnerability or complete handshake capture. Protected Management Frames and peer firmware can prevent an effect. This is an independent toolkit, **not complete Marauder parity**. The portal/probe-SSID lab and BLE replay/sound requests added in 0.11 have the explicit limits below and in [BLE Lab](BLE-LAB.md). See the [feature matrix](FEATURE-MATRIX.md) for remaining gaps. No GPS, 5/6 GHz Wi-Fi, Classic Bluetooth or connected BLE packet sniffing is provided by this hardware/build.

The Waveshare raw-frame adapter is pinned to ESP-IDF 5.1.4 and fails compilation when that SDK changes. Revalidate it with an independent receiver before claiming effects on a new driver or target. The other board builds do not link this adapter.

SSID playlists are saved atomically under `/lists` on the recon volume. Use the web controls to add/show/clear entries, configure a local beacon name/channel, inspect the current target, and set the CSA destination. Duplicate SSIDs are not added twice.

## Capture studio

Start Wi-Fi or BLE recording explicitly. The 2 MB PSRAM buffer stops when full or after 15 minutes; it never overwrites its earliest records. Stop, then **Save** to retain the recording on the upper recon filesystem. Unsaved data requires an explicit Discard before another recording can replace it. Saves use incremental writes and a temporary file followed by rename. Disk-full/write failures retain the RAM recording and do not format storage or delete earlier files.

Wi-Fi files are Radiotap PCAP, linktype 127, with receive channel, RSSI and a 1024-byte frame snaplen. FCS bytes are removed; original frame length is retained. Timestamps are monotonic time since boot, not geographic or UTC evidence. Only received management and data frames are included. BLE files are CSV with uptime, address/address type, RSSI, advertisement/total lengths and raw payload hex. Scan responses, when present, remain separated by the recorded boundary. They are advertisement records, not raw link-layer captures.

Files live in `/captures` on the existing upper 16 MB recon volume. GATT profiles and events keep their own directories; badge/history remain in the lower filesystem. Captures are not continuously written to flash. Download through the web flasher; the browser verifies byte count, chunk offsets and CRC32. The library has 32 capture slots; a full library asks you to download and delete older files. Both the device and browser can delete one explicitly selected saved file. A mount failure never causes automatic formatting.

## USB commands

`mode list`, `mode status`, `mode set 0..7`, `screen modes`, `screen tools`.

`tool list`, `tool aps`, `tool ap AA:BB:CC:DD:EE:FF CHANNEL`, `tool clients`, `tool client AA:BB:CC:DD:EE:FF`, `tool client clear`, `tool target`, `tool target clear`, `tool seconds N`, `tool rate N`, `tool channel N` (local-source tests), `tool destination N`, `tool name NAME`, `tool ssid add NAME`, `tool ssid list`, `tool ssid clear`, `tool start ID`, `tool status`, `tool stop`.

`recon capture wifi|ble|stop|save|discard`, `recon files`, `recon get FILENAME`, `recon remove FILENAME`. Use returned filenames. Export emits a bounded `RECON_FILE` header, ordered hex chunks and a CRC-checked `RECON_END`. It pauses reception during the transfer and restores the prior state afterwards.

## Credits and validation

Mode/workflow inspiration: [Colonel Panic's Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue). Packet-builder origins: Valleytech HackerPhone, independently implemented from [JustCallMeKoko's MIT Marauder](https://github.com/justcallmekoko/ESP32Marauder) protocol references. This release keeps MIT source licensing and the upstream notices; it does not copy Unified Blue/ChimeraBLE GPL code. [Full credits](../CREDITS.md) · [Measured validation](VALIDATION.md).

## 0.11 analysis and filtered capture

Traffic statistics count observed management/data frames, beacons, probes, disconnects, protected frames, retries, Pwnagotchi peers and recognized device signatures. EAPOL key-message candidates M1–M4, unencrypted PMKID KDEs and SAE authentication commits have separate counters. Counts do not establish a complete handshake, crackability or successful attack. Export PCAP for protocol analysis.

The 96-row probe inventory keys on the exact source address and raw SSID bytes; embedded NUL/non-text bytes do not merge into a different name. Web CSV preserves raw SSID hex and escapes spreadsheet formulas. A requested network name does not identify the sender.

Capture filters: all frames, beacons/responses, probes, deauth/disassociation, EAPOL/PMKID, Pwnagotchi peers, detected families, or authentication/SAE. Selected-AP capture requires a fresh selection and locks its channel. Filter/scope changes are rejected during recording. Continuous survey/capture can use an 11-bit channel mask and 80–2000 ms dwell; default 1–11/180 ms and preset 1/6/11/350 ms. This does not alter the balanced radar schedule. Capture Start remembers the prior mode; Stop/full/timeout restores it without persisting the temporary capture mode.

USB: `recon stats`, `recon stats clear`, `recon probes`, `recon filter`, `recon filter KIND SELECTED_AP`, `recon hop MASK DWELL_MS`.

## Portal / probe-SSID lab

A local, open AP serves captive DNS and an HTTP template for an explicit 1–60 second session. Default SSID is Dragon-Lab; the default page identifies itself as a security demonstration and asks for invented test values. Choose a name directly or select an exact, fresh observed probe's address/raw SSID pair. Selection does not start transmission; it is rechecked before copying the name. This is one selected SSID per run, not an automatic all-SSID responder or Internet proxy.

Upload an optional UTF-8 HTML template (1–16,384 bytes, no NUL) via CRC-checked USB. The template is stored atomically in `/portal` on the upper recon filesystem. Invalid saved CRC prevents Start until reset/reimport. POST bodies are limited to 511 bytes; at most 32 submissions are retained in RAM, with dropped-record counters. Stop, then save a hex-encoded CSV to `/captures`. Unsaved submissions block a new run until saved or explicitly cleared. Requests and submissions are not automatically posted elsewhere. Download/delete saved logs through Capture studio.

USB: `portal name NAME`, `portal probes`, `portal probe MAC SSID_HEX`, `portal start`, `portal status`, `portal stop`, `portal log`, `portal save`, `portal clear`, `portal html reset`; browser handles binary template import. Stop/Home/timeout closes the AP/server and restores prior reception. There is no automatic deauthentication step.

## Room motion (experimental CSI monitor)

Senses movement near this detector while it sits still. Not people, position or count.

Open **Radio tools → Room motion**. Select a fresh AP, set the detector down where it will stay, keep it still and start. The monitor listens passively to that one AP on its channel and pauses the radar's channel hopping until Stop, Home or the 15-minute ceiling; Wi-Fi detections on the monitored channel are still processed. It sends nothing and does not join the network.

**What it reads.** Only the selected AP's legacy-OFDM or HT frames that give a 128-byte LLTF block, with the PHY mode and bandwidth fixed by the first eligible frame. DC, guard tones and tone 1 (it shares the driver's possibly invalid first word) are dropped, leaving 51 of the 64 tones (LLTF 2..26 and -26..-1). Each one-second window needs at least five usable frames. A single packet that differs sharply from the recent ones (Hampel test on shape decorrelation, nine-packet window) is skipped once; a second in a row is accepted as a real change.

**Model.** The window features, calibration and fusion are ported from ESP32 Marauder's `CsiMotionModel` (develop 79cc6bc, MIT; see [credits](../CREDITS.md)): amplitude spread per tone, spread of shape decorrelation, of linear-detrended phase change and of tone-to-tone amplitude change. Calibration takes 30 seconds and at least 15 usable windows; each feature is scaled by its calibration median and MAD and the four are fused by their middle two. Marauder takes the 85th percentile of calibration windows as the limit; here the limit is 2.5 times that (at least z 2), because the original crosses about one still window in seven; on synthetic still CSI it entered MOTION about 10 to 30 times an hour. With the change, host simulation gave 0 and 1 MOTION entries in 1000 still minutes at two noise levels and 4 in 500 minutes at a higher one. The score shown is smoothed as in Marauder; MOTION decisions use the unsmoothed window score. These numbers come from synthetic data, not from rooms.

**States.**

| State | Meaning |
| --- | --- |
| CALIBRATING | First 30 seconds after Start, Recalibrate or a detector-moved event. Keep still. |
| LOW DATA | Fewer than five usable frames per second, poor reliability, or calibration still short of 15 windows. Never reported as STILL. |
| STILL | Calibrated, data flowing, no motion in the last 15 seconds. |
| MOTION | Two windows in a row over the limit; leaves after two windows under it (three when windows hold fewer than ten samples). Motion that returns within HOLD continues the same logged interval. |
| HOLD | Up to 15 seconds after the last motion window. |

**Margin.** `limit − highest score of the last 30 non-motion windows` (a lone crossing counts as noise). A large positive margin means the placement is quiet; near zero or negative means false alarms are likely: move the detector or choose another AP and recalibrate. Walk past once to see the motion peak in the history.

**Why LOW DATA.** The monitor counts the selected AP's frames by PHY class, both from the normal capture path and the CSI callback. Beacons are usually sent at 1 Mb/s DSSS, which carries no LLTF, so an idle AP may give nothing usable. The screen then says *This network's beacons carry no motion data; choose another network*, *Selected network not heard here; choose another network*, or *Too little OFDM traffic from this network*. The next step if passive sourcing starves is an active link (joining the AP or sending paced frames to it to make it answer with OFDM); that is not implemented.

**Detector moved or signal changed.** There is no motion sensor in the firmware, so the radio's own evidence is used: three windows in a row whose mean RSSI is 10 dB or more from the calibrated RSSI, or with a spread jump on 90% of tones (four times their calibration spread) and a 5 dB RSSI shift (such windows are not scored); a 10 dB RSSI range between the 10th and 90th percentile of the calibration windows; or MOTION/HOLD that lasts 30 windows with RSSI 4 dB or more off the calibrated level, as when the AP lowers its power or the noise floor rises (in simulation that otherwise held MOTION in an empty room for good). Any of these shows *Signal changed (detector moved or something close); recalibrating* and starts a new calibration instead of reporting motion; the interval is logged as `detector_moved`. A LOW DATA reason takes precedence on screen. It is a heuristic: a person very close to the detector or the AP can trigger it (and real motion during the new calibration is absorbed into it), and a gentle move may not, in which case motion is reported until you recalibrate. A persistent gain step under 4 dB can still give occasional false MOTION. Recalibrate ends an open motion interval.

**Limits.** It needs a still detector and a still AP, and an AP that sends OFDM traffic. It senses changes in the radio path between them, which includes movement in adjacent rooms, fans, curtains, pets, doors and other radios; it can stay STILL while someone sits motionless. It makes no through-wall, occupancy, breathing, identity, position or count claims. No popup, sound, vibration or alert is raised; MOTION shows only on this screen and over USB. Ground-truth walking/still validation on hardware has not been done.

**USB.** `csi start`, `csi stop`, `csi calibrate`, `csi status`, `csi history`. `csi status` keeps its `RECON_CSI` line (state values are now STILL / MOTION / HOLD / LOW DATA / CALIBRATING; `score` is the smoothed motion score, `quality` the reliability, `baseline` the calibration windows, `variation` the fused z and `threshold` the limit z) and adds a `RECON_CSI_ROOM` line with the window score, limit, margin, motion intervals, moved count, skipped packets, tone count, feature scores, supply diagnosis and per-class frame counts. `csi history` keeps the 120 one-second points, appends `state` and `margin` columns, then lists the last 12 motion intervals as `RECON_CSI_INTERVAL start_ms,end_ms,peak_score,status` (`open`, `ended` or `detector_moved`). No raw CSI or nearby credentials are stored.

## Connected network · 0.11.1

Open **Tools → Recon → Radio toolkit → Connected network**, or expand **Connected network** in the web Radio tools. Add an SSID and password to one of eight slots over USB, then explicitly join that slot. Open networks and WPA2/WPA3 personal networks are supported by the underlying station driver; enterprise authentication and WEP are outside this profile. WPA3 peer interoperability has not been exercised in this release.

Profiles are stored in a separate NVS namespace with integrity checks. Passwords are not returned by list/status commands or stored by the webpage. NVS is not encrypted: use a lab credential if physical extraction is a concern. An existing slot is replaced only by Save profile; Delete removes only that slot. There is no automatic connection on boot.

The shared radio pauses its radar schedule while joined. The page shows DHCP address, subnet mask, gateway, primary DNS, channel, signal and scan progress. Home, Disconnect, loss of connection, a 25-second failed-join deadline (at most three attempts), or a 15-minute session deadline release the station interface and restore the previous radar state. Other lab operations cannot take over during this session.

Gateway ping sends one ICMP echo. Scan local /24 checks the joined /24 (or narrower subnet), skips the detector, and sends no more than four requests per second, with an 800 ms response window. An explicit USB range may contain up to 254 addresses within the joined subnet. A missing reply does not establish that a host is offline. Replies must match the sender, identifier, sequence, random nonce, payload length and checksums. Scan stop retains results; save or clear them before another scan. CSV includes responding addresses and round-trip times, plus sent/no-reply/error totals when saved on the detector. Saved CSV uses the existing capture library and checked USB download.

USB commands: `net profiles`, `net status`, `net connect <slot 0..7>`, `net disconnect`, `net ping gateway`, `net scan local`, `net scan <first IPv4> <last IPv4>`, `net scan stop`, `net results`, `net save`, `net clear`, `net profile remove <slot>`. The browser encodes new profiles as `net profile put <slot> <SSID hex> <password hex or ->`; credentials never appear in the response.
