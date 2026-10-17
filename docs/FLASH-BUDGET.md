# Flash budget · 0.13.11

Published clean-CI measurements from `ea09a8b6624063b364ba3779696e1e2ea67c35ae`. Application growth is limited to 79% of each OTA application slot, leaving at least 21% free. Managed filesystem writes retain the same 21% guard. The guard must not be lowered to make a build pass.

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before 21% guard |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,733,680 | 6,291,456 | 56.55% | 2,236,570 |
| M5Stack StopWatch | 16 MiB | 2,902,176 | 4,194,304 | 30.81% | 411,324 |
| Spotpear | 16 MiB | 2,622,688 | 6,291,456 | 58.31% | 2,347,562 |
| Seeed XIAO Round | 8 MiB | 2,441,328 | 3,145,728 | 22.39% | 43,797 |
| Waveshare Lite | 16 MiB | 1,769,920 | 6,291,456 | 71.87% | 3,200,330 |

The separate OTA slot is reserved for update recovery. Filesystem allocations are available for user files, not occupied application bytes. Exact partition budgets ship in each `memory-budget.json`. A larger flash chip does not increase internal DMA/radio RAM, processor throughput, radio bands or simultaneous channels.

Lossless theme assets decode once into PSRAM. All 135 variants retain their native indexed pixels and pass decoder CRC checks. The full four boards share the AMOLED application workflows with different capture/storage limits; Lite retains five compact pages because it has only 2 MiB PSRAM and one button. See the [feature matrix](FEATURE-MATRIX.md).

This release updates and checks the connected AMOLED against its exact public binary. StopWatch remains on its already-correct 0.13.9 image; the other 0.13.11 profiles are build/package-tested. Lite hardware checks remain stopped. See [validation](VALIDATION.md).
