# Dragon Detector 0.14.4 · Waveshare AMOLED 1.75C controls

This release updates the **32 MB Waveshare ESP32-S3-Touch-AMOLED-1.75C**.
The web flasher continues to provide the existing firmware for all other boards,
including the separate 16 MB AMOLED 1.75 and 1.75-G profiles.

## Side buttons

The larger **PWR** button now cycles through **Radar → Tools → Badge → Clock → Radar**
with one short press per screen. Navigation follows the current screen, including
after using the touchscreen. The smaller **BOOT** button opens Tools outside the
stopwatch. Confirmation dialogs retain control until they are closed.

In **Tools → Utilities → Stopwatch**:

| Button | Action |
| --- | --- |
| Larger PWR | Start or pause |
| Smaller BOOT | Record a lap while running; request reset while paused |

For locksport themes, the smaller button records **OPEN** while the timer is
running. A held BOOT button does not repeat actions, and a button held during
startup must be released before it can trigger a control.

## Installation

Choose **Waveshare ESP32-S3-Touch-AMOLED-1.75C** in the
[web flasher](https://valleytech-dragon-detector.pages.dev/#install).
Use the update option to preserve saved settings and files. The partition layout
is unchanged. Keep automatic USB clock setup enabled to set the current date,
time and time-zone offset after installation.

## Validation

On November 8, 2026, the 0.14.4 application was installed on a 1.75C with write-hash
verification. Its 23 startup self-tests passed, the display and touch controller
initialized, and saved history and internal storage remained available. The owner
confirmed the complete four-screen cycle and both stopwatch button assignments
on the device.

Host tests cover navigation after touch input, pending transitions, confirmation
dialogs, stopwatch and locksport actions, BOOT debounce and startup holds, and
the unchanged non-C button mapping. The Waveshare 1.85B side-key regression tests,
AMOLED hardware-profile checks and firmware build also passed. Other boards were
not reflashed or physically retested for this release. All 171 browser tests passed.

[Clock and timer controls](STOPWATCH-CLOCK-ORIENTATION.md) ·
[0.14.2 release notes for the other stable profiles](RELEASE-0.14.2.md)
