# Flash budget · 0.13.12

Published clean-CI measurements from `aca3563c73d430a17adf6c322ce6212bdf98a779`. Application growth is limited to 79% of each OTA application slot, leaving at least 21% free. Managed filesystem writes retain the same 21% guard. The guard must not be lowered to make a build pass.

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before 21% guard |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,787,024 | 6,291,456 | 55.70% | 2,183,226 |
| M5Stack StopWatch | 16 MiB | 2,955,616 | 4,194,304 | 29.53% | 357,884 |
| Spotpear | 16 MiB | 2,659,088 | 6,291,456 | 57.73% | 2,311,162 |
| Seeed XIAO Round | 8 MiB | 2,462,368 | 3,145,728 | 21.72% | 22,757 |
| Waveshare Lite | 16 MiB | 1,790,496 | 6,291,456 | 71.54% | 3,179,754 |

The separate OTA slot is reserved for update recovery. Filesystem allocations are available for user files, not occupied application bytes. Exact partition budgets ship in each `memory-budget.json`. A larger flash chip does not increase internal DMA/radio RAM or processor throughput.

All 147 compressed logo variants pass native pixel CRC checks. The four full boards share the application workflows with different capture/storage limits; Lite retains five compact pages because it has 2 MiB PSRAM and one button. Both new animated clocks are included on Lite. The XIAO is close to the guard: further features must fit its measured remaining budget.

The connected AMOLED, StopWatch and Spotpear were updated with the exact public binaries and passed clock/restart/self-test checks. XIAO and Lite are build/package-tested in this release; Lite hardware checks remain stopped. [Validation](VALIDATION.md) · [Feature matrix](FEATURE-MATRIX.md).
