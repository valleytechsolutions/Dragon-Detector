# Flash budget · 0.13.13

Published clean-CI measurements from `613b3cb1f8b87e8080b8da9bc835d040d4005709`. Application growth is limited to 79% of each OTA application slot, leaving at least 21% free. Managed filesystem writes retain the same 21% guard. The guard must not be lowered to make a build pass.

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before 21% guard |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,789,088 | 6,291,456 | 55.67% | 2,181,162 |
| M5Stack StopWatch | 16 MiB | 2,957,696 | 4,194,304 | 29.48% | 355,804 |
| Spotpear | 16 MiB | 2,660,096 | 6,291,456 | 57.72% | 2,310,154 |
| Seeed XIAO Round | 8 MiB | 2,462,880 | 3,145,728 | 21.71% | 22,245 |
| Waveshare Lite | 16 MiB | 1,790,960 | 6,291,456 | 71.53% | 3,179,290 |

The separate OTA slot is reserved for update recovery. Filesystem allocations are available for user files, not occupied application bytes. Exact partition budgets ship in each `memory-budget.json`. A larger flash chip does not increase internal DMA/radio RAM or processor throughput.

All 147 compressed logo variants pass native pixel CRC checks. The four full boards share the application workflows with different capture/storage limits; Lite retains five compact pages because it has 2 MiB PSRAM and one button. Both new animated clocks are included on Lite. The XIAO is close to the guard: further features must fit its measured remaining budget.

The connected AMOLED, StopWatch and Spotpear were updated with the exact public binaries and passed clock/restart/self-test checks. XIAO and Lite are build/package-tested in this release; Lite hardware checks remain stopped. [Validation](VALIDATION.md) · [Feature matrix](FEATURE-MATRIX.md).
