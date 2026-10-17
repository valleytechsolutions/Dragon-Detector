# Reference feature comparison · 0.13.9

This is a feature audit, not a claim that Dragon Detector is an official port or has identical behavior. The original application remains MIT. Reference firmware is not silently linked into it. [Credits and exact source revisions](../CREDITS.md) · [Measured hardware checks](VALIDATION.md).

## AMOLED feature parity across boards

The v0.13.9 source audit confirms that Waveshare AMOLED, M5Stack StopWatch, Spotpear and Seeed XIAO Round already share the full UI, 29 themes, 27 selectable logo watch faces, stopwatch/locksport timers, notebook, Hunt, mode launcher and lab workflows below. No additional port is needed to expose those existing features on these four profiles. The physical controls, audio, RTC and SD support follow each board's hardware; capture and session limits are intentionally smaller where memory or storage is smaller.

| Profile | Capture buffer | Recon session limit | Application slot free |
|---|---:|---:|---:|
| Waveshare AMOLED | 2 MiB | 2 MiB | 56.57% |
| M5Stack StopWatch | 1 MiB | 256 KiB | 30.84% |
| Spotpear | 1 MiB | 256 KiB | 58.33% |
| Seeed XIAO Round | 512 KiB | 256 KiB | 22.44% |
| Waveshare Lite | No recorder | No session UI | 71.87% |

Lite shares detection and all 29 BOOT-selectable themes. It adds compact clock, timing, signal and diagnostic pages. Its 2 MiB PSRAM and single BOOT control still exclude the full touch UI, badge, recorder and active labs. All profiles retain the 21% application-slot reserve and managed-storage reserve. [Exact budgets](FLASH-BUDGET.md) · [Hardware capabilities](HARDWARE.md).

## Marauder: all four full boards

| Reference workflow | Dragon Detector entry / extent |
|---|---|
| AP, station and combined surveys | Radio tools → Survey / select network; live AP/client tables and explicit fresh selection |
| Beacon, probe, raw, deauth, EAPOL/PMKID, SAE and Pwnagotchi sniffing | Capture filters; Radiotap PCAP; traffic/message counters; probe inventory |
| Packet/channel analyzer | Existing Activity and Channel monitor; Wi-Fi statistics |
| Signal monitor / fox hunt / MAC tracking | Existing per-contact Hunt, relative RSSI trace, peak/trend and watch list |
| Flock, Flipper, Meta, AirTag, iBeacon and Remote ID scanners | Shared radar classifiers, family filters, focused modes and history; evidence quality is shown |
| Beacon lists/random/Rickroll, AP clone, probe/authentication, deauth/disassociation | Existing 29-tool timed transmitter, selected targets and bounded rates |
| Bad message, association sleep, SAE commit, CSA, quiet | Existing bounded builders and selected-AP/client workflows; driver acceptance is not proof of effect |
| Evil Portal / KARMA | Local portal, captive DNS, optional HTML, explicit recent probe-SSID selection, bounded submission log; no Internet proxy or automatic disconnect step |
| Apple/Samsung/Google/Windows/Flipper/mixed BLE advertisement tests | Existing timed BLE transmitter modes |
| AirTag identity replay | BLE Lab legacy advertisement replay; optional random-static identity only; no keys or rotating-identity cryptography |
| Find My sound | Explicit connected AirTag/Find My/DULT characteristic write; peer may reject it |
| CSI motion monitor | Experimental independent amplitude-shape monitor; calibrated signal-change score, low-data state and 120-point CSV. No phase-fusion equivalence or occupancy guarantee |
| Save/load SSIDs, custom HTML, capture browser | Recon flash playlist/template/capture library; checked browser download and individual deletion |
| Recon missions / desktop tool | Named sessions with independently encoded RCN1/PRB1/REL1 files and checked ZIP download; no GPS fix. External desktop integration is not yet tested |
| Join / saved network profiles / IP ping scan | Connected network: eight explicit profiles, DHCP/IP/gateway/DNS, gateway ping, at most 254 local-subnet addresses, CSV; optional saved-Wi-Fi clock recovery; ordinary reconnection remains explicit |
| Save/load AP lists, arbitrary source-MAC configuration, command scripts | Historical AP CSV save/load/export, volatile source-MAC configuration and reviewed bounded recipes. Archives cannot be loaded as fresh target observations |
| USB BadUSB / DuckyScript | Not implemented in this USB-CDC stopwatch build |
| GPS wardriving / WiGLE / NMEA | No supported local GPS in this board profile; broadcast Remote ID coordinates are different data |
| 5/6 GHz, Bluetooth Classic, external NRF24 | Not provided by the ESP32-S3 radio / current board profile |

## ChimeraBLE: independently implemented BLE Lab

| Workflow | Status / boundary |
|---|---|
| Advertisement inspection, address types, RSSI and protocol hints | Available; common assigned UUID/company labels and 40,333 IEEE MA-L prefixes; not a complete Bluetooth company-ID or MA-M/MA-S database |
| GATT discovery, service/characteristic/descriptor values | Available; 16 services / 64 characteristics / 128 attributes / 512-byte values; partial/error markers |
| Read/write/write-no-response, subscribe/unsubscribe, MTU | Available; requests obey advertised properties and peer access controls |
| Pairing, passkey, numeric comparison, bonds | Explicit opt-in policy and user input; no implicit PIN, automatic unseen approval or key cloning |
| HID Report Map | Bounded field/usage parser and semantic keyboard, mouse and consumer decoding. Unsupported maps/values retain raw evidence; no HID injection |
| Save/load/list/delete/compare clones | DDG1 profiles with CRC and atomic file saves |
| Peripheral emulation | Application GATT services and initial values, readable descriptors and legacy adverts; local writes/notifications. Proprietary behavior is not recreated |
| Live events and export | Bounded local event ring, rotating CSV logs and checked USB export. Chimera's JSON command/event schema is not implemented |
| Transparent relay / MITM | Cached-read GATT bridge forwards writes and notifications; not a transparent ATT proxy, identity clone or pairing bypass |

## SquachWatch and Unified Blue detection

All **158 table facts** from the pinned SquachWatch revision have parser and radar-visibility fixtures: 110 address patterns, 25 service IDs, eight company IDs, seven names and eight SSID prefixes. Runtime fixtures also cover BWCDEVICE/BodyWorn, Flock names/composites, Apple Find My/pairing forms, iBeacon, Remote ID and traffic anomalies. [Ledger and evidence limits](COVERAGE-AUDIT.md).

AXEOFF's company/service/address/UUID compatibility paths, priority receive capacity, first-hit alert treatment and distinctive hunt audio remain covered by dedicated regression tests. Eight listening modes provide the focused radar/survey workflows inspired by Unified Blue. New body-camera vendor and BWC payload clues also receive the reserved queue path. No signature or OUI proves malicious activity, camera recording, operator identity or a precise model.

SquachWatch pets/games/chat and social systems remain outside the user's detector scope. The five boards share recognition logic; the BLE/portal/CSI lab now runs on all four full boards. Firmware builds and signature fixtures do not substitute for a field trial against every physical product.

## Shared features

Waveshare AMOLED, M5Stack StopWatch, Spotpear and XIAO Round share the passive Recon Notebook: full bounded advertisements, common protocol metadata, probe-request inventory, protocol counters and CSV. Hunt now preserves both radios and a complete Wi-Fi sweep, including under delayed rendering. Normal USB badge/history/SD transfers cooperate with detection. All four full boards expose the 29-format toolkit in 0.13. [Details](RECON-NOTEBOOK.md).

These additions do not mean the boards have exhausted useful features or reached 79% occupied physical flash. The [budget report](FLASH-BUDGET.md) separates app slots, OTA allocation and user storage. The unsupported/unimplemented rows above remain explicit work or hardware limits.

The [0.13 guide](RECON-013.md) documents session, recipe, archive, HID and bridge limits. USB BadUSB/DuckyScript and Chimera JSON protocol compatibility remain unimplemented; unsupported radio bands and external GPS require different hardware.

### Clock and Lite expansion · 0.13.9

All four full boards now support opt-in saved-Wi-Fi time recovery. Lite adds a compact themed analog/digital clock, stopwatch/locksport timer, signal inspector and diagnostics, preserving passive reception. Its 2 MiB PSRAM and lack of touch still exclude the full-board labs and UI. Lossless logos recover flash capacity; the 21% guard is unchanged. See [Lite controls](WAVESHARE-LITE.md) and [Spotpear clock setup](SPOTPEAR-CLOCK.md).
