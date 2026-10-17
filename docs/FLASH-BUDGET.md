# Flash budget · 0.13.10

Published clean-CI measurements from `b4e484b0a91ae90ab253c027b65075c2d8b5c526`. Application growth is limited to 79% of each OTA application slot, leaving at least 21% free. Managed filesystem writes retain the same 21% guard. The guard must not be lowered to make a build pass.

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before 21% guard |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,732,384 | 6,291,456 | 56.57% | 2,237,866 |
| M5Stack StopWatch | 16 MiB | 2,900,896 | 4,194,304 | 30.84% | 412,604 |
| Spotpear | 16 MiB | 2,621,488 | 6,291,456 | 58.33% | 2,348,762 |
| Seeed XIAO Round | 8 MiB | 2,440,048 | 3,145,728 | 22.43% | 45,077 |
| Waveshare Lite | 16 MiB | 1,769,904 | 6,291,456 | 71.87% | 3,200,346 |

The separate OTA slot is reserved for update recovery. Filesystem allocations are available for user files, not occupied application bytes. Exact partition budgets ship in each `memory-budget.json`. A larger flash chip does not increase internal DMA/radio RAM, processor throughput, radio bands or simultaneous channels.

Lossless theme assets decode once into PSRAM. All 135 variants retain their native indexed pixels and pass decoder CRC checks. The full four boards share the AMOLED application workflows with different capture/storage limits; Lite retains five compact pages because it has only 2 MiB PSRAM and one button. See the [feature matrix](FEATURE-MATRIX.md).

This release updates and checks the connected AMOLED against its exact public binary. StopWatch remains on its already-correct 0.13.9 image; the other 0.13.10 profiles are build/package-tested. Lite hardware checks remain stopped. See [validation](VALIDATION.md).
