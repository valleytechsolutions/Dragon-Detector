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

## Experimental CSI signal monitor

Select a fresh AP, place both endpoints still, and Start from Radio tools. CSI accepts only that source's compatible LLTF 128-byte IQ layout, fixing the PHY mode/bandwidth from the first eligible packet, discarding the first two complex pairs consistently. It normalizes amplitude to remove uniform gain changes and compares successive subcarrier shapes. Only a fixed callback queue is filled in the Wi-Fi task; analysis runs in the application loop.

Calibration lasts 30 seconds and requires at least 20 usable one-second windows. Windows need at least five samples/second, recent data, low queue loss and stable RSSI. Insufficient data reports LOW DATA, never STABLE. The displayed change score is relative to that local baseline; it is not calibrated probability, an occupancy guarantee, a person detector or a location estimate. Recalibrate after moving the detector/AP. Walking/still ground-truth validation remains installation dependent.

The monitor retains the last 120 one-second summary points for CSV download. No raw CSI or nearby credentials are stored. Stop/Home and the 15-minute ceiling release the radio. USB: `csi start`, `csi status`, `csi calibrate`, `csi history`, `csi stop`.

## Connected network · 0.11.1

Open **Tools → Recon → Radio toolkit → Connected network**, or expand **Connected network** in the web Radio tools. Add an SSID and password to one of eight slots over USB, then explicitly join that slot. Open networks and WPA2/WPA3 personal networks are supported by the underlying station driver; enterprise authentication and WEP are outside this profile. WPA3 peer interoperability has not been exercised in this release.

Profiles are stored in a separate NVS namespace with integrity checks. Passwords are not returned by list/status commands or stored by the webpage. NVS is not encrypted: use a lab credential if physical extraction is a concern. An existing slot is replaced only by Save profile; Delete removes only that slot. There is no automatic connection on boot.

The shared radio pauses its radar schedule while joined. The page shows DHCP address, subnet mask, gateway, primary DNS, channel, signal and scan progress. Home, Disconnect, loss of connection, a 25-second failed-join deadline (at most three attempts), or a 15-minute session deadline release the station interface and restore the previous radar state. Other lab operations cannot take over during this session.

Gateway ping sends one ICMP echo. Scan local /24 checks the joined /24 (or narrower subnet), skips the detector, and sends no more than four requests per second, with an 800 ms response window. An explicit USB range may contain up to 254 addresses within the joined subnet. A missing reply does not establish that a host is offline. Replies must match the sender, identifier, sequence, random nonce, payload length and checksums. Scan stop retains results; save or clear them before another scan. CSV includes responding addresses and round-trip times, plus sent/no-reply/error totals when saved on the detector. Saved CSV uses the existing capture library and checked USB download.

USB commands: `net profiles`, `net status`, `net connect <slot 0..7>`, `net disconnect`, `net ping gateway`, `net scan local`, `net scan <first IPv4> <last IPv4>`, `net scan stop`, `net results`, `net save`, `net clear`, `net profile remove <slot>`. The browser encodes new profiles as `net profile put <slot> <SSID hex> <password hex or ->`; credentials never appear in the response.
