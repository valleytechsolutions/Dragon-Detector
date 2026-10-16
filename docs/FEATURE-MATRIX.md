# Reference feature comparison · 0.11.1

This is a feature audit, not a claim that Dragon Detector is an official port or has identical behavior. The original application remains MIT. Reference firmware is not silently linked into it. [Credits and exact source revisions](../CREDITS.md) · [Measured hardware checks](VALIDATION.md).

## Marauder: Waveshare AMOLED implementation

| Reference workflow | Dragon Detector entry / extent |
|---|---|
| AP, station and combined surveys | Radio tools → Survey / select network; live AP/client tables and explicit fresh selection |
| Beacon, probe, raw, deauth, EAPOL/PMKID, SAE and Pwnagotchi sniffing | Capture filters; Radiotap PCAP; traffic/message counters; probe inventory |
| Packet/channel analyzer | Existing Activity and Channel monitor; Wi-Fi statistics |
| Signal monitor / fox hunt / MAC tracking | Existing per-contact Hunt, relative RSSI trace, peak/trend and watch list |
| Flock, Flipper, Meta, AirTag, iBeacon and Remote ID scanners | Shared radar classifiers, family filters, focused modes and history; evidence quality is shown |
| Beacon lists/random/Rickroll, AP clone, probe/authentication, deauth/disassociation | Existing 24-tool timed transmitter, selected targets and bounded rates |
| Bad message, association sleep, SAE commit, CSA, quiet | Existing bounded builders and selected-AP/client workflows; driver acceptance is not proof of effect |
| Evil Portal / KARMA | Local portal, captive DNS, optional HTML, explicit recent probe-SSID selection, bounded submission log; no Internet proxy or automatic disconnect step |
| Apple/Samsung/Google/Windows/Flipper/mixed BLE advertisement tests | Existing timed BLE transmitter modes |
| AirTag identity replay | BLE Lab legacy advertisement replay; optional random-static identity only; no keys or rotating-identity cryptography |
| Find My sound | Explicit connected AirTag/Find My/DULT characteristic write; peer may reject it |
| CSI motion monitor | Experimental independent amplitude-shape monitor; calibrated signal-change score, low-data state and 120-point CSV. No phase-fusion equivalence or occupancy guarantee |
| Save/load SSIDs, custom HTML, capture browser | Recon flash playlist/template/capture library; checked browser download and individual deletion |
| Recon missions / desktop tool | Dragon history, PCAP/BLE CSV, probe CSV, GATT profiles/events. Marauder `.rlog` mission directories and its desktop format are not implemented |
| Join / saved network profiles / IP ping scan | Connected network: eight explicit profiles, DHCP/IP/gateway/DNS, gateway ping, at most 254 local-subnet addresses, CSV; no automatic reconnection |
| Save/load AP lists, arbitrary source-MAC configuration, command scripts | Not implemented; live AP selections expire and cannot be loaded as fresh observations |
| USB BadUSB / DuckyScript | Not implemented in this USB-CDC stopwatch build |
| GPS wardriving / WiGLE / NMEA | No supported local GPS in this board profile; broadcast Remote ID coordinates are different data |
| 5/6 GHz, Bluetooth Classic, external NRF24 | Not provided by the ESP32-S3 radio / current board profile |

## ChimeraBLE: independently implemented BLE Lab

| Workflow | Status / boundary |
|---|---|
| Advertisement inspection, address types, RSSI and protocol hints | Available; common assigned UUID/company labels, not a complete company/OUI database |
| GATT discovery, service/characteristic/descriptor values | Available; 16 services / 64 characteristics / 128 attributes / 512-byte values; partial/error markers |
| Read/write/write-no-response, subscribe/unsubscribe, MTU | Available; requests obey advertised properties and peer access controls |
| Pairing, passkey, numeric comparison, bonds | Explicit opt-in policy and user input; no implicit PIN, automatic unseen approval or key cloning |
| HID Report Map | Report IDs and input/output/feature lengths; malformed/unsupported items flagged. Full usage-tree and semantic live keyboard decoding are not implemented |
| Save/load/list/delete/compare clones | DDG1 profiles with CRC and atomic file saves |
| Peripheral emulation | Application GATT services and initial values, readable descriptors and legacy adverts; local writes/notifications. Proprietary behavior is not recreated |
| Live events and export | Bounded local event ring, rotating CSV logs and checked USB export. Chimera's JSON command/event schema is not implemented |
| Transparent relay / MITM | Not implemented; central and emulator roles are mutually exclusive |

## SquachWatch and Unified Blue detection

All **158 table facts** from the pinned SquachWatch revision have parser and radar-visibility fixtures: 110 address patterns, 25 service IDs, eight company IDs, seven names and eight SSID prefixes. Runtime fixtures also cover BWCDEVICE/BodyWorn, Flock names/composites, Apple Find My/pairing forms, iBeacon, Remote ID and traffic anomalies. [Ledger and evidence limits](COVERAGE-AUDIT.md).

AXEOFF's company/service/address/UUID compatibility paths, priority receive capacity, first-hit alert treatment and distinctive hunt audio remain covered by dedicated regression tests. Eight listening modes provide the focused radar/survey workflows inspired by Unified Blue. New body-camera vendor and BWC payload clues also receive the reserved queue path. No signature or OUI proves malicious activity, camera recording, operator identity or a precise model.

SquachWatch pets/games/chat and social systems remain outside the user's detector scope. The five boards share recognition logic; the new BLE/portal/CSI lab runs only on the 32 MB Waveshare AMOLED profile. Firmware builds and signature fixtures do not substitute for a field trial against every physical product.
