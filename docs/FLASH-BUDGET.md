# Flash budget · 0.12.0

Application growth is capped at **79% of each OTA application slot**, leaving at least 21% of that slot free. New badge/history/recon writes also preserve 21% of their respective writable flash filesystem; existing files are never deleted to reach that reserve. Factory formatting and explicit user deletion remain separate actions.

| Board | Physical flash | Current app, MiB | App slot, MiB | App slot free | User filesystem allocation, MiB |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2.54 | 6 | 57.6% | 19.94 |
| M5Stack StopWatch | 16 MiB | 2.42 | 4 | 39.4% | 7.94 |
| Spotpear | 16 MiB | 2.22 | 6 | 63.0% | 3.94 |
| Seeed XIAO Round | 8 MiB | 2.11 | 3 | 29.6% | 1.94 |
| Waveshare Lite | 16 MiB | 1.85 | 6 | 69.2% | 3.94 |

These are packaged-image measurements rounded for readability. Exact byte counts accompany every board as `memory-budget.json` and inside `build-info.json`. The build fails if the app would cross the 79% limit. `memory status` on the device reports actual running app size, legacy filesystem use, free heap and PSRAM; `lab status` adds Waveshare recon filesystem use. The web Notebook section reads the runtime budget.

**None of these boards is declared feature-complete or filled to 79% of physical flash.** The separate OTA slot is reserved for update recovery, not application functionality. Filesystem capacity is reserved space, not proof it contains data. A larger flash chip does not increase internal radio/DMA RAM, CPU throughput, radio bands, or the number of channels listened to simultaneously. Useful features and recorded evidence can grow within these limits; empty bytes are not padded to manufacture an occupancy target.

Waveshare retains its existing lower-16-MB layout and separate upper-16-MB recon filesystem. Updates write neither filesystem nor NVS. M5Stack, Spotpear and XIAO retain their original layouts, badges and settings. All four full boards gain the passive notebook/vendor lookup; active radio ownership remains a Waveshare lab feature so the other boards continue detection. Lite remains radar-only.

See the [feature comparison](FEATURE-MATRIX.md) for completed workflows and remaining work, including transparent BLE relay, full HID semantic decoding, command scripting, external-GPS workflows and Marauder mission-file compatibility. Other full boards have compiled builds; this release was physically exercised on the connected Waveshare only.
