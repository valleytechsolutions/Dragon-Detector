# Dragon Detector 0.14.1

Adds a dedicated **Waveshare ESP32-S3-Touch-LCD-1.85B** profile with a 360 px touch interface, SD-preferred storage and optional external UART GPS. The existing detector, theme, clock, badge, locksport and radio workflows remain shared with compatible full profiles.

External GPS starts disabled. On the 1.85B, the Wardrive menu appears only after the receiver is enabled and recent checksum-valid NMEA is received. Losing receiver data during a run does not remove the control needed to stop and save it. The connector supplies 3.3 V and uses SH1.0, so a standard Grove GPS cable is not automatically compatible.

On the 1.85B, BOOT cycles Radar, the RECON menu, Utilities, Badge and Clock; holding it returns to RECON. Timer mode uses a single click for start/pause and a double click for Lap/Finish or Reset. The cycle follows touchscreen navigation and respects open prompts and pending saves.

The 1.85B also moves its general IEEE vendor and Bluetooth display-name databases to SD. Its `dragon-resources-v1-20261103.zip` pack is required for lookup names, while firmware and core radar detection still run without it. **SYSTEM → SD resources** displays ready, missing, invalid or unavailable and opens setup guidance when tapped. The older profiles retain their optional pack and internal registry. This removes the 462,273-byte compressed vendor payload and its duplicate 709,215-byte decoded allocation from the new board's application path. Including the new controls, status, capture scheduling and maintenance code, the measured application fell from 3,265,280 to 2,807,968 bytes, a net reduction of 457,312 bytes. The release memory budget records its final packaged size.

During an active capture on the 1.85B, radar animation updates every 250 ms to leave processing time between expensive redraws. Touch handling continues on every application loop. The SD writer can drain up to 32 small records within its existing three-millisecond write budget; other boards keep their existing behavior.

The web flasher includes exact board identification, a dedicated installer, checked RTC synchronization, SD setup guidance and the [purchase link and wiring guide](WAVESHARE-LCD-185B.md). The build matrix covers twelve ESP32 profiles. This release does not update M5Burner or Gitea.

## Validation

Bench checks on November 3, 2026 used one physical 1.85B with 16 MB flash, 8 MB PSRAM and ST77916 panel revision 2. Its complete factory flash was backed up before installation. The owner confirmed that touch works great, the BOOT page cycle and timer single/double clicks work, and the hardware PWR button operates on battery power with USB unplugged. USB keeping the board running after a power-button shutdown request matches the vendor's documented circuit behavior. Display transfers and touch polling reported no bus errors during the initial checks.

The inserted FAT32 card mounted on the four-bit SDMMC interface. During the initial bench checks, a Wi-Fi capture saved 198 complete radiotap records (72,647 bytes); a BLE capture saved 283 complete CSV records (26,419 bytes). Both files were read back and parsed, with zero SD writer drops. Radio queue drops observed during bench work are not claimed as a lossless RF capture result.

At the owner's request, the same card was then explicitly reformatted. A fresh boot verified its identity, FAT32 filesystem and 32 KiB clusters. The complete lookup pack was installed; the 54,282-entry vendor file, 44-entry BLE label file, manifest and reference notes passed SHA-256 readback. Both indexes reported ready after restart. During the interval before reinstalling the pack, live Wi-Fi and BLE reception continued with unresolved lookup names, as intended.

After provisioning, 369 Wi-Fi packets and 1,521 BLE records passed SD readback and parsing with zero SD writer drops. The BLE run exposed a receive backlog while the radar repeatedly redrew. Following the capture scheduling change, a longer run saved 5,311 complete BLE rows (494,021 bytes) spanning 111.192 seconds, with zero observed radio queue or SD writer drops. The queue high-water mark was 19 and maximum queue lag was 367 ms, compared with 72 and 2,255 ms in the earlier run. This is a bench result in one radio environment, not a guarantee of lossless reception in all environments.

The PCF85063 accepted the current UTC time, passed direct readback and retained a ticking clock across a firmware restart. A subsequent direct RTC readback confirmed clock progression across the owner's battery-powered PWR off/on cycle. The BQ27220 reported a connected battery. Speaker initialization and a low-volume test command succeeded; audio quality still needs explicit listening confirmation.

Native tests cover the production RTC and touch fault paths, stale input cancellation after USB stalls, optional GPS configuration/presence/menu behavior, cold-start GSV reception, strict located records, SD failures and maintenance, asynchronous Wi-Fi/BLE scan ownership, BOOT navigation and explicit resource-status dialogs. The typed BLE label loader and unchanged vendor file have host validation and verified device provisioning. All 152 browser tests pass locally. The twelve-board CI and package checks must pass before rollout.

External GPS acquisition, moving wardrive routes, extended battery operation and live authenticated uploads remain separate checks. Firmware tests and the hardware's published specifications are not a claim that those physical tests have passed.
