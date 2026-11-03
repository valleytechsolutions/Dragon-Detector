# Dragon Detector 0.14.1

Adds a dedicated **Waveshare ESP32-S3-Touch-LCD-1.85B** profile with a 360 px touch interface, SD-preferred storage and optional external UART GPS. The existing detector, theme, clock, badge, locksport and radio workflows remain shared with compatible full profiles.

External GPS starts disabled. On the 1.85B, the Wardrive menu appears only after the receiver is enabled and recent checksum-valid NMEA is received. Losing receiver data during a run does not remove the control needed to stop and save it. The connector supplies 3.3 V and uses SH1.0, so a standard Grove GPS cable is not automatically compatible.

The web flasher includes exact board identification, a dedicated installer, checked RTC synchronization, SD setup guidance and the [purchase link and wiring guide](WAVESHARE-LCD-185B.md). The build matrix covers twelve ESP32 profiles. This release does not update M5Burner or Gitea.

## Validation

Bench checks on November 3, 2026 used one physical 1.85B with 16 MB flash, 8 MB PSRAM and ST77916 panel revision 2. Its complete factory flash was backed up before installation. The owner confirmed that touch works great and BOOT responds. Display transfers and touch polling reported no bus errors during the initial checks.

The inserted FAT32 card mounted on the four-bit SDMMC interface. The 54,282-entry optional vendor pack (1,270,072 bytes) was installed and its SD readback matched SHA-256. A Wi-Fi capture saved 198 complete radiotap records (72,647 bytes); a BLE capture saved 283 complete CSV records (26,419 bytes). Both files were read back and parsed, with zero SD writer drops. Radio queue drops observed during USB transfers are not claimed as a lossless RF capture result.

The PCF85063 accepted the current UTC time, passed direct readback and retained a ticking clock across a firmware restart. Full hardware-off retention is a separate battery-powered acceptance check. The BQ27220 reported a connected battery. Speaker initialization and a low-volume test command succeeded; audio quality still needs explicit listening confirmation.

Native tests cover the production RTC and touch fault paths, stale input cancellation after USB stalls, optional GPS configuration/presence/menu behavior, cold-start GSV reception, strict located records, SD failures and asynchronous Wi-Fi/BLE scan ownership. Existing detector and browser regressions pass locally. The twelve-board CI and package checks must pass before rollout.

External GPS acquisition, moving wardrive routes, extended battery operation and live authenticated uploads remain separate checks. Firmware tests and the hardware's published specifications are not a claim that those physical tests have passed.
