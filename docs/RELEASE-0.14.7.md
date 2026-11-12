# Dragon Detector 0.14.7 · Waveshare AMOLED 1.43C

Version 0.14.7 adds a profile for the **Waveshare ESP32-S3-Touch-AMOLED-1.43C**,
the 8 MB round 466 × 466 AMOLED board. It runs the full touch interface: radar,
themes, badge, clocks, stopwatch and locksport timers, radio tools and BLE Lab.

Choose **Waveshare AMOLED 1.43C** in the
[web flasher](https://valleytech-dragon-detector.pages.dev/#install) and use
**Fresh installation** the first time. Match the complete model name, including
the **C**: the plain 1.43 and 1.43-B are different boards and this profile does
not support them. Other board downloads are unchanged.

## What this board gets

- **Buttons.** BOOT is the only button the firmware can read, so it navigates.
  A short press cycles **Radar → Main menu → Utilities → Badge → Clock → Radar**,
  and a hold of about 0.7 seconds opens the main menu. On the stopwatch or
  locksport timer, one click starts or pauses and a double click records
  Lap/Finish while running or asks to reset while stopped. With no clock chip,
  the board shows **SET YOUR CLOCK** after a power-on that has no time yet, and
  a BOOT press answers **Later**. PWR stays the board's hardware power button.
- **Touch.** Quick taps count while the radar redraws, and quick swipes scroll.
  A tap made while a new page, alert or dialog is still being drawn, or on a
  dark screen, is dropped instead of landing on what replaced it. A quick
  second tap on a setting that redraws its page may therefore need repeating.
- **Storage.** One 4 MiB application slot and 3.94 MiB of internal storage.
  Captures are limited to 1 MiB and recon sessions to 256 KiB. There is no SD slot.
- **Clock.** The board has no clock chip. The flasher sets the time over USB;
  after power-off, use USB synchronization or configured saved-Wi-Fi recovery.
- **Battery.** A percentage estimated from the cell voltage, with a charge
  symbol while charging. A board with no cell connected reads as full on USB.
- **Sound.** The ES8311 codec and amplifier drive the speaker connector. A
  speaker must be plugged in; without one, alerts are visual.

The board has little free internal memory once Wi-Fi, Bluetooth and the display
are running. BOOT is therefore read from the existing system timer instead of a
task of its own, touch is read between display transfers, the display buffers
hold 32 rows instead of the 40 of the 1.75 boards, and Wi-Fi joins use the same
start-up order as the 1.85B. A second Wi-Fi join in one session and the lab
portal are then not refused for lack of memory.

[Setup, buttons and wiring](WAVESHARE-AMOLED-143C.md)

## Validation

Host tests cover the pin map, the single-slot 8 MB partition table, the
shared-code gates that select the 466 px fonts and clock digits, touch
mirroring, quick taps and swipes between two screen updates, taps dropped
when the picture changed under them or the screen was dark, BOOT debounce and click
routing, the clock prompt, the charger filter, the battery curve and the
display buffer size. All 177 browser tests passed; the new ones
cover board identification, the 8 MB size limits and the installer manifests.
The build matrix covers thirteen ESP32 profiles. The full firmware builds for
this profile with about 0.9 MiB of its application slot free.

On November 9 and 10, 2026, four pre-release images were installed on one
1.43C, each with write-hash verification, after a full backup of the factory
firmware. The last of them is older than this release's source: it read touch
once per screen update, took every queued BOOT step in one pass and did not
answer the clock prompt with BOOT. On it the board reported over USB:

- **Start-up.** 8 MB flash and 8 MB PSRAM, one start with no crash or
  unexpected reset, the CO5300 display, CST820 touch controller and ES8311
  codec initialized, 3.94 MiB of storage mounted, and all 23 detection
  self-tests passed.
- **Clock.** Set over USB and read back ten times. No clock chip was detected,
  as expected.
- **Memory.** 52 KB of internal memory free at idle and 46 KB after two Wi-Fi
  join attempts to a network name that does not exist. Neither attempt was
  refused for memory, and the lab portal started and stopped afterwards.
- **Display.** About ten thousand display transfers completed. Four screens
  were captured from the firmware's drawing path with matching checksums and
  compared with the intended layout. That checks what the firmware drew, not
  what the panel showed.
- **Battery line.** 4.20 V with the charger active.
- **USB commands.** Six replies in that run arrived only when the next
  command was sent, more than four seconds later. The firmware kept running
  throughout.

Two earlier images each ran for about half an hour without a crash or reset.

Not yet confirmed by a person on the board: the picture as seen on the panel
(position, orientation and colours), touch input, BOOT presses, sound through a
connected speaker, the battery estimate with a cell connected, the charge
symbol and LED, PWR behaviour, a join to a real network with clock recovery,
the clock prompt after a power-off, and download mode by hand. Also not yet
run on this board: touch read between display transfers with quick taps
replayed, taps dropped while the picture changes or the screen is dark, one
BOOT step per pass, the BOOT answer on the clock prompt (these have passed
host tests only), recording, saving and downloading a
capture, and installation and Update through the web flasher with the clock
set that follows it. The test images were written with esptool as one 8 MB
file. A completed installation and start-up are not a claim that these work.

Other boards were not reflashed or physically retested for this release. The
thirteen-board CI and package checks must pass before rollout.

[Clock setup](CLOCK-SETUP.md) · [Installation and recovery](FLASHING.md)
