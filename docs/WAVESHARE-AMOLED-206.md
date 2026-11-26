# Waveshare ESP32-S3-Touch-AMOLED-2.06

Dragon Detector **0.14.9** has a dedicated `waveshare_206` build for this board.
This is an initial port built ahead of hardware arrival. Physical validation
is pending.

## Hardware and application

The board has a 410 × 502 CO5300 AMOLED, FT3168 touch, 32 MB flash, 8 MB octal
PSRAM, AXP2101 power management, PCF85063 RTC, ES8311 speaker output and a
microSD slot. The display runs at native resolution. The radar field, clock chapter, menus, node screens and badge canvas use the
full rectangle, with no circular display mask. Portrait and landscape layouts
use the rotated display bounds. Theme colors are retained; analog and digital
clock faces use a rectangular composition instead of the round-board dials.
Badge Studio produces native 410 × 502 images for this profile.
Theme Studio remains for the existing round profiles; editing and installing
personal themes for rectangular displays is deferred.

The full 0.14.9 application includes radar detection, themes, personal badge,
clocks, stopwatch and locksport timing, radio tools, BLE Lab and node modes.
MicroSD holds supported logs, captures and profiles. Internal storage is
19.94 MiB, with two 6 MiB application slots. A missing card leaves internal
storage available. The optional SD vendor lookup pack follows the existing
SD resource workflow.

BOOT opens the menu, or acts as the primary timer button. A short PWR press
acts as the secondary key. Holding PWR for approximately six seconds invokes
the board's hardware power-off circuit. The RTC needs battery-backed power to
retain time through shutdown. USB and saved-Wi-Fi clock setup are available.

This board has no integrated GPS. Dragon Detector 0.14.9 does not provide
applications for the ES7210 microphones or QMI8658 motion sensor.

## Install

Choose **Waveshare AMOLED 2.06** in the web flasher. Use **Fresh installation**
for the Waveshare factory demo or firmware from another board profile. This
replaces the ESP32 flash contents, including saved files. Preserve a factory
backup before the first installation if factory restoration is required.

The package contains `factory-0x0.bin`, which flashes at address `0x0`, and
split images with `factory.json` and `update.json`. Update applies only to this
exact Dragon Detector board profile and preserves user files and settings.

Hold BOOT while connecting USB to enter download mode if normal USB detection
fails. Use the native USB port and a data cable.

## Checks when the board arrives

1. Install the factory package and confirm `badge info` reports this board and
   version `0.14.9`. Confirm the reported flash size is 32 MB and PSRAM is usable.
2. Check all display edges, RGB colors, brightness, every touch corner, drags,
   taps, release behavior and all four orientations. Open menus, keyboard,
   radar, watch faces, timer, badge, BLE Lab and node screens.
3. Check BOOT and PWR actions, timer start/pause/lap, screen wake and hardware
   power-off. Read `display hardware`, `clock hardware` and `clock diagnostics`.
4. Sync the clock, disconnect USB with a charged battery connected, shut down
   and restart. Confirm the retained date and time.
5. Check battery telemetry, charging, speaker alerts at low volume, and a
   FAT32 microSD card. Save and reopen captures and profiles, then confirm
   internal storage works after restarting without a card.
6. Check Wi-Fi and BLE detection against known devices, configured network
   tools on the conference demonstration network, and sustained scanning.
7. Apply an update through the web flasher and confirm saved settings and
   files survive. Run an extended battery-powered demonstration before the
   conference.

## Build and reference

Run `python tools/build_release.py --board waveshare_206`. The firmware version
remains 0.14.9. Pin mapping, the column offset of 22 and initialization follow
the [Waveshare documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.06)
and [manufacturer source](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/3de8684cc3b27a8b6aa920157ecf399287528181).
