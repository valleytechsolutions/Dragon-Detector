# IEEE address registry

Version 0.13.0 contains a **2026-10-15 snapshot of 40,333 distinct IEEE MA-L / 24-bit prefixes**, stored in flash. The factual index comes directly from [IEEE's public MA-L registry](https://standards-oui.ieee.org/oui/oui.csv); it is not imported from ChimeraBLE, Wireshark or another firmware. Organization names remain attributable to their registrants. This does not change the license of the original application code.

The independent generator keeps organization names and prefixes only. Two prefixes with multiple different organization names retain all names, separated by a slash. Sorted six-byte index records and a deduplicated UTF-8 string pool occupy **709,215 decoded bytes**. An independent bounded LZ4-block codec stores them in **462,273 flash bytes** on full boards and expands them into PSRAM after CRC verification at startup. Binary search uses that validated copy; allocation or CRC failure disables vendor lookup without disabling radar detection. No network request or SD card is needed.

`docs/vendor-registry.json` records source SHA-256, generated SHA-256 and counts. To regenerate from an official CSV snapshot: `python tools/generate_vendor_registry.py path/to/oui.csv`. A newer download can legitimately change the count/hash. Review the generated change and update the pinned count test before publishing it.

Lookup applies only to universally administered Wi-Fi or public/public-identity BLE addresses. Random/random-identity BLE addresses and locally administered Wi-Fi addresses return no manufacturer claim. MA-M and MA-S allocations are not separate entries in this index. A radio module's registered organization does not establish the finished product, operator, intent or recording status. Vendor lookup adds context to Contact Evidence and the BLE inspector; it does not promote an unknown device into a threat.

`registry status` reports the snapshot count. `vendor aa:bb:cc:dd:ee:ff` looks up a universal address. The full boards expose lookup in device details. Lite retains its radar-only interface.
