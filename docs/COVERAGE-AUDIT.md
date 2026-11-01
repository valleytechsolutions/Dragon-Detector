# SquachWatch coverage audit · 0.14.0

Reference: [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD/tree/3e9be6c02265b14c798e58da01a0913866143b0f), inspected 2026-11-01. This is a pinned comparison, not a promise to match future upstream changes automatically.

## What is checked

| Reference table | Entries | Dragon regression check |
|---|---:|---|
| Wi-Fi address patterns | 110 | Construct a complete beacon for each source prefix, recognize its family, check its radar eligibility |
| BLE service IDs | 25 | Construct a valid service-list advertisement for each ID, recognize its family, check radar eligibility |
| BLE manufacturer IDs | 8 | Check every company; Apple additionally requires a valid Find My payload |
| Exact device names | 7 | Check each name inside a valid BLE name AD field |
| SSID prefixes | 8 | Check each prefix with a suffix in an actual Wi-Fi beacon |
| **Total table entries** | **158** | **Every entry must pass; strong-only alert settings must not hide it** |

The [machine-readable ledger](data/squachwatch-coverage.json) lists every individual identifier and expected Dragon family. It contains public matching facts, not copied GPL functions or assets. The [native coverage tests](../tests/coverage_test.h) exercise the independent firmware parsers and the same visibility predicate used on the device. A failed or missing entry fails CI.

Additional fixtures cover the reference's runtime predicates: bare Flock SSID, BLE Flock/Penguin/Pigvision/Axon/Flipper substrings, ten-digit battery names, the Flock accessory UUID, valid Find My and Apple pairing-size forms, iBeacon, Pwnagotchi peer name/counter fields with a changed MAC, BLE/Wi-Fi/NAN Remote ID, disconnect bursts and SSID security conflicts. Dragon-specific Axon/AXEOFF, DULT, Biscuit, Nyanbox, HaleHound and fox signatures remain tested alongside them.

Run:

```sh
python tools/test_core.py --cxx g++
python tools/test_core.py --cxx g++ --sanitize
python tools/check_coverage.py --reference /path/to/SquachWatch-CYD
```

The last command compares the five upstream table identifier sets against the committed ledger. It reads a separately obtained reference checkout and does not compile or bundle its application. CI uses the pinned ledger, so an upstream branch change cannot silently change a release's detectors.

## November 2026 upstream review

The latest published release inspected was v1.40.0. Master at the pinned commit includes later MeshCore work. Compared with the previous `1485b0122aa527f8309c3f4e6ba8dc1614be5bb1` audit, `src/signatures.cpp`, `src/remote_id.cpp` and `include/remote_id.h` are unchanged. Receive-path changes primarily introduce the Tab5 P4/C6 ESP-Hosted transport and board capability guards; these are not a new signature list for the native ESP32-S3 profiles. Dragon retains transmitter-only Wi-Fi RSSI attribution instead of assigning a received frame's RSSI to its destination address. MeshCore/LoRa, 5 GHz and Classic-Bluetooth capabilities require different radio hardware.

Version 0.14.0 also fixes weaker Axon clues replacing stronger protocol evidence, including subsequent contact updates. The [release notes](RELEASE-0.14.0.md) and [Adult Mode notes](ADULT-MODE.md) distinguish these changes from existing table coverage.

## Meaning of parity

All **well-formed, receivable reference table signatures** have a visible candidate path. Shared identifiers stay weak: an Espressif radio is not proven to be Flock, a Motorola OUI is not proven to be an ALPR, Meta company IDs are not proven camera glasses, FEAA may be an Eddystone beacon, and an Apple pairing advertisement may be AirPods. Evidence details retain that specificity; the radar shows the matched family without CLUE/hint labels. Inspect the actual evidence before drawing conclusions.

Three locally administered patterns (`82:6B:F2`, `02:C0:CA`, `02:13:37`) are explicitly labeled address-pattern clues, not vendor assignments. Random BLE addresses never receive vendor attribution. Samsung `48:27:EA` and Sonos `00:0E:58` are excluded by both this reference revision and Dragon.

Dragon deliberately rejects malformed/truncated AD fields and Apple byte sequences embedded inside unrelated fields, even where the reference's raw substring matcher would accept them. Names identifying the Flock Noir dashboard or Dragon Detector itself do not count as camera-name evidence. It requires decoded Remote ID for strong drone evidence; a bare FFFA service remains a visible weak clue. SSID security-conflict alerts require repeat observations to reduce transient false positives. These are evidence-quality differences, not omitted device families.

## Hardware and reception boundaries

All eleven supported profiles share the same detector implementation. Their ESP32-S3 radios receive 2.4 GHz Wi-Fi and legacy Bluetooth LE. They cannot receive Bluetooth Classic-only serial/skimmer modules, 5/6 GHz Wi-Fi, LoRa, or extended-only BLE broadcasts. The Waveshare 1.75-G has onboard GPS; optional external GPS is capability-gated on supported boards. GPS geotags observations but does not add radio bands. Silent radios, address changes, antenna orientation, scan scheduling and radio congestion can still cause misses. No test suite substitutes for a controlled field trial against every physical product.

Passive BLE remains the default. **Settings → BLE scan → Active** requests scan responses from scannable devices, which can reveal names omitted from the initial advertisement. It transmits scan requests without connecting or pairing. Results are bounded to 96 advertisers per one-second active scan; subsequent cycles clear them. Dedicated hunts use the selected mode. Disable Active to return to listen-only BLE.

## Resource changes

- Contact capacity: 384 in PSRAM on full profiles; the no-PSRAM Dial uses 32 internal-RAM contacts and a 16-entry receive pool.
- A 1,024-entry lookup cache verifies complete radio/address/type identity on every hit; collisions fall back to a full search, and reused slots cannot alias a different device.
- The 96-entry packet pool passes one-byte indexes through queues instead of repeatedly clearing/copying 1 KB records. Without PCAP, Wi-Fi data frames retain only the header/LLC bytes needed for address and EAPOL detection; the AMOLED retains complete bounded EAPOL bodies for key-message analysis, and management frames retain their bodies.
- Radio batches yield after a 4 ms processing budget or 64 packets, whichever occurs first. One slow packet can exceed that budget; live diagnostics report the measured maximum and queue lag.
- Vendor lookup uses binary search. Radar ordering uses cached `O(n log n)` sorting instead of an `O(n²)` sort on every frame. All retained visible contacts can have dots; the former 60-dot cap is removed.
- Target lists create ten rows per category page, ordered ALPR/Flock, Axon, cameras and other signals, with trackers last. Within a category, freshness, evidence and signal determine order. Stored row identities are checked again on tap, so a busy scan cannot redirect a selection to a reused slot.
- Continuing history updates are rate limited while first matches and family changes are recorded immediately. Flash journaling remains bounded.

Radio-health counters disclose queue drops, peak occupancy, processing time, lag, evictions and memory. They describe the current boot, not guaranteed battery life or universal throughput. See [measured validation](VALIDATION.md).
