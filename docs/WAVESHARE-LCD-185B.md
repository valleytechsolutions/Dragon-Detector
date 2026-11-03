# Waveshare ESP32-S3-Touch-LCD-1.85B

Choose **Waveshare LCD 1.85B** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install). The build profile is `waveshare_185b`, first available in **0.14.1**. Match the complete **ESP32-S3-Touch-LCD-1.85B** name: the 1.85, 1.85C, AMOLED 1.75 variants and similarly sized Spotpear boards use different wiring.

[Buy the exact 1.85B model](https://www.waveshare.com/esp32-s3-touch-lcd-1.85b.htm?&aff_id=122099) · [Waveshare hardware documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B) · [Initial port validation](RELEASE-0.14.1.md) · [Sound update](RELEASE-0.14.2.md)

## Hardware and installation

This profile provides the full 360 × 360 touch interface, radar, themes, badge, analog clocks, stopwatch/locksport timers, radio tools and BLE Lab. Its ESP32-S3 has 16 MB flash and 8 MB PSRAM. The board also has a PCF85063 RTC, speaker codec and an internal microSD slot.

Back up the factory firmware before the first installation. Choose **Fresh installation** for the Waveshare demo or another firmware. **Update** is for this exact Dragon Detector 1.85B profile, 0.14.1 or newer; it preserves NVS and internal files. It does not migrate files from another board profile.

The web flasher sets local time and verifies the RTC directly after installation. Keep USB connected until that check finishes. Direct RTC readback on the bench unit confirmed that its clock continued across a battery-powered PWR off/on cycle. RTC retention while the main board is off requires a connected, charged battery. A USB-only board cannot retain time after every power source is removed; reconnect for clock synchronization or use configured Wi-Fi recovery.

The two side buttons have different roles. A short **BOOT** press cycles **Radar → Main menu → Utilities → Badge → Clock → Radar**. Hold BOOT for about 0.7 seconds to return to the main menu. On the stopwatch or locksport timer, a single click starts or pauses timing; a double click performs the timer's Lap/Finish action while running or Reset while stopped. Timer singles wait about 0.25 seconds for a possible double click, but use the original press timestamp so that delay does not change the recorded time. Touchscreen controls remain available. **PWR** is controlled by the board's power circuit and is not a second programmable timing button.

To switch off on battery power, disconnect USB and hold **PWR** for about two seconds. Hold it again for about one to two seconds to turn on. While USB is connected, USB power keeps the board running even after PWR requests shutdown; unplugging USB then allows that shutdown to take effect. This is the board's documented power-circuit behavior, explained in [Waveshare's power-button FAQ](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/FAQ#other-questions). Stop recording and wait for the save result before switching off or unplugging USB.

## SD card and resources

Insert a FAT32 microSD card before boot. Firmware mounts it without formatting it and creates the required folders. Existing files remain on the card. Firmware and core radar detections run without a resource pack; the 1.85B stores its general manufacturer and BLE lookup names on SD to reduce the application image.

Supported captures, BLE profiles, event logs, surveys and recon sessions prefer SD when it mounts successfully. The card uses a separate four-bit SDMMC bus, so its transfers do not share the display's QSPI transport. Saved recon data lives below `/dragon`; wardrive CSV files live in `/dragon/wardrive`. The file browser and USB tools can read saved files.

Download **`dragon-resources-v1-20261103.zip`**, the 1.85B [SD lookup resource pack](SD-RESOURCES.md). Extract its `dragon` directory to the card root, preserving other files. Both files must be present:

- `/dragon/resources/vendors.ddr` — IEEE manufacturer assignments and names.
- `/dragon/resources/ble-labels.ddb` — Bluetooth service, characteristic and company lookup names.

Insert the prepared card before boot and **restart after installing or replacing either file**. The firmware validates and loads the lookup data into PSRAM at startup. Copying a ZIP onto the card does not install its contents. Use the pack's SHA-256 listing to verify the download.

These files are required for lookup names on this profile. If a file is absent, invalid or cannot be loaded, its general lookup names remain unresolved; addresses, numeric identifiers and core radar detections still work. This profile has no internal fallback copy of those label databases. Other SD board profiles retain their existing optional vendor pack and internal fallback.

Open **SYSTEM → SD resources** to see whether the lookup pack is ready, missing, invalid or unavailable. Tap the status for the individual file results and installation instructions. Resource problems do not interrupt the radar, badge or clock with automatic popups.

SD saves reduce consumption of writable internal storage, while this profile's SD lookup databases also remove their built-in label copies from the application image. Application code, numeric detector signatures and built-in theme assets remain in flash so the device still starts and detects without a card. This profile reserves 4 MiB for each OTA application slot and 7.94 MiB for internal files; the package's `memory-budget.json` reports the measured application size. Additional files copied to the card do not reduce firmware size unless the firmware explicitly loads them there.

Finish an active recording before removing power or the card. If a card is absent, unreadable or full, check the displayed storage status rather than assuming a recording was saved. Firmware does not automatically erase or format a card.

For deliberate USB maintenance, `sd status` reports the inserted card's identity and capacity. The explicit `sd format <id> <card_bytes> ERASE` command erases that card after checking that timing, logging, recording and radio tools have stopped. It formats once as FAT32 with 32 KiB clusters and two FATs, restarts, and checks the same card's fresh mount before reporting completion. A changed or missing card cancels the request. Reinstall the resource pack afterwards. This command does not format computer drives or run during ordinary boot.

## Optional external UART GPS

GPS is **off by default**. This board does not include a GPS receiver. The four-pin **J8 UART connector is SH1.0**, not a standard Grove socket; use a correctly pinned adapter. The separate I²C connector is not the GPS connection.

The verified revision 1.1 schematic gives this pinout:

| J8 pin / board signal | Connect to GPS |
|---|---|
| 1 — RXD, GPIO44 | GPS TX output |
| 2 — TXD, GPIO43 | GPS RX input, optional for receive-only NMEA |
| 3 — 3V3 | GPS power only if the receiver accepts a 3.3 V supply |
| 4 — GND | GPS ground |

Use **3.3 V UART logic**. J8 supplies **3.3 V, not 5 V**. A receiver that requires 5 V power needs its own appropriate regulated supply and a common ground; never feed 5 V into the board's 3V3 or GPIO pins. Follow the connector pin numbers and PCB marking, not an assumed cable color or plug orientation.

After connecting a compatible NMEA receiver, open **SYSTEM → GPS setup**, choose its baud rate and enable GPS. The GPS setting is saved. Start with the receiver's documented baud rate; readable text alone is not proof of a valid GPS stream.

The **Wardrive** menu stays hidden until GPS is enabled and the board is receiving recent, checksum-valid recognized NMEA sentences. A satellite fix is not needed just to reveal the menu. Located records still require a valid fix. If a receiver stops responding during a run, the active Wardrive screen remains available so **End & return to menu** can finalize the files. Disable GPS to return to ordinary operation without the Wardrive entry.

Wardriving dedicates the radio workflow to Wi-Fi and BLE collection. Logs use the existing WiGLE CSV format. [Wardrive format and upload setup](WARDRIVE-UPLOADS.md) describes the explicit SD-configured upload workflow; successful live service upload and moving-route accuracy require their own checks.

## Sound levels

In 0.14.2, open **Settings → Sound level** or **Audio & hunt tones** and choose
**High, Medium or Silent**. Each tap on the device cycles Silent → Medium → High
→ Silent. The web audio editor offers the same three choices and applies them
only when **Save audio settings** is pressed. Medium uses application gain 50;
High uses 100. Fresh installations start at Medium. Silent disables audio and sets gain to zero, including preview and
hunt sounds. Quiet mode remains separate and mutes automatic feedback.

The 1.85B's ES8311 codec uses unity gain from 0.14.2. The owner approved the
Medium and High test beeps on November 3, 2026. Medium remained saved after
restart, while Silent set zero gain and rejected a preview request. The initial silent state had
alerts disabled with saved gain 10; the subsequent low-output report was resolved
by the 1.85B-specific codec gain adjustment. Existing saved gains remain intact
until a level is explicitly selected. [Audio controls and USB commands](USER-GUIDE.md#audio-alerts-and-foxhunting).

## Port reference

| Peripheral | Connection |
|---|---|
| ST77916 LCD | QSPI CS21, CLK40, D0 46, D1 45, D2 42, D3 41; reset3; backlight5 |
| CST816S touch | Internal I²C SDA11/SCL10, address0x15; reset1, interrupt4 |
| PCF85063 RTC | Internal I²C address0x51; interrupt6 |
| BQ27220 gauge | Internal I²C address0x55 |
| ES8311 speaker codec | Internal I²C address0x18; MCLK2, BCLK48, LRCK38, DOUT47; amplifier9 |
| microSD | CLK15, CMD14, D0 16, D1 17, D2 12, D3 13 |
| BOOT / external GPS | BOOT0; UART RX44/TX43 |

LCD initialization handles both panel IDs documented in the vendor BSP. The hardware reference is the [Waveshare repository at revision 7232af4](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/tree/7232af4c501c64d34215b73e1d8319376dfd9c19). Peripheral wiring and release builds do not prove receiver acquisition, battery retention or field performance; the [release validation record](RELEASE-0.14.1.md) separates those checks.
