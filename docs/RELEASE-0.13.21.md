# Dragon Detector 0.13.21 — Elecrow rotary port and AMOLED SD/GPS beta

This beta extends the owner-tested **16 MB non-C Waveshare
ESP32-S3-Touch-AMOLED-1.75** baseline. It adds a separate build for the
**1.75-G with onboard LC76G GPS**. The new SD, GPS and touch behavior has not
been tested on physical hardware. The existing 1.75C is a different, 32 MB board.

## Elecrow CrowPanel 2.1 Rotary

Adds a separate 480 × 480 IPS profile with touch, rotary navigation and press-to-select. It includes the shared themes, watch faces and timers, USB clock setup and internal storage, with no SD controls. This board builds ESP-IDF from source so RGB display buffers remain accessible during flash writes. It uses complete frame buffers and paced ring navigation. Startup and passive scan tests passed on hardware; extended display validation is ongoing. See the [Elecrow setup guide](ELECROW-ROTARY-21.md).

Existing board downloads retain their separately listed versions.

## Device interaction

- Tap a radar dot to select it and show its color-coded device family and RSSI.
- Double-tap the same device within 450 ms to open Device Details. A slow second
  tap keeps the summary. Tapping the summary also opens details.
- Details includes Hunt and controls. BLE contacts additionally offer BLE Toolkit
  and GATT Clone Tools for discovery, saving a profile and explicit emulation.
  Profiles contain supported GATT services and values, not protected keys or
  complete proprietary device behavior.
- Selection uses device identity, not a reusable cache index. Expired contacts
  and a busy radio cannot silently substitute a different BLE target.
- Axon retains its primary tag; other device colors and quiet tracker dots remain.

## SD storage

A FAT32 card needs **no downloaded boot pack**. Missing data folders are created
automatically. Core firmware still boots without a card.
The selected-board flasher panel links SD setup and GPS instructions beside the
install button. The panel explicitly states that no SD download is required.

- On non-C 1.75 boards, mounted SD becomes the preferred location for BLE profiles,
  BLE events, Wi-Fi surveys, recon missions and captures, under `/dragon`.
- Wi-Fi Radiotap PCAP and raw BLE CSV use a 64 KiB PSRAM queue, complete-record
  writes and 32 MiB segments. Stop drains the queue and finalizes files.
- Write errors, card loss and reserve exhaustion stop recording and report drops.
  Completed segments remain; unfinished `.part` files are retained for recovery.
  Power loss can still lose buffered data. Stop and wait for saving before removal.
- BLE event logs rotate at 16 MiB each. SD recon sessions allow up to eight hours
  or 64 MiB, with up to 512 session/survey slots.
- Wi-Fi inventory expands to 128 APs and 256 clients on these two profiles.
  Networks offers Save Wi-Fi survey; saved scans remain accessible over USB.
- File replacement supports FAT32 without overwriting an existing rename target,
  including mission status updates and recovery from an interrupted replacement.
- Without SD at boot, existing internal storage and bounded RAM captures remain.
  Insert the card before boot; hot remount and automatic migration of older
  internal profiles are not implemented.

## GPS wardriving

Flash **waveshare_175_gps** only onto the matching LC76G-equipped 1.75-G board.
The ordinary **waveshare_175** build remains available for non-GPS hardware.

Wardrive mode exclusively alternates passive Wi-Fi channels 1–11 and BLE
advertisement reception. Other radio tools must stop first. Detection analysis
and alerts are bypassed; Stop finalizes SD data and restores the prior radio
configuration. There is no automatic recording or upload.

Output is WiGLE 1.6 UTF-8 CSV in `/dragon/wardrive`. Rows require a fresh,
checksum-checked GNSS fix with matching RMC/GGA time and valid satellite fix
quality. Missing/stale fixes and delayed observations are counted and skipped.
BLE frequency and metric accuracy are reported as unknown, not invented.

The LC76G adapter uses the existing shared I²C bus, cooperative waits, bounded
FIFO reads and error backoff. It preserves the base display, touch, RTC and SD
wiring. Actual module FIFO behavior, shared-bus stability, route accuracy and
WiGLE service import still require field validation.

## Verification

Native checks cover GNSS parsing, CSV escaping, queue pressure, production SD
writer fault injection/32 MiB rollover, production GPS transport simulation,
FAT32 replacement recovery, tap identity/timing, board pin isolation and RTC
failure handling. Existing detector and protocol regression fixtures are included.
The browser suite passes 103 tests, including larger capture downloads and GPS
manifest identity checks.

Release packaging records the exact source commit, verifies merged-image offsets
and SHA-256 hashes, and keeps fresh-install and update manifests separate.
See the [validation notes](VALIDATION-0.13.21.md) and accompanying artifact
validation file for final build results.

This is an extension of the existing tool suite, not complete parity with
Marauder or ChimeraBLE. Microphone recording and IMU features remain unimplemented.

See the [board guide and field checklist](WAVESHARE-AMOLED-175.md) and
[upstream references](../CREDITS.md#amoled-sdgps-workflow-references--01321-beta).
