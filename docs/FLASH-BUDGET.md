# Flash budget

From 0.13.14, application slots and managed filesystem writes retain 5% free. Packaging checks application size; runtime checks guard filesystem writes. Existing partitions and saved data are unchanged.

Measured from the clean v0.13.17 release builds:

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before limit |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,834,144 | 6,291,456 | 54.95% | 3,142,739 |
| M5Stack StopWatch | 16 MiB | 3,002,784 | 4,194,304 | 28.41% | 981,804 |
| Spotpear | 16 MiB | 2,695,152 | 6,291,456 | 57.16% | 3,281,731 |
| Seeed XIAO Round | 8 MiB | 2,490,688 | 3,145,728 | 20.82% | 497,753 |
| Waveshare Lite | 16 MiB | 1,844,000 | 6,291,456 | 70.69% | 4,132,883 |

The separate OTA slot supports update recovery. Filesystem allocations hold user files; their usage depends on the device. More flash does not increase internal RAM, radio capacity or processor throughput. Each package includes its exact `memory-budget.json`.

The four full boards share the application workflows with hardware-specific capture and storage limits. Lite uses five compact pages, 2 MiB PSRAM and one button. [Feature matrix](FEATURE-MATRIX.md) · [Validation](VALIDATION.md).
