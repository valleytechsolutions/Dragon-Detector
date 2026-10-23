# SD resources beta — 0.13.28

This is an optional resource edition. Radar and the internal MA-L registry still work without a card. No SD-required edition is being released: additional capacity is useful, but no physical SD latency or scan-continuity measurements are available yet. These changes are based on local commit `588ec4d`, preserving the Dial v1.1 release and approved showcase images. The production site is not automatically replaced by building this beta.

## Install

Download `dragon-resources-v1-20261023.zip`. Extract **the `dragon` directory to the FAT32 card root**, giving `/dragon/resources/vendors.ddr`. Do not copy only the ZIP or create another containing directory. Preserve captures and private configuration already on the card. Insert before powering on and restart after changing the pack. Firmware 0.13.28 reads schema 1; older firmware ignores this optional file. `manifest.json` records exact source and output SHA-256 values. Check the ZIP against the supplied SHA256SUMS before use.

`resources status` reports validation status, snapshot, entry count, allocated bytes, startup load time, maximum lookup time and lookup count. Missing cards/files, unsupported schema, old snapshots, truncation, failed allocation and malformed data leave internal lookup available. This command exists only on SD profiles. No card formatting, overwriting existing resources, or automatic downloads occur.

## Exact capability matrix

| Profile | SD pack | Onboard GPS / upload candidate |
|---|---|---|
| Spotpear / Guition JC3636W518 | Optional | No |
| Waveshare ESP32-S3-Touch-AMOLED-1.75 | Optional | No |
| Waveshare ESP32-S3-Touch-AMOLED-1.75-G | Optional | LC76G; upload transport not enabled in this beta |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | None | No |
| LILYGO T-Display-S3-AMOLED-1.75, H0175Y003AM | Optional | No |
| Seeed XIAO ESP32-S3 + Round Display | Optional | No |
| M5Stack StopWatch C152 | None | No |
| M5Stack Dial / Dial v1.1 | None | No |
| Elecrow CrowPanel 2.1 Rotary | None | No |
| Waveshare ESP32-S3-LCD-1.28 Lite | None | No |
| Tuya T5-E1 beta | Not implemented; separate ARM project | Not implemented |

The matrix follows the current board drivers and manufacturer references already pinned in the board guides. SD on the XIAO means the Round Display expansion, not the bare XIAO. No external GPS accessories are implicitly enabled. The T5-E1 has not been converted into an ESP32 target or given ESP32 capabilities. This beta also removes the GPS wardrive menu/command route from the non-GPS 1.75.

## Architecture and measurements

The pack adds IEEE MA-M (28-bit) and MA-S (36-bit) assignments to MA-L (24-bit). Its 54,282 records occupy **1,270,072 bytes**, including a sorted 10-byte index and deduplicated names. More specific prefixes win. Public/universal addresses only: randomized BLE, locally administered Wi-Fi and multicast addresses do not produce vendor claims. Registry labels never change detector confidence or infer product identity.

The independent DDR1 reader accepts at most 3 MiB / 100,000 records, validates lengths, whole-file CRC32 (checksum field zeroed), sorted unique keys, permitted prefix widths, string boundaries and label lengths. Snapshots before 2026-10-15 are rejected. It does not claim authenticity: CRC detects accidental corruption, and trusted distribution SHA-256 checks remain important.

The card is read sequentially in 4 KiB chunks before radio startup. A complete validated index is retained in PSRAM; lookups perform at most three binary searches with **zero SD calls**, no allocation and no pointer eviction. At least 2 MiB free PSRAM is required beyond the allocation at load time. The existing 709,215-byte decoded internal registry remains available as fallback. This trades about 1.27 MB extra PSRAM and startup cost for predictable steady-state lookup. A sector cache would save RAM but introduce card stalls and cache-pointer lifetimes into UI lookup; it is not justified without hardware measurements. No-SD full profiles retain their existing compact flash registry; Lite profiles retain their existing detector tables.

One Windows host run validated the full pack in 5,110 microseconds and performed 100,000 lookups in 7,211 microseconds (`-O2`, assertions enabled). These are parser/CPU observations, **not ESP32 timing, filesystem throughput, or evidence of faster scanning**. Reproduce with `tools/test_sd_resources.py`; timings vary. Device counters provide the missing measurements.

## Hardware acceptance still required

For each SD board, compare no card, blank FAT32, valid pack, old schema/snapshot, truncated and corrupted files. Verify internal fallback, display and touch, scan queue drops and maximum radio lag; measure boot/load time, lookup maximum and free/largest PSRAM block. Test card removal after load: vendor pointers must remain valid, while recording reports its own card failure. Stop/finalize recording before normal removal. Repeat with slow cards and a nearly full card, and compare against the same firmware without a pack. No throughput or physical validation is asserted by the host tests.

## Compared lookup approaches

ChimeraBLE's pinned `src/oui_db.cpp` searches a six-byte on-card OUI2 index and retains up to 128 lookup results, clearing that cache at capacity. A miss needs repeated seeks/reads. CYM's pinned `ESP32C5/main/oui_lookup.c` loads 32-byte OUI1 records into PSRAM and binary-searches memory. These are two different RAM/latency choices, not evidence that putting a database on SD makes execution faster. Dragon Detector's original reader was implemented independently before this source comparison: it uses a validated deduplicated 10-byte multi-prefix index in PSRAM, retaining the compact internal fallback. No code, binary format implementation or databases from those repositories are redistributed.

## Upstream review — 2026-10-23

* [MatthewKuKanich/ChimeraBLE](https://github.com/MatthewKuKanich/ChimeraBLE/tree/2330423dae8f8fa163617172310784889d8972a7), GPL-3.0. Its Cardputer guide documents an optional `/ChimeraBLE/oui.bin` and a separate folder for profiles/logs. Reviewed as a workflow reference; no GPL code or database copied.
* [JimGat/CYM](https://github.com/JimGat/CYM/tree/59a86a86a7bfb35635ebd3a8cac9165bdc6ca238), the exact Cheap Yellow Monster project. The recursive tree and README have no general top-level license grant at this revision; component/case licenses do not license the entire firmware.
* [JimGat/CYM-SD-Assets](https://github.com/JimGat/CYM-SD-Assets/tree/d4c93f3478b92e36bc3cca3e02be14cadc4f2698) separates SD provisioning and source manifests. No general top-level license grant found. Its optional dictionaries/assets are not imported. Dragon Detector has no supported RFID research workflow needing those dictionaries.
* Data is generated independently from [IEEE MA-L](https://standards-oui.ieee.org/oui/oui.csv), [MA-M](https://standards-oui.ieee.org/oui28/mam.csv) and [MA-S](https://standards-oui.ieee.org/oui36/oui36.csv). The pack contains assignment/name facts only. Names and marks remain their owners'.

Reverse-engineering storage already uses Dragon Detector's bounded DDG1 profiles, BLE inspection and captures. The beta pack includes an original reference note for interpreting those records; executable scripts, attack dictionaries, and undocumented payloads are not loaded from the card. See `WARDRIVE-UPLOAD-DESIGN.md` for the GPS-only upload investigation.

The supplied ChatGPT Library reference could not be inspected: its exact URL redirected to account sign-in in the in-app browser, which has no remembered authenticated account. The file itself was not returned by web retrieval. Attach the file or provide an authenticated browser session to incorporate it.
