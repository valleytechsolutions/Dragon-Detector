# Flash budget

From 0.13.14, application slots and managed filesystem writes retain 5% free. Packaging checks application size; runtime checks guard filesystem writes. Existing partitions and saved data are unchanged.

Measured from the clean v0.13.15 release builds:

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before limit |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,811,648 | 6,291,456 | 55.31% | 3,165,235 |
| M5Stack StopWatch | 16 MiB | 2,980,352 | 4,194,304 | 28.94% | 1,004,236 |
| Spotpear | 16 MiB | 2,676,448 | 6,291,456 | 57.46% | 3,300,435 |
| Seeed XIAO Round | 8 MiB | 2,474,432 | 3,145,728 | 21.34% | 514,009 |
| Waveshare Lite | 16 MiB | 1,802,512 | 6,291,456 | 71.35% | 4,174,371 |

The separate OTA slot supports update recovery. Filesystem allocations hold user files; their usage depends on the device. More flash does not increase internal RAM, radio capacity or processor throughput. Each package includes its exact `memory-budget.json`.

The four full boards share the application workflows with hardware-specific capture and storage limits. Lite uses five compact pages, 2 MiB PSRAM and one button. [Feature matrix](FEATURE-MATRIX.md) · [Validation](VALIDATION.md).
