# Flash budget · 0.13.9

Application growth is capped at **79% of each OTA application slot**, leaving at least 21% of that slot free. New badge/history/recon writes also preserve 21% of their respective writable flash filesystem; existing files are never deleted to reach that reserve. Factory formatting and explicit user deletion remain separate actions.

| Board | Physical flash | Current app, MiB | App slot, MiB | App slot free | User filesystem allocation, MiB |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2.61 | 6 | 56.57% | 19.94 |
| M5Stack StopWatch | 16 MiB | 2.77 | 4 | 30.84% | 7.94 |
| Spotpear | 16 MiB | 2.50 | 6 | 58.33% | 3.94 |
| Seeed XIAO Round | 8 MiB | 2.33 | 3 | 22.44% | 1.94 |
| Waveshare Lite | 16 MiB | 1.69 | 6 | 71.87% | 3.94 |

These are published clean-CI v0.13.9 package measurements from source `14e2719fd6648ed1d054402c246d37b109a9220b`, rounded for readability. Exact bytes accompany every board in `memory-budget.json` and `build-info.json`; clean-CI source revisions and hardware hashes accompany the release verification file. The 21% guard keeps the requested 20% minimum intact. Remaining application growth before the guard is **2,238,058 bytes** on Waveshare AMOLED, **412,732 bytes** on M5Stack StopWatch, **2,348,906 bytes** on Spotpear, **45,205 bytes** on Seeed XIAO Round, **3,200,426 bytes** on Waveshare Lite. The reserve must not be lowered to make a build pass.

The build fails if the app crosses the 79% limit. `memory status` reports running app size, legacy filesystem use, free heap and PSRAM; `lab status` adds Waveshare recon storage. The web Notebook section reads this runtime budget.

The separate OTA slot is reserved for update recovery. Filesystem allocations are space available for user files, not occupied application bytes. A larger flash chip does not increase internal radio/DMA RAM, CPU throughput, radio bands, or simultaneous channels. Empty bytes are not padded to manufacture an occupancy target.

Waveshare retains its lower-16-MB layout and separate upper-16-MB recon filesystem. Matching updates write neither filesystem nor NVS. M5Stack, Spotpear and XIAO retain their original layouts, badges and settings. All four full boards share the AMOLED application workflows; hardware controls and recording limits differ.

Lossless theme packing frees application space while retaining the original indexed pixels and palettes. Each board decodes only its panel-size assets once into PSRAM; no per-frame allocation is added. All 135 packed variants pass native CRC checks. The existing compressed vendor registry uses about 693 KiB of PSRAM. Runtime free heap/PSRAM is recorded in release hardware checks rather than inferred from flash capacity.

Lite adds five compact BOOT-operated pages: radar, themed clock, stopwatch/locksport timer, signal details and diagnostics. Its 2 MiB PSRAM and single button still exclude the full touch UI, recorder, badge and active labs; AMOLED's 2 MiB capture buffer alone would use that entire memory. Lite's reserved filesystem is not exposed as a user-file feature. All five profiles include the VTS startup animation.

See the [feature comparison](FEATURE-MATRIX.md) and [validation](VALIDATION.md) for boundaries. StopWatch and AMOLED receive final-binary USB runtime checks. Spotpear clock retention was checked on an earlier candidate; Wi-Fi provisioning was deferred. XIAO and new Lite pages are build/package-tested. Lite hardware troubleshooting remains stopped at the owner's request.
