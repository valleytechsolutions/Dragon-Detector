# Dragon Detector 0.13.19

Adds an **experimental Waveshare ESP32-S3-Touch-AMOLED-1.75** profile for the 16 MB non-C board. Physical hardware testing is pending.

- Correct GPIO39 display reset, GPIO40 touch reset, GPIO42 audio MCLK and 1-bit microSD wiring.
- Full 466 × 466 interface, all themes including Can Commander, personal themes, timers and shared radio tools.
- PCF85063 UTC clock support with startup restoration, checked writes and post-flash readback/ticking verification.
- Separate factory/update manifests and a 16 MB layout, with 7.94 MiB shared internal file storage.
- A distinct experimental selection in the web flasher, with exact board identity checks and first-install instructions.

The five existing board profiles retain their hardware configuration and features. The new profile is not interchangeable with the 32 MB **1.75C**. See [port status, installation and arrival checks](WAVESHARE-AMOLED-175.md).
