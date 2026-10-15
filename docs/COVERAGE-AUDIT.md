# SquachWatch coverage audit · 0.5.0

Reference: [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD/tree/a6167dbb0693963b5ad7022e7923cfb458d7676c), inspected 2026-10-13. This is a pinned comparison, not a promise to match future upstream changes automatically.

## What is checked

| Reference table | Entries | Dragon regression check |
|---|---:|---|
| Wi-Fi address patterns | 97 | Construct a complete beacon for each source prefix, recognize its family, check its radar eligibility |
| BLE service IDs | 15 | Construct a valid service-list advertisement for each ID, recognize its family, check radar eligibility |
| BLE manufacturer IDs | 7 | Check every company; Apple additionally requires a valid Find My payload |
| Exact device names | 7 | Check each name inside a valid BLE name AD field |
| SSID prefixes | 8 | Check each prefix with a suffix in an actual Wi-Fi beacon |
| **Total table entries** | **134** | **Every entry must pass; strong-only alert settings must not hide it** |

The [machine-readable ledger](data/squachwatch-coverage.json) lists every individual identifier and expected Dragon family. It contains public matching facts, not copied GPL functions or assets. The [native coverage tests](../tests/coverage_test.h) exercise the independent firmware parsers and the same visibility predicate used on the device. A failed or missing entry fails CI.

Additional fixtures cover the reference's runtime predicates: bare Flock SSID, BLE Flock/Penguin/Pigvision/Axon/Flipper substrings, ten-digit battery names, the Flock accessory UUID, valid Find My and Apple pairing-size forms, iBeacon, Pwnagotchi peer name/counter fields with a changed MAC, BLE/Wi-Fi/NAN Remote ID, disconnect bursts and SSID security conflicts. Dragon-specific Axon/AXEOFF, DULT, Biscuit, Nyanbox, HaleHound and fox signatures remain tested alongside them.

Run:

```sh
python tools/test_core.py --cxx g++
python tools/test_core.py --cxx g++ --sanitize
python tools/check_coverage.py --reference /path/to/SquachWatch-CYD
```

The last command compares the five upstream table identifier sets against the committed ledger. It reads a separately obtained reference checkout and does not compile or bundle its application. CI uses the pinned ledger, so an upstream branch change cannot silently change a release's detectors.

## Meaning of parity

All **well-formed, receivable reference table signatures** have a visible candidate path. Shared identifiers stay weak: an Espressif radio is not proven to be Flock, a Motorola OUI is not proven to be an ALPR, Meta company IDs are not proven camera glasses, FEAA may be an Eddystone beacon, and an Apple pairing advertisement may be AirPods. Evidence details retain that specificity; the radar shows the matched family without CLUE/hint labels. Inspect the actual evidence before drawing conclusions.

Three locally administered patterns (`82:6B:F2`, `02:C0:CA`, `02:13:37`) are explicitly labeled address-pattern clues, not vendor assignments. Random BLE addresses never receive vendor attribution. Samsung `48:27:EA` and Sonos `00:0E:58` are excluded by both this reference revision and Dragon.

Dragon deliberately rejects malformed/truncated AD fields and Apple byte sequences embedded inside unrelated fields, even where the reference's raw substring matcher would accept them. Names identifying the Flock Noir dashboard or Dragon Detector itself do not count as camera-name evidence. It requires decoded Remote ID for strong drone evidence; a bare FFFA service remains a visible weak clue. SSID security-conflict alerts require repeat observations to reduce transient false positives. These are evidence-quality differences, not omitted device families.

## Hardware and reception boundaries

All three selected boards share the same detector implementation. Their ESP32-S3 radios receive 2.4 GHz Wi-Fi and legacy Bluetooth LE. They cannot receive Bluetooth Classic-only serial/skimmer modules, 5/6 GHz Wi-Fi, LoRa, or extended-only BLE broadcasts. They have no supported local GPS receiver. Silent radios, address changes, antenna orientation, scan scheduling and radio congestion can still cause misses. No test suite substitutes for a controlled field trial against every physical product.

Passive BLE remains the default. **Settings → BLE scan → Active** requests scan responses from scannable devices, which can reveal names omitted from the initial advertisement. It transmits scan requests without connecting or pairing. Results are bounded to 96 advertisers per one-second active scan; subsequent cycles clear them. Dedicated hunts use the selected mode. Disable Active to return to listen-only BLE.

## Resource changes

- Contact capacity: 160 → 384, allocated in PSRAM rather than occupying internal radio/DMA memory.
- A 1,024-entry lookup cache verifies complete radio/address/type identity on every hit; collisions fall back to a full search, and reused slots cannot alias a different device.
- The 96-entry packet pool passes one-byte indexes through queues instead of repeatedly clearing/copying 1 KB records. Without PCAP, Wi-Fi data frames retain only the header/LLC bytes needed for address and EAPOL detection; management frames retain their bodies.
- Radio batches yield after a 4 ms processing budget or 64 packets, whichever occurs first. One slow packet can exceed that budget; live diagnostics report the measured maximum and queue lag.
- Vendor lookup uses binary search. Radar ordering uses cached `O(n log n)` sorting instead of an `O(n²)` sort on every frame. All retained visible contacts can have dots; the former 60-dot cap is removed.
- Contact lists create ten rows per page, sorted by evidence and signal. Stored row identities are checked again on tap, so a busy scan cannot redirect a selection to a reused slot.
- Continuing history updates are rate limited while first matches and family changes are recorded immediately. Flash journaling remains bounded.

Radio-health counters disclose queue drops, peak occupancy, processing time, lag, evictions and memory. They describe the current boot, not guaranteed battery life or universal throughput. See [measured validation](VALIDATION.md).
