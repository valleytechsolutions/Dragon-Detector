# IEEE address registry

Most full profiles use the flash registry introduced in 0.13.0: a **2026-10-15 snapshot of 40,333 distinct IEEE MA-L / 24-bit prefixes**. The factual index comes directly from [IEEE's public MA-L registry](https://standards-oui.ieee.org/oui/oui.csv). Organization names remain attributable to their registrants. This does not change the license of the original application code. **Waveshare LCD 1.85B, introduced in 0.14.1, uses SD lookup resources instead**, as described below.

The independent generator keeps organization names and prefixes only. Two prefixes with multiple different organization names retain all names, separated by a slash. Sorted six-byte index records and a deduplicated UTF-8 string pool occupy **709,215 decoded bytes**. An independent bounded LZ4-block codec stores them in **462,273 flash bytes** on profiles with the compiled registry and expands them into PSRAM after CRC verification at startup. Binary search uses that validated copy; allocation or CRC failure disables vendor lookup without disabling radar detection. These profiles need no network request or SD card for the base MA-L registry.

`docs/vendor-registry.json` records source SHA-256, generated SHA-256 and counts. To regenerate from an official CSV snapshot: `python tools/generate_vendor_registry.py path/to/oui.csv`. A newer download can legitimately change the count/hash. Review the generated change and update the pinned count test before publishing it.

Lookup applies only to universally administered Wi-Fi or public/public-identity BLE addresses. Random/random-identity BLE addresses and locally administered Wi-Fi addresses return no manufacturer claim. MA-M and MA-S allocations are not separate entries in this index. A radio module's registered organization does not establish the finished product, operator, intent or recording status. Vendor lookup adds context to Contact Evidence and the BLE inspector; it does not promote an unknown device into a threat.

`registry status` reports the snapshot count. `vendor aa:bb:cc:dd:ee:ff` looks up a universal address. The full boards expose lookup in device details. Lite retains its radar-only interface.

## Waveshare LCD 1.85B SD lookup names

The 1.85B build omits the 462,273-byte compiled vendor payload and its separate 709,215-byte decoded PSRAM copy. It uses `/dragon/resources/vendors.ddr`, containing the 2026-10-23 MA-L/MA-M/MA-S snapshot: 54,282 assignments in 1,270,072 bytes. The file is identical to the vendor data in the older optional pack. It is loaded and validated once at boot; address lookups use the resulting PSRAM index without card I/O.

The same board's `/dragon/resources/ble-labels.ddb` contains 44 display mappings in 1,005 bytes: 31 service/characteristic UUID labels and 13 Bluetooth company labels. UUID and company identifiers occupy separate typed namespaces. Their source data is maintained in `docs/ble-labels.json`. These names are display metadata; numeric detector signatures and matching rules remain compiled into firmware.

Download `dragon-resources-v1-20261103.zip` for the 1.85B, extract its `dragon` directory to the card root and restart. **SYSTEM → SD resources** shows whether both resources loaded and offers installation guidance. Missing or invalid resource files leave the corresponding names unresolved, with numeric identifiers still available. Core radar detection continues; this profile does not fall back to a compiled general vendor/name database. Other SD profiles keep their existing optional vendor pack and compiled fallback. See [SD lookup resource installation](SD-RESOURCES.md).
