# Flash budget · 0.13.8

Application growth is capped at **79% of each OTA application slot**, leaving at least 21% of that slot free. New badge/history/recon writes also preserve 21% of their respective writable flash filesystem; existing files are never deleted to reach that reserve. Factory formatting and explicit user deletion remain separate actions.

| Board | Physical flash | Current app, MiB | App slot, MiB | App slot free | User filesystem allocation, MiB |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2.83 | 6 | 52.90% | 19.94 |
| M5Stack StopWatch | 16 MiB | 2.99 | 4 | 25.33% | 7.94 |
| Spotpear | 16 MiB | 2.62 | 6 | 56.32% | 3.94 |
| Seeed XIAO Round | 8 MiB | 2.37 | 3 | 21.04% | 1.94 |
| Waveshare Lite | 16 MiB | 1.64 | 6 | 72.63% | 3.94 (unused by radar UI) |

These are packaged-image measurements rounded for readability. Exact byte counts accompany every board as `memory-budget.json` and inside `build-info.json`. The build fails if the app would cross the 79% limit. `memory status` on the device reports actual running app size, legacy filesystem use, free heap and PSRAM; `lab status` adds Waveshare recon filesystem use. The web Notebook section reads the runtime budget.

These figures are from the published clean-CI v0.13.8 packages, source commit `c8b388f069abf9c995d9ae5a354f5a746e7a2f59`. The 21% guard keeps the requested 20% minimum intact. Remaining application growth before that guard is **2,006,890 bytes** on AMOLED, **181,516 bytes** on StopWatch, **2,222,378 bytes** on Spotpear, **1,125 bytes** on XIAO Round and **3,248,154 bytes** on Lite. XIAO is effectively at its current growth limit: further shared features must first recover space or fit within that measured allowance. The reserve must not be lowered to make a build pass.

**None of these boards is declared feature-complete or filled to 79% of physical flash.** The separate OTA slot is reserved for update recovery, not application functionality. Filesystem capacity is reserved space, not proof it contains data. A larger flash chip does not increase internal radio/DMA RAM, CPU throughput, radio bands, or the number of channels listened to simultaneously. Useful features and recorded evidence can grow within these limits; empty bytes are not padded to manufacture an occupancy target.

Waveshare retains its existing lower-16-MB layout and separate upper-16-MB recon filesystem. Updates write neither filesystem nor NVS. M5Stack, Spotpear and XIAO retain their original layouts, badges and settings. All four full boards expose the notebook and active labs. Normal pages keep detection scheduled; explicit lab sessions temporarily own the radio. The compressed registry saves about 241 KiB of flash and uses about 693 KiB of PSRAM. Lite remains radar-only.

Flash headroom does not establish runtime compatibility. All four full boards already compile the same AMOLED software workflows with hardware-specific controls and recording limits. Lite has 2 MiB of PSRAM and no touch input. AMOLED's 2 MiB capture buffer alone would consume that entire PSRAM, before the display, contact store and receive queues. A smaller, separately designed Lite interface and memory budget would need implementation and runtime validation before expanding its supported feature set; unused flash is not sufficient evidence that the full build will work.

See the [feature comparison](FEATURE-MATRIX.md) for completed workflows and remaining work, including transparent ATT forwarding, USB HID/DuckyScript, external GPS and Chimera JSON protocol compatibility. v0.13.8 has physical runtime checks on Waveshare AMOLED, Spotpear and StopWatch. XIAO remains build/package-tested. Lite's application write passed checksum verification, but normal startup was not verified; hardware troubleshooting was stopped at the owner's request. [Detailed validation](VALIDATION.md).
