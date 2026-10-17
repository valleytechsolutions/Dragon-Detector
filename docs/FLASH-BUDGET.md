# Flash budget

From 0.13.14, application slots and managed filesystem writes retain 5% free. The application limit is checked during packaging; filesystem writes check available space at runtime. Existing partitions and saved data are unchanged.

The table below records the previous 0.13.13 build; current measurements ship in each release’s `memory-budget.json`.

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before 5% limit |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,789,088 | 6,291,456 | 55.67% | 3,187,795 |
| M5Stack StopWatch | 16 MiB | 2,957,696 | 4,194,304 | 29.48% | 1,026,892 |
| Spotpear | 16 MiB | 2,660,096 | 6,291,456 | 57.72% | 3,316,787 |
| Seeed XIAO Round | 8 MiB | 2,462,880 | 3,145,728 | 21.71% | 525,561 |
| Waveshare Lite | 16 MiB | 1,790,960 | 6,291,456 | 71.53% | 4,185,923 |

The separate OTA slot is reserved for update recovery. Filesystem allocations are available for user files, not occupied application bytes. Exact partition budgets ship in each `memory-budget.json`. A larger flash chip does not increase internal DMA/radio RAM or processor throughput.

All 147 compressed logo variants pass native pixel CRC checks. The four full boards share the application workflows with different capture/storage limits; Lite retains five compact pages because it has 2 MiB PSRAM and one button. Both new animated clocks are included on Lite.

The connected AMOLED, StopWatch and Spotpear were updated with the exact public binaries and passed clock/restart/self-test checks. XIAO and Lite are build/package-tested in this release; Lite hardware checks remain stopped. [Validation](VALIDATION.md) · [Feature matrix](FEATURE-MATRIX.md).
