# Seeed SenseCAP Watcher — Dragon Detector beta

This profile targets the original ESP32-S3 SenseCAP Watcher with a 412 × 412 SPD2010 LCD, 32 MB flash and 8 MB PSRAM. Build environment: `watcher`. Firmware starts at 0.13.29. It is a distinct hardware profile; never install a Waveshare, XIAO or Elecrow image on it.

## Features and controls

The full Dragon Detector application includes Wi-Fi/BLE detection, signal hunting, themes, badges, clocks, stopwatch and locksport timers, radio tools, BLE Lab, saved surveys and captures. Watcher integration adds touch, rotary navigation, PCF8563 RTC, ES8311 speaker output, battery estimate, FAT32 microSD storage and an external AT6668 UART GPS path for passive wardriving. Turn to highlight controls, click to select and hold for about one second to open the menu. Touch remains available alongside the knob. Battery percentage is an approximate voltage-based estimate.

The camera, microphone recording, RGB LED and separate Himax AI processor are not enabled in this first port. The Himax firmware is left untouched. Hardware test results are recorded below; compiled features are not evidence of a successful physical test.

## Installation and recovery

The USB connector exposes two CH342 serial ports. Identify the ESP32-S3 port using esptool chip identification; the other port belongs to the AI processor. Port numbers differ between computers. Select **Seeed SenseCAP Watcher** in the flasher. Choose Fresh installation when replacing Seeed firmware; this changes the partition layout and saved ESP32 data. Update is only for this exact 0.13.29+ Watcher layout.

Back up all 32 MB of ESP32 flash before replacing factory firmware. Keep that dump private: it may contain account or network configuration. Verify it against the device and record its SHA-256. A dump of ESP32 flash does not back up Himax flash. Restore only to the same device using the saved image at offset 0, after verifying its checksum. No eFuse changes are needed.

The partition layout has two 6 MiB application slots and 19.94 MiB of LittleFS. SD is preferred for persistent recon data; without SD, supported recon operations can use internal storage. Wardriving requires SD. The combined factory BIN is 32 MB; split images are used by the browser installer.

## SD card and resources

Insert a FAT32 microSD before boot. The firmware creates its folders without formatting the card. Existing files remain in place. No boot artwork or mandatory asset download is needed: themes, fonts and the fallback vendor registry are built in.

The optional [SD vendor resource pack](SD-RESOURCES.md) expands manufacturer lookup. Extract `dragon/` from `dragon-resources-v1-20261023.zip` to the card root so the file is exactly `/dragon/resources/vendors.ddr`. Keep the supplied manifest with the pack. Restart after installation; the CRC-checked registry loads into PSRAM once. Missing, invalid or oversized resources fall back to the built-in registry. Do not remove the card during logging; stop sessions and wait for saved status first.

| Folder | Contents |
| --- | --- |
| `/dragon/resources` | Optional versioned manufacturer registry |
| `/dragon/captures` | PCAP and other capture files |
| `/dragon/wardrive` | WiGLE-compatible Wi-Fi/BLE CSV sessions |
| `/dragon/surveys` | Saved survey results |
| `/dragon/missions` | Session records |
| `/dragon/profiles` | Saved profiles |
| `/dragon/events` | Event records |
| `/music` | User audio files where supported |

## AT6668 GPS wiring

**Do not plug this UART GPS directly into the Watcher Grove socket.** Watcher Grove J5 pin 1 is GPIO48/SCL and pin 2 is GPIO47/SDA, shared with its power expander, RTC and audio codec; pin 3 is switched 3.3 V. Reassigning these signals to UART would break internal peripherals. The M5Stack unit expects 5 V power.

Use a Grove breakout from the **GPS unit**, separate wires to the Watcher expansion header J2, and a regulated 5 V source for the GPS. Power down before wiring. Verify the header pin numbering against Seeed's schematic and the board markings; the following numbers are schematic pin numbers, not a guessed view of the enclosure.

| GPS Unit v1.1 wire | Connect to | Purpose |
| --- | --- | --- |
| Yellow, Grove pin 1 | Watcher J2 pin 4 / GPIO19 | GPS TXD1 data → ESP32 UART RX |
| Black, Grove pin 4 | Watcher J2 pin 1 or 8 / GND, and 5 V supply ground | Common ground |
| Red, Grove pin 3 | Regulated external 5 V | GPS power |
| White, Grove pin 2 | Leave disconnected for initial tests | Optional GPS RXD1 input; may connect to J2 pin 5 / GPIO20 for future commands |

M5Stack's table labels yellow `UART_RX` and white `UART_TX` from the host perspective. Its linked schematic connects yellow pin 1 to the GPS module's TXD1 and white pin 2 to RXD1. The UART signals are on the module's 3.3 V domain; 5 V is only the supply input. Confirm the actual unit is U032-V11 before using these colors. For another AT6668 carrier, follow that carrier's schematic and module TX/RX signals.

Watcher J2 pin 7 `Exp_5V` is documented as an external **power input**; do not assume it provides 5 V to the GPS. Do not connect an unverified 5 V rail to GPIO19, GPIO20 or 3.3 V. A USB power breakout provides a convenient separate supply during initial testing, with grounds joined.

Firmware listens at 115200 baud, 8N1. Initial wiring is receive-only; no GPS reconfiguration is required. Place the antenna with an unobstructed sky view, open Wardrive and inspect fix age, satellites and HDOP. Logging requires a fresh valid position and GPS UTC; loss of fix skips observations instead of inventing coordinates. Stop to finalize CSV files before downloading. WiGLE/WDG uploading is not enabled: files stay local until explicitly exported.

## Diagnostics and physical validation

USB commands include `watcher status`, `clock hardware`, `clock diagnostics`, `wardrive status`, `resources status`, `memory status` and ordinary Detector status commands. The Watcher status reports display flushes, touch samples, rotary edges/clicks/holds, power-bus errors, SD and RTC presence.

Current status: firmware compilation and browser checks passed. The connected unit’s full 32 MB ESP32 factory image was read and verified against the device before flashing; its SHA-256 and restoration instructions are stored privately outside the repository. Display rendering, RTC retention across reset, SD mounting, the 54,282-entry vendor pack and a 579-packet Wi-Fi PCAP save (zero reported drops) were verified on the connected unit. Hold opens the menu, rotation moves focus and a short click selects; rotation alone does not open the menu. Touch and revised rotary behavior still require final hands-on confirmation. Longer capture sessions and battery calibration remain unverified. GPS wiring and field logging are deferred at the owner’s request. See the eventual hardware validation report before treating this as a verified release.

## Sources

- [Seeed Watcher schematic v1.0, sheet 10 for expansion/Grove](https://raw.githubusercontent.com/Seeed-Studio/OSHW-SenseCAP-Watcher/main/Hardware/SenseCAP_Watcher_v1.0_SCH.pdf)
- [Seeed Watcher firmware/BSP](https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware)
- [M5Stack Unit GPS v1.1 specifications and connector table](https://docs.m5stack.com/en/unit/Unit_GPS_v1.1)
- [M5Stack linked GPS schematic, TXD1/RXD1 connector nets](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/849/U032-V11-UNIT_GPS_SCHE.pdf)
