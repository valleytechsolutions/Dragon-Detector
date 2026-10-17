# Flash budget

From 0.13.14, application slots and managed filesystem writes retain 5% free. Packaging checks application size; runtime checks guard filesystem writes. Existing partitions and saved data are unchanged.

Measured from the clean v0.13.14 release builds:

| Board | Physical flash | Application bytes | OTA slot bytes | Slot free | Further application bytes before limit |
|---|---:|---:|---:|---:|---:|
| Waveshare AMOLED | 32 MiB | 2,794,304 | 6,291,456 | 55.59% | 3,182,579 |
| M5Stack StopWatch | 16 MiB | 2,962,912 | 4,194,304 | 29.36% | 1,021,676 |
| Spotpear | 16 MiB | 2,663,600 | 6,291,456 | 57.66% | 3,313,283 |
| Seeed XIAO Round | 8 MiB | 2,464,688 | 3,145,728 | 21.65% | 523,753 |
| Waveshare Lite | 16 MiB | 1,793,056 | 6,291,456 | 71.50% | 4,183,827 |

The separate OTA slot supports update recovery. Filesystem allocations hold user files; their usage depends on the device. More flash does not increase internal RAM, radio capacity or processor throughput. Each package includes its exact `memory-budget.json`.

The four full boards share the application workflows with hardware-specific capture and storage limits. Lite uses five compact pages, 2 MiB PSRAM and one button. [Feature matrix](FEATURE-MATRIX.md) · [Validation](VALIDATION.md).
