# Axon detection and priority alerts · 0.9.6

Axon is the highest-priority device family. With **Axon priority: ON** (default), every recognized Axon match receives reserved packet capacity and can alert on its first received observation, even with strong-only filtering for other devices. It wins the primary radar tag, protected contact retention, notification priority and speaker playback. The normal radar remains a balanced 2.2-second BLE / 2.2-second Wi-Fi scan across channels 1–11. BLE now requests a 100/100 ms passive window.

## Reference match parity

The match conditions independently implement the public facts in [Colonel Panic's AXEOFF](https://github.com/colonelpanichacks/oui-spy-unified-blue/blob/6e5e2829540925ff1ab5fa6d0d365378818ed56e/src/raw/axeoff.cpp):

- Manufacturer company `034D` (little-endian advertising bytes `4D 03`).
- Service `FC81`, `FE6B` or `FE6C`, including AXEOFF's partial 32/128-bit UUID match. Service-data encodings are also accepted.
- BLE address prefix `00:25:DF`, including random-address types.
- Wi-Fi transmitter prefix `00:25:DF`; probes and beacons match AXEOFF, with additional probe-response/data/management coverage.

The priority path also accepts SquachWatch's Axon name patterns (`Axon`, `AB2`, `AB3`, `AB4`) and public/source address patterns `28:24:FF` / `E4:05:40`. Names from directed probe requests do not identify their transmitter. Complete bounded fields can survive an unrelated malformed tail; incomplete fields do not supply identifiers.

The radar shows the family name and signal strength, without CLUE or hint wording. Evidence details retain the actual match and confidence: a shared module address, custom UUID or name match is not upgraded into proof of a specific body-camera model or recording state. This reflects the reference signatures' specificity, rather than hiding their detections.

## Alerts and crowded environments

Axon, Flipper, Flock/ALPR, camera glasses, Ring, other camera families and drones alert on first recognized evidence. Axon priority OFF restores the general confidence/repeat gate for Axon. Family switches, ignored/snoozed addresses, repeat cooldowns, quiet mode and sound settings still apply. Quiet mode must be OFF to hear the alert. AirTag/Find My, SmartTag, Google tags, Tile and iBeacon remain visible/logged but do not automatically sound or show cards unless explicitly watched.

Axon interrupts a lower-priority melody/card. Other cards queue by priority for up to 30 seconds. Radar pages are removed; every retained visible contact has a dot. Up to six priority tags fit on the full display (four on Lite), and tapping a dot reveals its tag. Live higher-priority contacts cannot be evicted by new lower-priority addresses.

## Diagnostics

`axon status` reports received signatures, queue drops, maximum processing lag and alert count. `weak` remains the serial-compatible counter for broad UUID/address/name research matches; these now receive priority. `axon priority on|off` saves the first-hit behavior. `detector quiet off|on` saves the sound/card quiet setting. `radar status` reports one page, plotted count and primary contact index.

Host fixtures verify matching and prioritization. Real-camera reception still requires a repeat field test against the same camera and reference detectors; synthetic over-air tests are recorded separately in [VALIDATION.md](VALIDATION.md).
