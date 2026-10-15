# Waveshare modes and radio toolkit · 0.10.0

Available on **ESP32-S3-Touch-AMOLED-1.75C**. Open **Tools → Recon → Modes** or **Radio tools**. Desktop controls live in [Radio tools on the web flasher](https://valleytechsolutions.github.io/Dragon-Detector/#radio-tools). Other boards retain their existing detection, badge and daily-tool capabilities.

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

Counters distinguish driver-accepted transmissions, driver errors, observed authentication replies and observed EAPOL frames. Acceptance does not establish reception, client disconnection, successful pairing, vulnerability or complete handshake capture. Protected Management Frames and peer firmware can prevent an effect. This is an independent toolkit, **not complete Marauder parity**: Evil Portal, Karma, AirTag identity spoofing, Find My sound and GPS/wardriving workflows are not included in this release. No GPS, 5/6 GHz Wi-Fi, Classic Bluetooth or connected BLE packet sniffing is provided by this hardware/build.

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
