# Detection feature comparison · 0.11.0

The requested reference is [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD), reviewed at `1485b0122aa527f8309c3f4e6ba8dc1614be5bb1`. Its detection families and controls informed this independently implemented Dragon Radar interface. Its GPL-3.0 application code and artwork are not incorporated into the MIT application. [Credits and license boundaries](../CREDITS.md).

## Device coverage

Every detection category and all **158 table entries** in that pinned reference have corresponding recognition and radar-visibility fixtures here: 110 address patterns, 25 service IDs, eight company IDs, seven exact names and eight SSID prefixes. See the [complete fact ledger](data/squachwatch-coverage.json) and [audit procedure](COVERAGE-AUDIT.md). Matching a category does not guarantee reception of every product in it. The ESP32-S3 hardware, advertised payloads, radio scheduling and evidence thresholds set the practical limits.

| Reference category | Dragon Detector implementation |
|---|---|
| Flock | Registered Flock prefix, bounded research-prefix hints, Flock accessory service, named candidates, battery composites, strict probe fingerprint |
| Axon / TASER | Registered vendor prefix, two shared-module research hints, company/service IDs, AB2/3/4 and Axon names |
| Camera glasses | Luxottica/Meta composite, Snap company + FE45, weaker company and name clues |
| Skimmer | HC-03/05/06/08, RN42, BT04-A and serial-service clues, cyber-blue module hint; always weak evidence |
| Raven | Exact 3100/3200/3300/3400/3500 services; no broad 3100–3500 range |
| AirTag / Find My | Find My payload, FD44 and DULT; pairing-size Apple adverts are weak clues that may be AirPods |
| Drone | Decoded Remote ID over legacy BLE, Wi-Fi beacons and 2.4 GHz NAN action frames; identity, aircraft position/altitude/speed, operator location when advertised |
| Other ALPR vendors | Genetec and Motorola Solutions vendor clues, graded by specificity |
| Camera vendors | Wyze, Hikvision, Verkada, Avigilon, Axis, Arlo and Blink vendor blocks; explicitly weak shared-module hints |
| Samsung tags | FD5A; Samsung manufacturer ID alone is insufficient |
| Google tags | FEAA service data with Find Hub 40/41 frame and valid EID length; bare/general FEAA is an explicitly weak Eddystone clue |
| Tile | FEED / FEEC / FD84 services; Chipolo FE33/FE65/CID08C3 share the tracker family |
| Ring | Ring LLC vendor blocks; shared Amazon blocks remain weak |
| Deauthentication activity | Six deauth/disassociation frames per rolling three seconds across source addresses; retry suppression and 15-second burst cooldown |
| Evil twin candidate | Repeated same **raw SSID**, different BSSIDs, conflicting open/protected status, contemporaneous observations |
| iBeacon | Complete Apple iBeacon format; enabled by default, optional family toggle |
| Pentest devices | Flipper service/appearance/name, Pwnagotchi name/counter fields even with changed MAC, Pineapple/deauther names; retained Nyanbox, HaleHound and Biscuit candidates |

Service IDs also accept equivalent 16/32/Bluetooth-Base-128 advertising encodings, with the original evidence thresholds preserved.

## Detection controls

| Capability | Status in the five current board profiles |
|---|---|
| Signal hunt / foxhunting | Dedicated BLE or Wi-Fi listening, dBm gauge, 24-second signal trace, peak, trend, stability, saved comparison point, stale/reacquisition state |
| Training fox | Explicitly enabled non-connectable Dragon BLE beacon; off at boot. Another Dragon Detector can detect and hunt it. Original protocol; not SquachMesh-compatible |
| Type and confidence filters | Per-family visibility/alert switches; separate weak / medium+ / strong audible-alert threshold. Priority families bypass the confidence gate; trackers only alert when watched. All enabled matches remain on radar; all families enabled on fresh installs |
| Ignore, snooze, watch | 64 address/radio rules; ignore/watch persist, snooze lasts until restart; watched addresses receive priority cards |
| Repeated alert suppression | Off / after 5 / after 10; after the allowance, another alert needs a 7 dB stronger signal; allowance resets after 30 minutes |
| Advertisement floods | Distinct-address and pairing-payload grouping over five seconds; aggregated alerts and an Activity view; tag cards quieted during a burst while history and watch rules remain active |
| Alert cards | Six-second family-colored notification bubbles; Axon preempts lower priorities, with eight pending cards and 30-second expiry. Watch cards stay until dismissed; Hunt shortcut; quiet mode |
| Persistent history | 512 unique address/radio/family records in RAM, bounded two-file flash journal, record CRC, first-match recording, encounters, peak/last signal, UTC when synchronized |
| Export | Local CSV through the GitHub web tool; optional additional SD event CSV/PCAP on Spotpear |
| Privacy | Screen names/address masking and hidden location/evidence text; stored/exported records retain full evidence |
| Display | Default 100% brightness and no dim/off timers; adjustable brightness, dim timer/level, off timer and wake-on-alert |
| Sound/haptic alerts | Original RTTTL tones for all 26 families, speaker volume, Axon heartbeat, automatic family focus and rising/falling hunt feedback. Three audio adapters; StopWatch additionally has haptics. See hardware validation. |

## Deliberate boundaries

The user's revised scope defers badge changes and excludes pets/games and the social application. The existing USB badge editor remains available. Encrypted chat, squad relays, invite/key management, OTA/group updates, PIN/duress features, clock/desk games and cosmetic unlocks are not part of this detector release. Dragon foxhunting works without those systems.

The chosen boards have no supported 5 GHz, LoRa or GPS receiver in these profiles. SquachWatch's C5 5 GHz and T-Watch LoRa/GPS/WiGLE functions therefore are not advertised here. Bluetooth Classic-only skimmer modules, silent radios and extended-advertising-only BLE products can be missed. Remote ID coordinates are broadcast reports from the aircraft, not the detector's own GPS fix. There is no owner identification, precise ranging or bearing measurement.

History keeps encounter counts, not a claim that someone is following the wearer. Rotating BLE addresses remain separate. No cross-address identity correlation is attempted.

## Signature provenance

- Flock facts: [Flock Noir](https://github.com/valleytechsolutions/Flock-Noir), Colonel Panic and the credited Flock-You research; [detailed evidence](DETECTION.md).
- Vendor assignments: [Wireshark's IEEE-derived registry](https://www.wireshark.org/download/automated/data/manuf), checked 2026-10-13. The independently selected facts are in [vendor-prefixes.csv](data/vendor-prefixes.csv). This includes registered camera-vendor blocks beyond the reference's smaller list. No registry organization is treated as proof of a product or intent.
- Bluetooth assignments: [Bluetooth SIG](https://www.bluetooth.com/specifications/assigned-numbers/).
- Google framing: [Find Hub Network Accessory Specification](https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn).
- Remote ID decoder: [OpenDroneID](https://github.com/opendroneid/opendroneid-core-c), pinned Apache-2.0 source and [provenance](../lib/opendroneid/UPSTREAM.md). Transport framing is independently implemented from the public protocol.

Two notable corrections are intentional: Samsung `48:27:EA` and Sonos `00:0E:58` are not Flock/ALPR matches (also excluded by this reference snapshot). General FEAA remains visible as an unverified Eddystone clue, never strong tracker evidence. Raven's range ambiguity is resolved in favor of its exact named services. These exclusions are regression-tested.
