# Detection and tracking evidence

The receiver observes advertisements and 802.11 traffic. It does not attack, pair with, query, jam or identify a device owner. Names and protocol signatures can be imitated. "Candidate" is a description of observed evidence, not an accusation.

| Family | Evidence used | Assessment / limits |
|---|---|---|
| Flock / possible ALPR infrastructure | Flock accessory 128-bit service `e8ccbb38-9532-46a8-9fe5-1814df172e6f`; Xuntong `0x09C8` plus exact Penguin serial, bare serial or FS Ext Battery name; transmitter OUI plus strict wildcard probe IE sequence | Service may identify an accessory rather than a camera. Matching probe sequence remains spoofable. |
| Flock research OUIs | 34-prefix union from Flock Noir / Colonel Panic Flock-You | Broad chipset/vendor hints: low confidence, excluded from default candidate radar unless accompanied by stronger evidence. A wildcard probe by itself is insufficient. |
| Axon | TASER company `0x034D`; Axon services `0xFC81`, `0xFE6B`, `0xFE6C`; public OUI `00:25:DF`; names | Company or service: medium; both: strong signature. Does not confirm body camera model or recording. Public OUI alone remains weak. |
| Apple Find My / possible AirTag | Company `0x004C`, type `0x12`, declared length `0x19` and at least 29 manufacturer bytes; service `0xFD44` | Tracker-family candidate; not necessarily AirTag, and not proof of unwanted tracking. Ordinary Apple popup/nearby advertisements are not classified as AirTags. |
| Samsung SmartTag | 16-bit service `0xFD5A` | SmartTag-family candidate. Samsung company ID alone does not match. No owner, distance or model inference. |
| DULT-compatible trackers | Previously supported 128-bit tracker service | Tracker-family candidate; implementation is not a certified unwanted-tracker safety system. |
| Flipper Zero | Service `0x3080`–`0x3083`; appearance `0x8600` corroboration; name | Service alone is weak; service + appearance is medium. No inference about what its operator is doing. |
| Biscuit / Pro / Ultra | Biscuit-prefixed local name, optionally with `4fafc201-1fb5-459e-8fcc-c5c9c331914b` | Name alone weak; name + UUID medium. This UUID is also an Arduino example and **never matches by itself**. Pro/Ultra cannot be confirmed. |
| Nyanbox, HaleHound, Pineapple, Pwnagotchi, skimmer-like modules | Named candidates; inherited Marauder Pwnagotchi peer MAC + payload fields | Most names are weak/spoofable. HC-05/06/08 also appear on benign serial modules. A probe requesting an AP name does not identify its sender as that AP. |
| Meta glasses | Luxottica `0x0D53` + Meta `0xFD5F`/related service, or weak advertised-name hint | The combined evidence is medium; a Meta service alone is not enough. Recording state unknown. |
| Wi-Fi disconnect / BLE pairing bursts | Repeated traffic in bounded time windows, with retry suppression and distinct-address counting | Traffic pattern, not proven malicious intent. Busy spaces can trigger alerts. |

## Default radar and freshness

The radar requires at least two matching observations spread across 400 ms and matching evidence within 30 seconds. Generic Wi-Fi/BLE and OUI-only candidates remain in the contact lists. Settings can show all contacts. Medium/strong candidate alerts also wait for repeat evidence. A weaker later observation cannot refresh a stronger classification. Old evidence expires even if unrelated traffic from the address continues.

BLE OUI lookup applies only to public addresses. Random and rotating addresses remain separate contacts. There is no reliable cross-address correlation, owner identification, following-person inference, or MAC de-randomization. Wi-Fi receive/transmitter roles are not collapsed into a single claimed device identity.

## Tracking

Tap a contact → **Track signal**. BLE tracking reserves the BLE listening phase for continuous passive scans; Wi-Fi tracking locks to the observed channel. Other radio coverage is reduced during tracking. Settings → Tracking returns to balanced scanning.

Signal estimation uses a five-sample median followed by an elapsed-time exponential filter (1.8-second time constant). Trends compare filtered signal every four seconds with a 3 dB threshold. A 15-second gap resets the filter, and a 10-second absence marks the selected target stale. Target slots are retained so a crowded scan cannot silently replace the selected device. Angle remains an address-derived decoration; radius remains a signal-strength mapping. Neither is physical localization.

Balanced mode alternates 3.3 seconds Wi-Fi and 3.3 seconds BLE, with 350 ms Wi-Fi channel dwell and a requested 85/100 ms BLE scan window. Country/channel plan is US channels 1–11. Listening gaps, antenna orientation, body shielding, interference, transmit power and crowded traffic affect results. A 96-entry packet queue reduces burst loss but does not guarantee lossless capture. Counters remain visible in serial status.

## Sources

- [Flock Noir source snapshot](https://github.com/valleytechsolutions/Flock-Noir/blob/0559417e4f5e96f615ca4c4b42c3e0f0d56e2337/firmware/FlockNoir/radio_protocol.h).
- [Colonel Panic's OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue) and [Flock-You](https://github.com/colonelpanichacks/flock-you).
- [Biscuit GATT reference](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html) and [Arduino BLE example](https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/examples/Server/Server.ino).
- [Samsung SmartTag original research](https://github.com/pogen300/galaxy_smarttag_research).
- [Flipper serial profile](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/f7/ble_glue/profiles/serial_profile.c).
- [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder). Its MIT notice is preserved under [licenses](licenses/MARAUDER-LICENSE.txt).

These are reference signatures, not a field-validated accuracy claim. See [VALIDATION.md](VALIDATION.md).
