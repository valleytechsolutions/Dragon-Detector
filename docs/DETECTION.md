# Detection evidence · 0.5.0

Dragon Detector listens to Wi-Fi frames and BLE advertisements. It does not connect to, pair with, jam or identify a device owner. BLE is passive by default; optional active scanning transmits scan requests to collect advertised response data. Names, MAC addresses and protocol fields can be imitated. Confidence describes the observed signature, never intent. An optional **Dragon fox** training transmitter is explicitly enabled and off at boot.

See the [SquachWatch category comparison](DETECTION-PARITY.md) for coverage, implementation choices and hardware limits, and the [user guide](USER-GUIDE.md) for controls.

| Family | Evidence | Confidence and limits |
|---|---|---|
| Flock | Flock Safety B4:1E:52; accessory UUID e8ccbb38-9532-46a8-9fe5-1814df172e6f; Xuntong 09C8 + strict battery name; exact probe IE order/vendor bytes with research prefix | Registered vendor medium; accessory service strong; composites medium. Shared module OUIs and names weak. Accessory is not a verified camera. Samsung 48:27:EA excluded; 82:6B:F2 is a weak local-address pattern, never vendor attribution. |
| Axon/TASER | 00:25:DF, company 034D, services FC81 / FE6B / FE6C, anchored names | Vendor/company/service medium, company + service strong, names weak. Model and recording unknown. |
| Camera glasses | Luxottica 0D53 + Meta FD5F / FEB7 / FEB8; Snap 03C2 + FE45 | Composite medium; company/OUI/name clues weak; Meta service alone is a weak, unverified clue. |
| Find My / DULT | Apple 004C, type 12, declared length 19, full minimum payload; FD44; DULT UUID | Medium Find My family evidence; Apple pairing-size forms additionally produce weak clues that may be AirPods. Not necessarily AirTag, not a finding of unwanted tracking. |
| Samsung SmartTag | FD5A service | Medium; generic Samsung company ID does not match. |
| Google Find Hub | FEAA service data, frame 40/41, 20/32-byte EID with optional flags | Strong format evidence; generic Eddystone and bare FEAA are weak clues, not established trackers. Legacy scanning can miss extended-only broadcasts. |
| Tile | FEED / FEEC services | Medium vendor-service evidence; owner/model unknown. |
| Raven | Exact 3100, 3200, 3300, 3400, 3500 | Medium research signature, all five together strong; adjacent/range values excluded. Physical Raven validation pending. |
| Ring | Ring LLC registered prefixes | Medium vendor evidence; shared Amazon blocks weak. Not every Ring product is a camera. |
| ALPR vendors | Genetec and Motorola Solutions prefixes | Genetec medium; broad Motorola Solutions weak. Product unverified; Sonos 00:0E:58 excluded. |
| Camera vendors | Wyze/Hikvision/Verkada/Avigilon/Axis/Arlo/Blink assignments | Medium vendor evidence. Shared Amazon/Telink/Murata/Espressif/Realtek weak. A module/vendor is not a confirmed covert camera. |
| Serial/skimmer-like modules | HC-03/05/06/08, RN42, BT04-A, serial service and module prefix | Weak only; widely used benign modules. ESP32-S3 cannot hear Bluetooth Classic-only devices. |
| Flipper | Company 0E29, registered 0C:FA:22, services 3080–3083 and appearance 8600 | Company/vendor or corroborated service medium; name/service alone weak. No activity inference. |
| Biscuit / Pro / Ultra | Biscuit-prefixed name with optional 4fafc201-1fb5-459e-8fcc-c5c9c331914b | Name weak; name + service medium. Shared Arduino UUID alone excluded. Models not reliably distinguished. |
| Pwnagotchi | Peer name and handshake-counter fields, optional DE:AD:BE:EF:DE:AD | Medium payload evidence; fixed peer MAC composite strong, still spoofable. |
| Pineapple / deauther, Nyanbox, HaleHound | Characteristic advertised names and inherited tool hints | Weak, spoofable; no product/behavior guarantee. |
| Drone Remote ID | BLE FFFA, code 0D + counter + message; Wi-Fi FA0BBC/0D; NAN service descriptor | Strong decoded protocol; bare FFFA service is weak. Bounds checked before Apache decoder. Coordinates/ID self-reported with separate age. No GPS fix for this detector. |
| iBeacon | Full Apple 004C 02 15 payload and exact length | Strong format evidence; common benign beacon. Enabled by default; individually configurable. |
| Wi-Fi disconnect burst | Six deauth/disassociation observations in a rolling three seconds across source addresses | Medium traffic evidence; retries suppressed, source spoofable. |
| SSID conflict | Three observations per BSSID over at least 400 ms, same raw SSID, open/protected mismatch, concurrent freshness | Medium possible evil twin. Multiple BSSIDs or WPA2/WPA3 suite differences alone do not alert; legitimate configurations can trigger. |
| BLE advert burst | 12 distinct pairing/tag addresses within five seconds, or 24 distinct pairing payloads in five seconds | Medium aggregate traffic evidence. Crowds/rotating addresses can trigger; not labeled proven fake devices. |

Identifiers in the table are hexadecimal.

## Freshness and radio identity

Assigned service IDs are parsed from 16-bit, zero-extended 32-bit and exact Bluetooth Base 128-bit service lists/data fields. Custom UUIDs must match their complete identifier to qualify as assigned service evidence. The Axon transport adapter separately exposes AXEOFF's partial-UUID matches as priority detections, retaining their lower specificity in details. Encoding width does not increase confidence or enable reception of extended-only BLE advertisements. See [practical Flipper and Pineapple Pager checks](DEVICE-CHECKS.md).

A first valid signature immediately appears on the radar and in history. Ordinary audible alerts require two matching observations spread over 400 ms. Axon priority (on by default) alerts on every recognized Axon signature at the first observation, including the broad AXEOFF/SquachWatch patterns. Other important families (Flipper, cameras/ALPR, glasses and drones) also alert on first recognized evidence. Quiet, ignore/snooze, enabled families and cooldowns still apply. Matching evidence expires after 30 seconds even if unrelated packets continue. Weaker packets cannot refresh a stronger classification. The general medium-confidence threshold applies to other families. Trackers remain silent dots unless explicitly watched; evidence grades remain available in details. All detector families, including iBeacon, are enabled on fresh installs. Watch rules explicitly select an address of interest, including otherwise unclassified contacts.

Vendor lookup uses public BLE addresses and globally assigned unicast Wi-Fi prefixes. Random/local addresses do not receive OUI attribution. Three literal local Wi-Fi address patterns have separate, explicitly unverified clues. BLE address type is part of the contact key. Rotating addresses stay separate; no owner, identity or following-person inference is made.

Balanced scanning starts with BLE and alternates 2.2 seconds BLE and 2.2 seconds Wi-Fi. Wi-Fi schedules US channels 1–11 at 200 ms dwell; BLE defaults to passive legacy scanning with an 100/100 ms requested window. Optional active mode requests scan responses, clears result storage each second and processes both initial adverts and responses. Dedicated hunt reduces other-radio coverage. Callback queues are bounded, with reserved capacity and priority draining for all recognized Axon matches; USB status reports overall drops and `axon status` reports Axon-specific drops/lag. Silent radios, Bluetooth Classic, extended-only BLE, 5/6 GHz and scan gaps limit coverage.

## Signal estimation

A five-sample median, accepting at most one sample per 100 ms, precedes elapsed-time exponential smoothing (0.6/1.8/4-second constant). Trend compares filtered values over four seconds using a ±3 dB threshold. A 15-second gap resets the estimate; ten-second absence marks a hunt stale. A stale Wi-Fi hunt begins sweeping channels 1–11 after twelve seconds and locks again when its address is heard. The 48-point trace covers 24 seconds with 500 ms buckets and explicit missing-signal gaps. Hunt also shows the most recent raw RSSI alongside the filtered gauge. Peak and saved mark are relative signal references, not range calibration.

## Sources

- [Flock Noir snapshot](https://github.com/valleytechsolutions/Flock-Noir/blob/0559417e4f5e96f615ca4c4b42c3e0f0d56e2337/firmware/FlockNoir/radio_protocol.h), [Colonel Panic / Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue), [Flock-You](https://github.com/colonelpanichacks/flock-you).
- [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder); [MIT notice](licenses/MARAUDER-LICENSE.txt).
- [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD) feature reference; [scope comparison](DETECTION-PARITY.md).
- [Wireshark IEEE-derived registry](https://www.wireshark.org/download/automated/data/manuf); selected [assignment facts](data/vendor-prefixes.csv).
- [Bluetooth SIG assignments](https://www.bluetooth.com/specifications/assigned-numbers/), [Google Find Hub specification](https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn).
- [OpenDroneID core](https://github.com/opendroneid/opendroneid-core-c), [official BLE framing](https://github.com/opendroneid/transmitter-linux/blob/main/bluetooth.c), [vendored provenance](../lib/opendroneid/UPSTREAM.md).
- [Biscuit GATT](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html), [SmartTag research](https://github.com/pogen300/galaxy_smarttag_research), [Flipper](https://github.com/flipperdevices/flipperzero-firmware).

Fixtures and negative tests do not establish field sensitivity/specificity for every product. [Validation status](VALIDATION.md).

## Axon / AXEOFF reference

The passive Axon checks cover AXEOFF's public signature set: Wi-Fi source and public BLE OUI `00:25:DF`, manufacturer company ID `0x034D`, and service UUIDs `0xFC81`, `0xFE6B`, `0xFE6C`. One registered vendor/service clue is medium confidence; the manufacturer-plus-service combination is stronger. BLE random addresses do not receive OUI attribution. Broad AXEOFF partial-UUID/random-address patterns receive the same priority queue and first-hit alert handling while retaining their actual evidence grade. First-hit alerts use the saved Axon priority policy; see [Axon setup, exact evidence and diagnostics](AXON.md). This can indicate other Axon/TASER equipment as well as body cameras; it does not reveal recording state.

Axon detections create the normal radar/history entry and unique configurable RTTTL alert. Audio focus adds a proximity-style double heartbeat and lost-signal cue, independently implemented from [Colonel Panic's AXEOFF workflow](https://github.com/colonelpanichacks/axeoff). Other detector families have separate melodies. The exact waveform and scheduler are original Dragon Detector code.
